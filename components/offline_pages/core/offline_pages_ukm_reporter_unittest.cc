// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_pages_ukm_reporter.h"
#include "base/test/scoped_task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kVisitedUrl[] = "http://m.en.wikipedia.org/wiki/Glider_(sailplane)";
}  // namespace

namespace offline_pages {

class OfflinePagesUkmReporterTest : public testing::Test {
 public:
  ukm::TestUkmRecorder* test_recorder() { return &test_recorder_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
};

TEST_F(OfflinePagesUkmReporterTest, RecordOfflinePageVisit) {
  OfflinePagesUkmReporter reporter;
  GURL gurl(kVisitedUrl);

  reporter.ReportUrlOfflineRequest(gurl, false);

  EXPECT_EQ(1U, test_recorder()->sources_count());
  const ukm::UkmSource* found_source =
      test_recorder()->GetSourceForUrl(kVisitedUrl);
  EXPECT_NE(nullptr, found_source);

  test_recorder()->ExpectMetric(
      *found_source, ukm::builders::OfflinePages_SavePageRequested::kEntryName,
      ukm::builders::OfflinePages_SavePageRequested::
          kRequestedFromForegroundName,
      0);
}

}  // namespace offline_pages
