// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/snapshot/annotation_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"
#include "third_party/crashpad/crashpad/test/process_type.h"
#include "third_party/crashpad/crashpad/util/process/process_memory_native.h"

namespace gwp_asan {
namespace internal {

namespace {

// Initializes this ProcessSnapshot so that it appears the given allocator was
// used for backing malloc.
void InitializeSnapshot(crashpad::test::TestProcessSnapshot* process_snapshot,
                        const GuardedPageAllocator& gpa) {
  std::string crash_key_value = gpa.GetCrashKey();
  std::vector<uint8_t> crash_key_vector(crash_key_value.begin(),
                                        crash_key_value.end());

  std::vector<crashpad::AnnotationSnapshot> annotations;
  annotations.emplace_back(
      kMallocCrashKey,
      static_cast<uint16_t>(crashpad::Annotation::Type::kString),
      crash_key_vector);

  auto module = std::make_unique<crashpad::test::TestModuleSnapshot>();
  module->SetAnnotationObjects(annotations);

  auto exception = std::make_unique<crashpad::test::TestExceptionSnapshot>();
  // Just match the bitness, the actual architecture doesn't matter.
#if defined(ARCH_CPU_64_BITS)
  exception->MutableContext()->architecture =
      crashpad::CPUArchitecture::kCPUArchitectureX86_64;
#else
  exception->MutableContext()->architecture =
      crashpad::CPUArchitecture::kCPUArchitectureX86;
#endif

  auto memory = std::make_unique<crashpad::ProcessMemoryNative>();
  ASSERT_TRUE(memory->Initialize(crashpad::test::GetSelfProcess()));

  process_snapshot->AddModule(std::move(module));
  process_snapshot->SetException(std::move(exception));
  process_snapshot->SetProcessMemory(std::move(memory));
}

}  // namespace

TEST(CrashAnalyzerTest, StackTraceCollection) {
  GuardedPageAllocator gpa;
  gpa.Init(1, 1, 1);

  void* ptr = gpa.Allocate(10);
  ASSERT_NE(ptr, nullptr);
  gpa.Deallocate(ptr);

  // Lets pretend a double free() occurred on the allocation we saw previously.
  gpa.state_.double_free_address = reinterpret_cast<uintptr_t>(ptr);

  crashpad::test::TestProcessSnapshot process_snapshot;
  InitializeSnapshot(&process_snapshot, gpa);

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  bool proto_present =
      CrashAnalyzer::GetExceptionInfo(process_snapshot, &proto);
  ASSERT_TRUE(proto_present);

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

  // Lets pretend an invalid free() occurred in the allocator region.
  gpa.state_.free_invalid_address =
      reinterpret_cast<uintptr_t>(gpa.state_.first_page_addr);
  // Out of bounds slot_to_metadata_idx, allocator was initialized with only a
  // single entry slot/metadata entry.
  gpa.slot_to_metadata_idx_[0] = 5;

  crashpad::test::TestProcessSnapshot process_snapshot;
  InitializeSnapshot(&process_snapshot, gpa);

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  bool proto_present =
      CrashAnalyzer::GetExceptionInfo(process_snapshot, &proto);
  ASSERT_TRUE(proto_present);

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
