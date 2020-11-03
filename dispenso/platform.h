// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE.md file in the root directory of this source tree.

/**
 * @file platform constants and common utilities.
 **/
#pragma once
#include <atomic>
#include <thread>
#include <type_traits>

namespace dispenso {

#if _MSC_VER
using ssize_t = std::make_signed<std::size_t>::type;
#endif

/**
 * @var constexpr size_t kCacheLineSize
 * @brief A constant that defines a safe number of bytes+alignment to avoid false sharing.
 **/
constexpr size_t kCacheLineSize = 64;

/**
 * @def DISPENSO_THREAD_LOCAL
 * @brief A macro that can be used when declaring a lightweight thread-local variable.
 **/

// TODO(bbudge): Non-gcc/clang/msvc platforms.
#if defined(_MSC_VER)
#define DISPENSO_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define DISPENSO_THREAD_LOCAL __thread
#else
#error Supply lightweight thread-locals for this compiler.  Can define to thread_local if lightweight not available
#endif

namespace detail {

inline void* alignedMalloc(size_t bytes, size_t alignment) {
#if _POSIX_C_SOURCE >= 200112L || defined(__APPLE_CC__) || defined(__ANDROID__)
  void* ptr;
  if (::posix_memalign(&ptr, alignment, bytes)) {
    return nullptr;
  }
  return ptr;
#elif defined(_MSC_VER)
  return (_aligned_malloc(bytes, alignment));
#else
#error Need to provide alignedMalloc implementation for non-posix, non-windows system
#endif // _POSIX_C_SOURCE
}

inline void* alignedMalloc(size_t bytes) {
  return alignedMalloc(bytes, kCacheLineSize);
}

inline void alignedFree(void* ptr) {
#if _POSIX_C_SOURCE >= 200112L || defined(__APPLE_CC__) || defined(__ANDROID__)
  ::free(ptr);
#elif defined(_MSC_VER)
  _aligned_free(ptr);
#else
#error Need to provide alignedFree for non-posix system
#endif // _POSIX_C_SOURCE
}

inline constexpr uintptr_t alignToCacheLine(uintptr_t val) {
  constexpr uintptr_t kMask = kCacheLineSize - 1;
  val += kMask;
  val &= ~kMask;
  return val;
}

#if defined __x86_64__ || defined __i386__
inline void cpuRelax() {
  asm volatile("pause" ::: "memory");
}
#else
// TODO: provide reasonable relax on non-x86
inline void cpuRelax() {}
#endif // x86-arch

// When statically chunking a range, it is generally not possible to use a single chunk size plus
// remainder and get a good load distribution.  By estimating too high, we can have idle threads. By
// estimating too low, the remainder can be several times as large as the chunk for other threads.
// Instead, we compute the chunk size that is the ceil of the fractional chunk size.  That can be
// used for the first transitionIndex values, while the remaining (chunks - transitionTaskIndex)
// values will be ceilChunkSize - 1.
struct StaticChunking {
  size_t transitionTaskIndex;
  size_t ceilChunkSize;
};

inline StaticChunking staticChunkSize(size_t items, size_t chunks) {
  StaticChunking chunking;
  chunking.ceilChunkSize = (items + chunks - 1) / chunks;
  size_t numLeft = chunking.ceilChunkSize * chunks - items;
  chunking.transitionTaskIndex = chunks - numLeft;
  return chunking;
}

} // namespace detail
} // namespace dispenso
