// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_metrics_reporter.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_ukm_entry.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ukm::TestUkmRecorder;

namespace contextual_suggestions {

namespace {
const char kTestNavigationUrl[] = "https://foo.com";
}  // namespace

class ContextualSuggestionsMetricsReporterTest : public ::testing::Test {
 protected:
  ContextualSuggestionsMetricsReporterTest() = default;

  TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

  ukm::SourceId GetSourceId();

  ContextualSuggestionsMetricsReporter& GetReporter() { return reporter_; }

 private:
  // The reporter under test.
  ContextualSuggestionsMetricsReporter reporter_;

  // Sets up the task scheduling/task-runner environment for each test.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Sets itself as the global UkmRecorder on construction.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSuggestionsMetricsReporterTest);
};

ukm::SourceId ContextualSuggestionsMetricsReporterTest::GetSourceId() {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  test_ukm_recorder_.UpdateSourceURL(source_id, GURL(kTestNavigationUrl));
  return source_id;
}

TEST_F(ContextualSuggestionsMetricsReporterTest, BaseTest) {
  base::HistogramTester histogram_tester;
  GetReporter().SetupForPage(GetSourceId());
  GetReporter().RecordEvent(FETCH_REQUESTED);
  GetReporter().RecordEvent(FETCH_COMPLETED);
  GetReporter().RecordEvent(UI_PEEK_REVERSE_SCROLL);
  GetReporter().RecordEvent(UI_OPENED);
  GetReporter().RecordEvent(SUGGESTION_DOWNLOADED);
  GetReporter().RecordEvent(SUGGESTION_CLICKED);
  // Flush data to write to UKM.
  GetReporter().Flush();
  // Check what we wrote.
  TestUkmRecorder* test_ukm_recorder = GetTestUkmRecorder();
  std::vector<const ukm::mojom::UkmEntry*> entry_vector =
      test_ukm_recorder->GetEntriesByName(kContextualSuggestionsUkmEntryName);
  EXPECT_EQ(1U, entry_vector.size());
  const ukm::mojom::UkmEntry* first_entry = entry_vector[0];
  EXPECT_TRUE(test_ukm_recorder->EntryHasMetric(
      first_entry, kContextualSuggestionsFetchMetricName));
  EXPECT_EQ(static_cast<int64_t>(FetchState::COMPLETED),
            *(test_ukm_recorder->GetEntryMetric(
                first_entry, kContextualSuggestionsFetchMetricName)));
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 0, 0);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 1, 0);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 2, 1);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 3, 0);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 4, 0);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 5, 0);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 6, 0);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 7, 1);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 8, 1);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 9, 1);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 10, 0);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 11, 1);
  histogram_tester.ExpectBucketCount("ContextualSuggestions.Events", 12, 1);
}

// TODO(donnd): add more tests, and test UMA data!

}  // namespace contextual_suggestions
