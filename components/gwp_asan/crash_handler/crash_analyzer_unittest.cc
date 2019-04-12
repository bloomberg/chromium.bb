// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include "base/debug/stack_trace.h"
#include "base/test/gtest_util.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan {
namespace internal {

TEST(CrashAnalyzerTest, StackTraceCollection) {
  GuardedPageAllocator gpa_;
  gpa_.Init(1, 1, 1);

  void* ptr = gpa_.Allocate(10);
  ASSERT_NE(ptr, nullptr);

  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  // gpa_ was initialized with only a single metadata slot so the allocation
  // should have metadata index 0.
  ASSERT_EQ(gpa_.metadata_[0].alloc_ptr, addr);

  gwp_asan::Crash_AllocationInfo allocation_info;
  CrashAnalyzer::ReadAllocationInfo(gpa_.metadata_[0].alloc, &allocation_info);
  ASSERT_GT(allocation_info.stack_trace_size(), 0);

  base::debug::StackTrace st;
  size_t trace_len;
  const void* const* trace = st.Addresses(&trace_len);
  ASSERT_NE(trace, nullptr);
  ASSERT_GT(trace_len, 0U);

  // Scan the stack trace we collected and the one from the allocator and make
  // sure that all entries up to the current frame match.
  for (size_t i = 1; i <= trace_len; i++) {
    ASSERT_EQ(
        allocation_info.stack_trace(allocation_info.stack_trace_size() - i),
        reinterpret_cast<uintptr_t>(trace[trace_len - i]));

    if (trace[trace_len - i] == __builtin_return_address(0))
      return;
  }

  FAIL() << "Return address not found in the collected stack trace.";
}

}  // namespace internal
}  // namespace gwp_asan
