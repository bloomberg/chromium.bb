// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include "base/debug/stack_trace.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_exception_snapshot.h"
#include "third_party/crashpad/crashpad/test/process_type.h"
#include "third_party/crashpad/crashpad/util/process/process_memory_native.h"

namespace gwp_asan {
namespace internal {

TEST(CrashAnalyzerTest, StackTraceCollection) {
  GuardedPageAllocator gpa;
  gpa.Init(1, 1, 1);

  void* ptr = gpa.Allocate(10);
  ASSERT_NE(ptr, nullptr);
  gpa.Deallocate(ptr);

  crashpad::ProcessMemoryNative memory;
  ASSERT_TRUE(memory.Initialize(crashpad::test::GetSelfProcess()));

  // Lets pretend a double free() occurred on the allocation we saw previously.
  crashpad::test::TestExceptionSnapshot exception_snapshot;
  gpa.state_.double_free_address = reinterpret_cast<uintptr_t>(ptr);

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  CrashAnalyzer::AnalyzeCrashedAllocator(
      memory, exception_snapshot, reinterpret_cast<uintptr_t>(&gpa), &proto);

  histogram_tester.ExpectTotalCount(CrashAnalyzer::kCrashAnalysisHistogram, 0);

  ASSERT_TRUE(proto.has_allocation());
  ASSERT_TRUE(proto.has_deallocation());

  base::debug::StackTrace st;
  size_t trace_len;
  const void* const* trace = st.Addresses(&trace_len);
  ASSERT_NE(trace, nullptr);
  ASSERT_GT(trace_len, 0U);

  // Adjust the stack trace to point to the entry above the current frame.
  while (trace_len > 0) {
    if (trace[0] == __builtin_return_address(0))
      break;

    trace++;
    trace_len--;
  }

  ASSERT_GT(proto.allocation().stack_trace_size(), (int)trace_len);
  ASSERT_GT(proto.deallocation().stack_trace_size(), (int)trace_len);

  // Ensure that the allocation and deallocation stack traces match the stack
  // frames that we collected above the current frame.
  for (size_t i = 1; i <= trace_len; i++) {
    SCOPED_TRACE(i);
    ASSERT_EQ(proto.allocation().stack_trace(
                  proto.allocation().stack_trace_size() - i),
              reinterpret_cast<uintptr_t>(trace[trace_len - i]));
    ASSERT_EQ(proto.deallocation().stack_trace(
                  proto.deallocation().stack_trace_size() - i),
              reinterpret_cast<uintptr_t>(trace[trace_len - i]));
  }

  // Allocate() and Deallocate() were called from different points in this test.
  ASSERT_NE(proto.allocation().stack_trace(
                proto.allocation().stack_trace_size() - (trace_len + 1)),
            proto.deallocation().stack_trace(
                proto.deallocation().stack_trace_size() - (trace_len + 1)));
}

TEST(CrashAnalyzerTest, InternalError) {
  GuardedPageAllocator gpa;
  gpa.Init(1, 1, 1);

  crashpad::ProcessMemoryNative memory;
  ASSERT_TRUE(memory.Initialize(crashpad::test::GetSelfProcess()));

  // Lets pretend a invalid free() occurred in the allocator region.
  crashpad::test::TestExceptionSnapshot exception_snapshot;
  gpa.state_.free_invalid_address =
      reinterpret_cast<uintptr_t>(gpa.state_.first_page_addr);
  // Out of bounds slot_to_metadata_idx, allocator was initialized with only a
  // single entry slot/metadata entry.
  gpa.slot_to_metadata_idx_[0] = 5;

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  CrashAnalyzer::AnalyzeCrashedAllocator(
      memory, exception_snapshot, reinterpret_cast<uintptr_t>(&gpa), &proto);

  int result = static_cast<int>(
      CrashAnalyzer::GwpAsanCrashAnalysisResult::kErrorBadMetadataIndex);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(CrashAnalyzer::kCrashAnalysisHistogram),
      testing::ElementsAre(base::Bucket(result, 1)));

  EXPECT_TRUE(proto.has_internal_error());
  ASSERT_TRUE(proto.has_missing_metadata());
  EXPECT_TRUE(proto.missing_metadata());
}

}  // namespace internal
}  // namespace gwp_asan
