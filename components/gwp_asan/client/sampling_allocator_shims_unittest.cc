// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_allocator_shims.h"

#include <stdlib.h>
#include <cstdlib>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/process/process_metrics.h"
#include "base/test/gtest_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/gwp_asan/common/guarded_page_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

// These tests install global allocator shims so they are not safe to run in
// multi-threaded contexts. Instead they're implemented as multi-process tests.

#if BUILDFLAG(USE_ALLOCATOR_SHIM)

namespace gwp_asan {
namespace internal {

extern GuardedPageAllocator& GetGpaForTesting();

namespace {

constexpr size_t kSamplingFrequency = 10;

// The likelihood of iterating this many times and not getting a sampled
// allocation is less than one in a trillion [1 / exp(32)].
constexpr size_t kLoopIterations = kSamplingFrequency * 32;

constexpr int kSuccess = 0;

class SamplingAllocatorShimsTest : public base::MultiProcessTest {
 protected:
  void runTest(const char* name) {
    base::Process process = SpawnChild(name);
    int exit_code = -1;
    ASSERT_TRUE(process.WaitForExitWithTimeout(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, kSuccess);
  }
};

// Return whether some of the allocations returned by the calling the allocate
// parameter are sampled to the guarded allocator.
bool allocationCheck(std::function<void*(void)> allocate,
                     std::function<void(void*)> free) {
  size_t guarded = 0;
  size_t unguarded = 0;
  for (size_t i = 0; i < kLoopIterations; i++) {
    std::unique_ptr<void, decltype(free)> alloc(allocate(), free);
    EXPECT_NE(alloc.get(), nullptr);
    if (!alloc.get())
      return false;

    if (GetGpaForTesting().PointerIsMine(alloc.get()))
      guarded++;
    else
      unguarded++;
  }

  return guarded > 0 && unguarded > 0;
}

MULTIPROCESS_TEST_MAIN(BasicFunctionality) {
  InstallAllocatorHooks(GuardedPageAllocator::kGpaMaxPages, kSamplingFrequency);

  const size_t page_size = base::GetPageSize();

  EXPECT_TRUE(allocationCheck([&] { return malloc(page_size); }, &free));
  EXPECT_TRUE(allocationCheck([&] { return calloc(1, page_size); }, &free));
  EXPECT_TRUE(
      allocationCheck([&] { return realloc(nullptr, page_size); }, &free));

#if defined(OS_WIN)
  EXPECT_TRUE(allocationCheck(
      [&] { return _aligned_malloc(page_size, page_size); }, &_aligned_free));
  EXPECT_TRUE(allocationCheck([&] { return _aligned_malloc(page_size, 1); },
                              &_aligned_free));
#else
  EXPECT_TRUE(allocationCheck(
      [&] { return aligned_alloc(page_size, page_size); }, &free));
  EXPECT_TRUE(
      allocationCheck([&] { return aligned_alloc(1, page_size); }, &free));
#endif

  EXPECT_TRUE(
      allocationCheck([&] { return std::malloc(page_size); }, &std::free));
  EXPECT_TRUE(
      allocationCheck([&] { return std::calloc(1, page_size); }, &std::free));
  EXPECT_TRUE(allocationCheck([&] { return std::realloc(nullptr, page_size); },
                              &std::free));

  EXPECT_TRUE(allocationCheck([] { return new int; },
                              [](void* ptr) { delete (int*)ptr; }));
  EXPECT_TRUE(allocationCheck([] { return new int[4]; },
                              [](void* ptr) { delete[](int*) ptr; }));

  EXPECT_FALSE(allocationCheck([&] { return malloc(page_size + 1); }, &free));

  return kSuccess;
}

TEST_F(SamplingAllocatorShimsTest, BasicFunctionality) {
  runTest("BasicFunctionality");
}

MULTIPROCESS_TEST_MAIN(Realloc) {
  InstallAllocatorHooks(GuardedPageAllocator::kGpaMaxPages, kSamplingFrequency);

  void* alloc = GetGpaForTesting().Allocate(base::GetPageSize());
  CHECK_NE(alloc, nullptr);

  constexpr unsigned char kFillChar = 0xff;
  memset(alloc, kFillChar, base::GetPageSize());

  unsigned char* new_alloc =
      static_cast<unsigned char*>(realloc(alloc, base::GetPageSize() + 1));
  CHECK_NE(alloc, new_alloc);
  CHECK_EQ(GetGpaForTesting().PointerIsMine(new_alloc), false);

  for (size_t i = 0; i < base::GetPageSize(); i++)
    CHECK_EQ(new_alloc[i], kFillChar);

  free(new_alloc);

  return kSuccess;
}

TEST_F(SamplingAllocatorShimsTest, Realloc) {
  runTest("Realloc");
}

MULTIPROCESS_TEST_MAIN(Calloc) {
  InstallAllocatorHooks(GuardedPageAllocator::kGpaMaxPages, kSamplingFrequency);

  for (size_t i = 0; i < kLoopIterations; i++) {
    unsigned char* alloc =
        static_cast<unsigned char*>(calloc(base::GetPageSize(), 1));
    CHECK_NE(alloc, nullptr);

    if (GetGpaForTesting().PointerIsMine(alloc)) {
      for (size_t i = 0; i < base::GetPageSize(); i++)
        CHECK_EQ(alloc[i], 0U);
      free(alloc);
      return kSuccess;
    }

    free(alloc);
  }

  return kSuccess + 1;
}

TEST_F(SamplingAllocatorShimsTest, Calloc) {
  runTest("Calloc");
}

}  // namespace

}  // namespace internal
}  // namespace gwp_asan

#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
