// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_detector.h"

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

// Default values for LeakDetector params. See header file for the meaning of
// each parameter.
const float kDefaultSamplingRate = 1.0f;
const size_t kDefaultMaxCallStackUnwindDepth = 4;
const uint64_t kDefaultAnalysisIntervalBytes = 32 * 1024 * 1024;  // 32 MiB.
const uint32_t kDefaultSizeSuspicionThreshold = 4;
const uint32_t kDefaultCallStackSuspicionThreshold = 4;

using LeakReport = LeakDetector::LeakReport;

// Observer class that receives leak reports and stores them in |reports_|.
// Only one copy of each unique report will be stored.
class TestObserver : public LeakDetector::Observer {
 public:
  // Contains a comparator function used to compare LeakReports for uniqueness.
  struct Comparator {
    bool operator()(const LeakReport& a, const LeakReport& b) const {
      if (a.alloc_size_bytes != b.alloc_size_bytes)
        return a.alloc_size_bytes < b.alloc_size_bytes;

      return a.call_stack < b.call_stack;
    }
  };

  TestObserver() {}

  void OnLeakFound(const LeakReport& report) override {
    reports_.insert(report);
  }

  const std::set<LeakReport, Comparator>& reports() const { return reports_; }

 private:
  // Container for all leak reports received through OnLeakFound(). Stores only
  // one copy of each unique report.
  std::set<LeakReport, Comparator> reports_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class LeakDetectorTest : public ::testing::Test {
 public:
  LeakDetectorTest()
      : detector_(new LeakDetector(kDefaultSamplingRate,
                                   kDefaultMaxCallStackUnwindDepth,
                                   kDefaultAnalysisIntervalBytes,
                                   kDefaultSizeSuspicionThreshold,
                                   kDefaultCallStackSuspicionThreshold)) {}

 protected:
  // Use a scoped_ptr to hold the test object so it can be destroyed before the
  // test is over.
  scoped_ptr<LeakDetector> detector_;

 private:
  // For supporting content::BrowserThread operations.
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetectorTest);
};

TEST_F(LeakDetectorTest, NotifyObservers) {
  // Generate two sets of leak reports.
  std::vector<LeakReport> reports1(3);
  reports1[0].alloc_size_bytes = 8;
  reports1[0].call_stack = {1, 2, 3, 4};
  reports1[1].alloc_size_bytes = 16;
  reports1[1].call_stack = {5, 6, 7, 8};
  reports1[2].alloc_size_bytes = 24;
  reports1[2].call_stack = {9, 10, 11, 12};

  std::vector<LeakReport> reports2(3);
  reports2[0].alloc_size_bytes = 32;
  reports2[0].call_stack = {1, 2, 4, 8};
  reports2[1].alloc_size_bytes = 40;
  reports2[1].call_stack = {16, 32, 64, 128};
  reports2[2].alloc_size_bytes = 48;
  reports2[2].call_stack = {256, 512, 1024, 2048};

  // Register three observers;
  TestObserver obs1, obs2, obs3;
  detector_->AddObserver(&obs1);
  detector_->AddObserver(&obs2);
  detector_->AddObserver(&obs3);

  // Pass both sets of reports to the leak detector.
  detector_->NotifyObservers(reports1);
  detector_->NotifyObservers(reports2);

  // Shut down the leak detector before checking the reports, so that the
  // stored reports can be examined without new reports being generated.
  detector_.reset();

  // Check that all three observers got both sets of reports, passed in
  // separately.
  for (const TestObserver* obs : {&obs1, &obs2, &obs3}) {
    EXPECT_EQ(6U, obs->reports().size());
    for (const auto& report : {reports1[0], reports1[1], reports1[2],
                               reports2[0], reports2[1], reports2[2]}) {
      EXPECT_TRUE(obs->reports().find(report) != obs->reports().end());
    }
  }
}

}  // namespace metrics
