// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/leak_detector/leak_detector_controller.h"

#include <set>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/metrics/proto/memory_leak_report.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

// Converts a vector of values into a protobuf RepeatedField. Although there is
// no hard requirement, T should be a POD type since that is what RepeatedField
// is used for in protobuf classes.
template <typename T>
void InitializeRepeatedField(const std::vector<T>& input,
                             ::google::protobuf::RepeatedField<T>* output) {
  *output = ::google::protobuf::RepeatedField<T>(input.begin(), input.end());
}

}  // namespace

// Test class for LeakDetectorController that exposes protected methods.
class TestLeakDetectorController : public LeakDetectorController {
 public:
  using LeakDetectorController::OnLeaksFound;

  // This constructor suppresses starting memory metrics job in the superclass.
  TestLeakDetectorController() {
    LeakDetectorController::set_enable_collect_memory_usage_step(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLeakDetectorController);
};

class LeakDetectorControllerTest : public ::testing::Test {
 public:
  LeakDetectorControllerTest() {}

 private:
  // For supporting content::BrowserThread operations.
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetectorControllerTest);
};

// Use a global instance of the test class because LeakDetectorController
// initializes class LeakDetector, which can only be initialized once, enforced
// by an internal CHECK. Multiple initializations of LeakDetectorController in
// the same process will result in multiple initializations of class
// LeakDetector. It has to be Leaky as running its destructor will otherwise
// DCHECK when called outside the scope of a TestBrowserThreadBundle.
//
// See src/components/metrics/leak_detector/leak_detector.h for more info.
base::LazyInstance<TestLeakDetectorController>::Leaky g_instance =
    LAZY_INSTANCE_INITIALIZER;

TEST_F(LeakDetectorControllerTest, SingleReport) {
  MemoryLeakReportProto report;
  report.set_size_bytes(8);
  InitializeRepeatedField({1, 2, 3, 4}, report.mutable_call_stack());

  TestLeakDetectorController* controller = &g_instance.Get();
  controller->OnLeaksFound({report});

  std::vector<MemoryLeakReportProto> stored_reports;
  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(1U, stored_reports.size());

  const MemoryLeakReportProto& proto = stored_reports[0];
  EXPECT_EQ(8U, proto.size_bytes());
  ASSERT_EQ(4, proto.call_stack().size());
  EXPECT_EQ(1U, proto.call_stack().Get(0));
  EXPECT_EQ(2U, proto.call_stack().Get(1));
  EXPECT_EQ(3U, proto.call_stack().Get(2));
  EXPECT_EQ(4U, proto.call_stack().Get(3));

  // Check that default leak detector parameters are stored in the leak report.
  // Default values are listed in leak_detector_controller.cc.
  EXPECT_DOUBLE_EQ(1.0f / 256, proto.params().sampling_rate());
  EXPECT_EQ(4U, proto.params().max_stack_depth());
  EXPECT_EQ(32U * 1024 * 1024, proto.params().analysis_interval_bytes());
  EXPECT_EQ(4U, proto.params().size_suspicion_threshold());
  EXPECT_EQ(4U, proto.params().call_stack_suspicion_threshold());

  // No more reports.
  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());
}

TEST_F(LeakDetectorControllerTest, SingleReportHistory) {
  MemoryLeakReportProto report;

  auto* entry = report.add_alloc_breakdown_history();
  InitializeRepeatedField({100, 200, 300}, entry->mutable_counts_by_size());
  entry->set_count_for_call_stack(15);

  entry = report.add_alloc_breakdown_history();
  InitializeRepeatedField({150, 250, 350, 650},
                          entry->mutable_counts_by_size());
  entry->set_count_for_call_stack(30);

  entry = report.add_alloc_breakdown_history();
  InitializeRepeatedField({200, 300, 400, 700, 800},
                          entry->mutable_counts_by_size());
  entry->set_count_for_call_stack(45);

  TestLeakDetectorController* controller = &g_instance.Get();
  controller->OnLeaksFound({report});

  std::vector<MemoryLeakReportProto> stored_reports;
  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(1U, stored_reports.size());

  const auto& history = stored_reports[0].alloc_breakdown_history();
  ASSERT_EQ(3, history.size());

  ASSERT_EQ(3, history.Get(0).counts_by_size().size());
  EXPECT_EQ(100U, history.Get(0).counts_by_size(0));
  EXPECT_EQ(200U, history.Get(0).counts_by_size(1));
  EXPECT_EQ(300U, history.Get(0).counts_by_size(2));
  EXPECT_EQ(15U, history.Get(0).count_for_call_stack());

  ASSERT_EQ(4, history.Get(1).counts_by_size_size());
  EXPECT_EQ(150U, history.Get(1).counts_by_size(0));
  EXPECT_EQ(250U, history.Get(1).counts_by_size(1));
  EXPECT_EQ(350U, history.Get(1).counts_by_size(2));
  EXPECT_EQ(650U, history.Get(1).counts_by_size(3));
  EXPECT_EQ(30U, history.Get(1).count_for_call_stack());

  ASSERT_EQ(5, history.Get(2).counts_by_size_size());
  EXPECT_EQ(200U, history.Get(2).counts_by_size(0));
  EXPECT_EQ(300U, history.Get(2).counts_by_size(1));
  EXPECT_EQ(400U, history.Get(2).counts_by_size(2));
  EXPECT_EQ(700U, history.Get(2).counts_by_size(3));
  EXPECT_EQ(800U, history.Get(2).counts_by_size(4));
  EXPECT_EQ(45U, history.Get(2).count_for_call_stack());

  // No more reports.
  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());
}

