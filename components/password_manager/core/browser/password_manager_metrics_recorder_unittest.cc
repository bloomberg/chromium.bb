// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "components/ukm/public/ukm_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
constexpr char kTestUrl[] = "https://www.example.com/";

// Create a UkmEntryBuilder with a SourceId that is initialized for kTestUrl.
std::unique_ptr<ukm::UkmEntryBuilder> CreateUkmEntryBuilder(
    ukm::TestUkmRecorder* test_ukm_recorder) {
  ukm::SourceId source_id = test_ukm_recorder->GetNewSourceID();
  static_cast<ukm::UkmRecorder*>(test_ukm_recorder)
      ->UpdateSourceURL(source_id, GURL(kTestUrl));
  return PasswordManagerMetricsRecorder::CreateUkmEntryBuilder(
      test_ukm_recorder, source_id);
}

// Verifies that the metric |metric_name| was recorded with value |value| in the
// single entry of |test_ukm_recorder| exactly |expected_count| times.
void ExpectUkmValueCount(ukm::TestUkmRecorder* test_ukm_recorder,
                         const char* metric_name,
                         int64_t value,
                         int64_t expected_count) {
  const ukm::UkmSource* source = test_ukm_recorder->GetSourceForUrl(kTestUrl);
  ASSERT_TRUE(source);

  ASSERT_EQ(1U, test_ukm_recorder->entries_count());
  const ukm::mojom::UkmEntry* entry = test_ukm_recorder->GetEntry(0);

  int64_t occurrences = 0;
  for (const ukm::mojom::UkmMetricPtr& metric : entry->metrics) {
    if (metric->metric_hash == base::HashMetricName(metric_name) &&
        metric->value == value)
      ++occurrences;
  }
  EXPECT_EQ(expected_count, occurrences) << metric_name << ": " << value;
}

}  // namespace

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordField) {
  ukm::TestUkmRecorder test_ukm_recorder;
  {
    PasswordManagerMetricsRecorder recorder(
        CreateUkmEntryBuilder(&test_ukm_recorder));
    recorder.RecordUserModifiedPasswordField();
  }
  ExpectUkmValueCount(&test_ukm_recorder,
                      internal::kUkmUserModifiedPasswordField, 1, 1);
}

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordFieldMultipleTimes) {
  ukm::TestUkmRecorder test_ukm_recorder;
  {
    PasswordManagerMetricsRecorder recorder(
        CreateUkmEntryBuilder(&test_ukm_recorder));
    // Multiple calls should not create more than one entry.
    recorder.RecordUserModifiedPasswordField();
    recorder.RecordUserModifiedPasswordField();
  }
  ExpectUkmValueCount(&test_ukm_recorder,
                      internal::kUkmUserModifiedPasswordField, 1, 1);
}

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordFieldNotCalled) {
  ukm::TestUkmRecorder test_ukm_recorder;
  {
    PasswordManagerMetricsRecorder recorder(
        CreateUkmEntryBuilder(&test_ukm_recorder));
  }
  ExpectUkmValueCount(&test_ukm_recorder,
                      internal::kUkmUserModifiedPasswordField, 1, 0);
}

}  // namespace password_manager
