// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Contains;
using ::testing::Not;

namespace password_manager {

namespace {

constexpr char kTestUrl[] = "https://www.example.com/";

// Creates a PasswordManagerMetricsRecorder that reports metrics for kTestUrl.
PasswordManagerMetricsRecorder CreateMetricsRecorder(
    ukm::UkmRecorder* ukm_recorder) {
  return PasswordManagerMetricsRecorder(
      ukm_recorder, ukm_recorder->GetNewSourceID(), GURL(kTestUrl));
}

}  // namespace

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordField) {
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    PasswordManagerMetricsRecorder recorder(
        CreateMetricsRecorder(&test_ukm_recorder));
    recorder.RecordUserModifiedPasswordField();
  }

  const auto& entries = test_ukm_recorder.GetEntriesByName("PageWithPassword");
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));
    test_ukm_recorder.ExpectEntryMetric(entry, kUkmUserModifiedPasswordField,
                                        1);
  }
}

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordFieldMultipleTimes) {
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    PasswordManagerMetricsRecorder recorder(
        CreateMetricsRecorder(&test_ukm_recorder));
    // Multiple calls should not create more than one entry.
    recorder.RecordUserModifiedPasswordField();
    recorder.RecordUserModifiedPasswordField();
  }
  const auto& entries = test_ukm_recorder.GetEntriesByName("PageWithPassword");
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));
    test_ukm_recorder.ExpectEntryMetric(entry, kUkmUserModifiedPasswordField,
                                        1);
  }
}

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordFieldNotCalled) {
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    PasswordManagerMetricsRecorder recorder(
        CreateMetricsRecorder(&test_ukm_recorder));
  }
  const auto& entries = test_ukm_recorder.GetEntriesByName("PageWithPassword");
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(kTestUrl));
    EXPECT_FALSE(
        test_ukm_recorder.EntryHasMetric(entry, kUkmUserModifiedPasswordField));
  }
}

}  // namespace password_manager
