// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/leak_detector_controller.h"

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/metrics/proto/memory_leak_report.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

// Test class for LeakDetectorController that exposes protected methods.
class TestLeakDetectorController : public LeakDetectorController {
 public:
  using LeakDetectorController::OnLeakFound;

  TestLeakDetectorController() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLeakDetectorController);
};

using LeakReport = LeakDetector::LeakReport;

TEST(LeakDetectorControllerTest, SingleReport) {
  LeakReport report;
  report.alloc_size_bytes = 8;
  report.call_stack = {1, 2, 3, 4};

  TestLeakDetectorController controller;
  controller.OnLeakFound(report);

  std::vector<MemoryLeakReportProto> stored_reports;
  controller.GetLeakReports(&stored_reports);
  ASSERT_EQ(1U, stored_reports.size());

  EXPECT_EQ(8U, stored_reports[0].size_bytes());
  ASSERT_EQ(4, stored_reports[0].call_stack().size());
  EXPECT_EQ(1U, stored_reports[0].call_stack().Get(0));
  EXPECT_EQ(2U, stored_reports[0].call_stack().Get(1));
  EXPECT_EQ(3U, stored_reports[0].call_stack().Get(2));
  EXPECT_EQ(4U, stored_reports[0].call_stack().Get(3));

  // No more reports.
  controller.GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());
}

TEST(LeakDetectorControllerTest, MultipleReportsSeparately) {
  TestLeakDetectorController controller;
  std::vector<MemoryLeakReportProto> stored_reports;

  // Pass in first report.
  LeakReport report;
  report.alloc_size_bytes = 8;
  report.call_stack = {1, 2, 3, 4};

  controller.OnLeakFound(report);
  controller.GetLeakReports(&stored_reports);
  ASSERT_EQ(1U, stored_reports.size());

  EXPECT_EQ(8U, stored_reports[0].size_bytes());
  ASSERT_EQ(4, stored_reports[0].call_stack().size());
  EXPECT_EQ(1U, stored_reports[0].call_stack().Get(0));
  EXPECT_EQ(2U, stored_reports[0].call_stack().Get(1));
  EXPECT_EQ(3U, stored_reports[0].call_stack().Get(2));
  EXPECT_EQ(4U, stored_reports[0].call_stack().Get(3));

  controller.GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());

  // Pass in second report.
  report.alloc_size_bytes = 16;
  report.call_stack = {5, 6, 7, 8, 9, 10};
  controller.OnLeakFound(report);

  controller.GetLeakReports(&stored_reports);
  ASSERT_EQ(1U, stored_reports.size());

  EXPECT_EQ(16U, stored_reports[0].size_bytes());
  ASSERT_EQ(6, stored_reports[0].call_stack().size());
  EXPECT_EQ(5U, stored_reports[0].call_stack().Get(0));
  EXPECT_EQ(6U, stored_reports[0].call_stack().Get(1));
  EXPECT_EQ(7U, stored_reports[0].call_stack().Get(2));
  EXPECT_EQ(8U, stored_reports[0].call_stack().Get(3));
  EXPECT_EQ(9U, stored_reports[0].call_stack().Get(4));
  EXPECT_EQ(10U, stored_reports[0].call_stack().Get(5));

  controller.GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());

  // Pass in third report.
  report.alloc_size_bytes = 24;
  report.call_stack = {9, 10, 11, 12, 13, 14, 15, 16};
  controller.OnLeakFound(report);

  controller.GetLeakReports(&stored_reports);
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

  controller.GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());
}

TEST(LeakDetectorControllerTest, MultipleReportsTogether) {
  LeakReport report1, report2, report3;
  report1.alloc_size_bytes = 8;
  report1.call_stack = {1, 2, 3, 4};
  report2.alloc_size_bytes = 16;
  report2.call_stack = {5, 6, 7, 8, 9, 10};
  report3.alloc_size_bytes = 24;
  report3.call_stack = {9, 10, 11, 12, 13, 14, 15, 16};

  TestLeakDetectorController controller;
  controller.OnLeakFound(report1);
  controller.OnLeakFound(report2);
  controller.OnLeakFound(report3);

  std::vector<MemoryLeakReportProto> stored_reports;

  controller.GetLeakReports(&stored_reports);
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

  controller.GetLeakReports(&stored_reports);
  ASSERT_EQ(0U, stored_reports.size());
}

}  // namespace metrics
