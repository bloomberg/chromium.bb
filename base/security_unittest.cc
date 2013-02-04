// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <limits>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::nothrow;
using std::numeric_limits;

namespace {

// Check that we can not allocate a memory range that cannot be indexed
// via an int. This is used to mitigate vulnerabilities in libraries that use
// int instead of size_t.
// See crbug.com/169327.

// - NO_TCMALLOC because we only patched tcmalloc
// - ADDRESS_SANITIZER because it has its own memory allocator
// - IOS does not seem to honor nothrow in new properly
// - OS_MACOSX does not use tcmalloc
#if !defined(NO_TCMALLOC) && !defined(ADDRESS_SANITIZER) && \
    !defined(OS_IOS) && !defined(OS_MACOSX)
  #define ALLOC_TEST(function) function
#else
  #define ALLOC_TEST(function) DISABLED_##function
#endif

// TODO(jln): switch to std::numeric_limits<int>::max() when we switch to
// C++11.
const size_t kTooBigAllocSize = INT_MAX;

// Detect runtime TCMalloc bypasses.
bool IsTcMallocBypassed() {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // This should detect a TCMalloc bypass from Valgrind.
  char* g_slice = getenv("G_SLICE");
  if (g_slice && !strcmp(g_slice, "always-malloc"))
    return true;
#endif
  return false;
}

// Fake test that allow to know the state of TCMalloc by looking at bots.
TEST(SecurityTest, ALLOC_TEST(IsTCMallocDynamicallyBypassed)) {
  printf("Malloc is dynamically bypassed: %s\n",
         IsTcMallocBypassed() ? "yes." : "no.");
}

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsMalloc)) {
  if (!IsTcMallocBypassed()) {
    scoped_ptr<char, base::FreeDeleter>
        ptr(static_cast<char*>(malloc(kTooBigAllocSize)));
    ASSERT_TRUE(ptr == NULL);
  }
}

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsCalloc)) {
  if (!IsTcMallocBypassed()) {
    scoped_ptr<char, base::FreeDeleter>
        ptr(static_cast<char*>(calloc(kTooBigAllocSize, 1)));
    ASSERT_TRUE(ptr == NULL);
  }
}

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsRealloc)) {
  if (!IsTcMallocBypassed()) {
    char* orig_ptr = static_cast<char*>(malloc(1));
    ASSERT_TRUE(orig_ptr != NULL);
    scoped_ptr<char, base::FreeDeleter>
        ptr(static_cast<char*>(realloc(orig_ptr, kTooBigAllocSize)));
    ASSERT_TRUE(ptr == NULL);
    // If realloc() did not succeed, we need to free orig_ptr.
    free(orig_ptr);
  }
}

typedef struct {
  char large_array[kTooBigAllocSize];
} VeryLargeStruct;

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsNew)) {
  if (!IsTcMallocBypassed()) {
    scoped_ptr<VeryLargeStruct> ptr(new (nothrow) VeryLargeStruct);
    ASSERT_TRUE(ptr == NULL);
  }
}

TEST(SecurityTest, ALLOC_TEST(MemoryAllocationRestrictionsNewArray)) {
  if (!IsTcMallocBypassed()) {
    scoped_ptr<char[]> ptr(new (nothrow) char[kTooBigAllocSize]);
    ASSERT_TRUE(ptr == NULL);
  }
}

// The tests bellow check for overflows in new[] and calloc().

#if defined(OS_IOS) || defined(OS_WIN)
  #define DISABLE_ON_IOS_AND_WIN(function) DISABLED_##function
#else
  #define DISABLE_ON_IOS_AND_WIN(function) function
#endif

#if defined(ADDRESS_SANITIZER)
  #define DISABLE_ON_ASAN(function) DISABLED_##function
#else
  #define DISABLE_ON_ASAN(function) function
#endif

// There are platforms where these tests are known to fail. We would like to
// be able to easily check the status on the bots, but marking tests as
// FAILS_ is too clunky.
void OverflowTestsSoftExpectTrue(bool overflow_detected) {
  if (!overflow_detected) {
#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_MACOSX)
    // Sadly, on Linux, Android, and OSX we don't have a good story yet. Don't
    // fail the test, but report.
    printf("Platform has overflow: %s\n",
           !overflow_detected ? "yes." : "no.");
#else
    // Otherwise, fail the test. (Note: EXPECT are ok in subfunctions, ASSERT
    // aren't).
    EXPECT_TRUE(overflow_detected);
#endif
  }
}

// This function acts as a compiler optimization barrier. We use it to
// prevent the compiler from making an expression a compile-time constant.
// We also use it so that the compiler doesn't discard certain return values
// as something we don't need (see the comment with calloc below).
template <typename Type>
Type HideValueFromCompiler(volatile Type value) {
  return value;
}