TEST_F(LeakDetectorControllerTest, MultipleReportsSeparately) {
  TestLeakDetectorController* controller = &g_instance.Get();
  std::vector<MemoryLeakReportProto> stored_reports;

  // Pass in first report.
  MemoryLeakReportProto report1;
  report1.set_size_bytes(8);
  InitializeRepeatedField({1, 2, 3, 4}, report1.mutable_call_stack());

  controller->OnLeaksFound({report1});
  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(1U, stored_reports.size());

  EXPECT_EQ(8U, stored_reports[0].size_bytes());
  ASSERT_EQ(4, stored_reports[0].call_stack().size());
  EXPECT_EQ(1U, stored_reports[0].call_stack().Get(0));
  EXPECT_EQ(2U, stored_reports[0].call_stack().Get(1));
  EXPECT_EQ(3U, stored_reports[0].call_stack().Get(2));
  EXPECT_EQ(4U, stored_reports[0].call_stack().Get(3));

  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());

  // Pass in second report.
  MemoryLeakReportProto report2;
  report2.set_size_bytes(16);
  InitializeRepeatedField({5, 6, 7, 8, 9, 10}, report2.mutable_call_stack());
  controller->OnLeaksFound({report2});

  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(1U, stored_reports.size());

  EXPECT_EQ(16U, stored_reports[0].size_bytes());
  ASSERT_EQ(6, stored_reports[0].call_stack().size());
  EXPECT_EQ(5U, stored_reports[0].call_stack().Get(0));
  EXPECT_EQ(6U, stored_reports[0].call_stack().Get(1));
  EXPECT_EQ(7U, stored_reports[0].call_stack().Get(2));
  EXPECT_EQ(8U, stored_reports[0].call_stack().Get(3));
  EXPECT_EQ(9U, stored_reports[0].call_stack().Get(4));
  EXPECT_EQ(10U, stored_reports[0].call_stack().Get(5));

  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());

  // Pass in third report.
  MemoryLeakReportProto report3;
  report3.set_size_bytes(24);
  InitializeRepeatedField({9, 10, 11, 12, 13, 14, 15, 16},
                          report3.mutable_call_stack());
  controller->OnLeaksFound({report3});

  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(1U, stored_reports.size());

  EXPECT_EQ(24U, stored_reports[0].size_bytes());
  ASSERT_EQ(8, stored_reports[0].call_stack().size());
  EXPECT_EQ(9U, stored_reports[0].call_stack().Get(0));
  EXPECT_EQ(10U, stored_reports[0].call_stack().Get(1));
  EXPECT_EQ(11U, stored_reports[0].call_stack().Get(2));
  EXPECT_EQ(12U, stored_reports[0].call_stack().Get(3));
  EXPECT_EQ(13U, stored_reports[0].call_stack().Get(4));
  EXPECT_EQ(14U, stored_reports[0].call_stack().Get(5));
  EXPECT_EQ(15U, stored_reports[0].call_stack().Get(6));
  EXPECT_EQ(16U, stored_reports[0].call_stack().Get(7));

  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());
}

TEST_F(LeakDetectorControllerTest, MultipleReportsTogether) {
  std::vector<MemoryLeakReportProto> reports(3);
  reports[0].set_size_bytes(8);
  InitializeRepeatedField({1, 2, 3, 4}, reports[0].mutable_call_stack());
  reports[1].set_size_bytes(16);
  InitializeRepeatedField({5, 6, 7, 8, 9, 10}, reports[1].mutable_call_stack());
  reports[2].set_size_bytes(24);
  InitializeRepeatedField({9, 10, 11, 12, 13, 14, 15, 16},
                          reports[2].mutable_call_stack());

  TestLeakDetectorController* controller = &g_instance.Get();
  controller->OnLeaksFound(reports);

  std::vector<MemoryLeakReportProto> stored_reports;

  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(3U, stored_reports.size());

  EXPECT_EQ(8U, stored_reports[0].size_bytes());
  ASSERT_EQ(4, stored_reports[0].call_stack().size());
  EXPECT_EQ(1U, stored_reports[0].call_stack().Get(0));
  EXPECT_EQ(2U, stored_reports[0].call_stack().Get(1));
  EXPECT_EQ(3U, stored_reports[0].call_stack().Get(2));
  EXPECT_EQ(4U, stored_reports[0].call_stack().Get(3));

  EXPECT_EQ(16U, stored_reports[1].size_bytes());
  ASSERT_EQ(6, stored_reports[1].call_stack().size());
  EXPECT_EQ(5U, stored_reports[1].call_stack().Get(0));
  EXPECT_EQ(6U, stored_reports[1].call_stack().Get(1));
  EXPECT_EQ(7U, stored_reports[1].call_stack().Get(2));
  EXPECT_EQ(8U, stored_reports[1].call_stack().Get(3));
  EXPECT_EQ(9U, stored_reports[1].call_stack().Get(4));
  EXPECT_EQ(10U, stored_reports[1].call_stack().Get(5));

  EXPECT_EQ(24U, stored_reports[2].size_bytes());
  ASSERT_EQ(8, stored_reports[2].call_stack().size());
  EXPECT_EQ(9U, stored_reports[2].call_stack().Get(0));
  EXPECT_EQ(10U, stored_reports[2].call_stack().Get(1));
  EXPECT_EQ(11U, stored_reports[2].call_stack().Get(2));
  EXPECT_EQ(12U, stored_reports[2].call_stack().Get(3));
  EXPECT_EQ(13U, stored_reports[2].call_stack().Get(4));
  EXPECT_EQ(14U, stored_reports[2].call_stack().Get(5));
  EXPECT_EQ(15U, stored_reports[2].call_stack().Get(6));
  EXPECT_EQ(16U, stored_reports[2].call_stack().Get(7));

  controller->GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());
}

}  // namespace metrics