// Test array[TooBig][X] and array[X][TooBig] allocations for int overflows.
// IOS doesn't honor nothrow, so disable the test there.
// Disable on Windows, we suspect some are failing because of it.
TEST(SecurityTest, DISABLE_ON_IOS_AND_WIN(NewOverflow)) {
  const size_t kArraySize = 4096;
  // We want something "dynamic" here, so that the compiler doesn't
  // immediately reject crazy arrays.
  const size_t kDynamicArraySize = HideValueFromCompiler(kArraySize);
  // numeric_limits are still not constexpr until we switch to C++11, so we
  // use an ugly cast.
  const size_t kMaxSizeT = ~static_cast<size_t>(0);
  ASSERT_EQ(numeric_limits<size_t>::max(), kMaxSizeT);
  const size_t kArraySize2 = kMaxSizeT / kArraySize + 10;
  const size_t kDynamicArraySize2 = HideValueFromCompiler(kArraySize2);
  {
    scoped_ptr<char[][kArraySize]> array_pointer(new (nothrow)
        char[kDynamicArraySize2][kArraySize]);
    OverflowTestsSoftExpectTrue(array_pointer == NULL);
  }
  {
    scoped_ptr<char[][kArraySize2]> array_pointer(new (nothrow)
        char[kDynamicArraySize][kArraySize2]);
    OverflowTestsSoftExpectTrue(array_pointer == NULL);
  }
}

// Test if calloc() can overflow. Disable on ASAN for now since the
// overflow seems present there.
TEST(SecurityTest, DISABLE_ON_ASAN(CallocOverflow)) {
  const size_t kArraySize = 4096;
  const size_t kMaxSizeT = numeric_limits<size_t>::max();
  const size_t kArraySize2 = kMaxSizeT / kArraySize + 10;
  {
    scoped_ptr<char> array_pointer(
        static_cast<char*>(calloc(kArraySize, kArraySize2)));
    // We need the call to HideValueFromCompiler(): we have seen LLVM
    // optimize away the call to calloc() entirely and assume
    // the pointer to not be NULL.
    EXPECT_TRUE(HideValueFromCompiler(array_pointer.get()) == NULL);
  }
  {
    scoped_ptr<char> array_pointer(
        static_cast<char*>(calloc(kArraySize2, kArraySize)));
    // We need the call to HideValueFromCompiler(): we have seen LLVM
    // optimize away the call to calloc() entirely and assume
    // the pointer to not be NULL.
    EXPECT_TRUE(HideValueFromCompiler(array_pointer.get()) == NULL);
  }
}

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(__x86_64__)
// Useful for debugging.
void PrintProcSelfMaps() {
  int fd = open("/proc/self/maps", O_RDONLY);
  file_util::ScopedFD fd_closer(&fd);
  ASSERT_GE(fd, 0);
  char buffer[1<<13];
  int ret;
  ret = read(fd, buffer, sizeof(buffer) - 1);
  ASSERT_GT(ret, 0);
  buffer[ret - 1] = 0;
  fprintf(stdout, "%s\n", buffer);
}

// Check if TCMalloc uses an underlying random memory allocator.
TEST(SecurityTest, ALLOC_TEST(RandomMemoryAllocations)) {
  if (IsTcMallocBypassed())
    return;
  // Two successsive calls to mmap() have roughly one chance out of 2^6 to
  // have the same two high order nibbles, which is what we are looking at in
  // this test. (In the implementation, we mask these two nibbles with 0x3f,
  // hence the 6 bits).
  // With 32 allocations, we see ~16 that end-up in different buckets (i.e.
  // zones mapped via mmap(), so the chances of this test flaking is roughly
  // 2^-(6*15).
  const int kAllocNumber = 32;
  // Make kAllocNumber successive allocations of growing size and compare the
  // successive pointers to detect adjacent mappings. We grow the size because
  // TCMalloc can sometimes over-allocate.
  scoped_ptr<char, base::FreeDeleter> ptr[kAllocNumber];
  for (int i = 0; i < kAllocNumber; ++i) {
    // Grow the Malloc size slightly sub-exponentially.
    const size_t kMallocSize = 1 << (12 + (i>>1));
    ptr[i].reset(static_cast<char*>(malloc(kMallocSize)));
    ASSERT_TRUE(ptr[i] != NULL);
    if (i > 0) {
      // Without mmap randomization, the two high order nibbles
      // of a 47 bits userland address address will be identical.
      // We're only watching the 6 bits that we actually do touch
      // in our implementation.
      const uintptr_t kHighOrderMask = 0x3f0000000000ULL;
      bool pointer_have_same_high_order =
          (reinterpret_cast<size_t>(ptr[i].get()) & kHighOrderMask) ==
          (reinterpret_cast<size_t>(ptr[i - 1].get()) & kHighOrderMask);
      if (!pointer_have_same_high_order) {
        // PrintProcSelfMaps();
        return;  // Test passes.
      }
    }
  }
  ASSERT_TRUE(false);  // NOTREACHED();
}

#endif  // (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(__x86_64__)

}  // namespace
