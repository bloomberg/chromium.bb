// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"

#include <memory>

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyOf;
using testing::HasSubstr;
using testing::Not;

TEST(OptimizationGuideNavigationDataTest, RecordMetricsNoData) {
  base::test::TaskEnvironment env;

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.RecordMetrics();

  // Make sure no UMA recorded.
  EXPECT_THAT(histogram_tester.GetAllHistogramsRecorded(),
              Not(AnyOf(HasSubstr("OptimizationGuide.ApplyDecision"),
                        HasSubstr("OptimizationGuide.TargetDecision"))));
  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsBadHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.set_serialized_hint_version_string("123");
  data.RecordMetrics();

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsEmptyHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/123);
  data.set_serialized_hint_version_string("");

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsZeroTimestampOrSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(0);
  hint_version.set_hint_source(optimization_guide::proto::HINT_SOURCE_UNKNOWN);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics();

  // Make sure UKM not recorded for all empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsGoodHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics();

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithUnknownSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  hint_version.set_hint_source(optimization_guide::proto::HINT_SOURCE_UNKNOWN);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics();

  // Make sure version is serialized properly and UKM is only recorded for
  // non-empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithNoSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics();

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithZeroTimestamp) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(0);
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics();

  // Make sure version is serialized properly and UKM is only recorded for
  // non-empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithNoTimestamp) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data.set_serialized_hint_version_string(hint_version_string);
  data.RecordMetrics();

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsMultipleOptimizationTypes) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::kAllowedByHint);
  data.SetDecisionForOptimizationType(
      optimization_guide::proto::DEFER_ALL_SCRIPT,
      optimization_guide::OptimizationTypeDecision::
          kAllowedByOptimizationFilter);
  data.RecordMetrics();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(
          optimization_guide::OptimizationTypeDecision::kAllowedByHint),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.DeferAllScript",
      static_cast<int>(optimization_guide::OptimizationTypeDecision::
                           kAllowedByOptimizationFilter),
      1);
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsRecordsLatestType) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::kAllowedByHint);
  data.SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::
          kAllowedByOptimizationFilter);
  data.RecordMetrics();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(optimization_guide::OptimizationTypeDecision::
                           kAllowedByOptimizationFilter),
      1);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsMultipleOptimizationTargets) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget::kPainfulPageLoad,
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches);
  data.SetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget::kUnknown,
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch);
  data.RecordMetrics();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      static_cast<int>(
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.Unknown",
      static_cast<int>(optimization_guide::OptimizationTargetDecision::
                           kPageLoadDoesNotMatch),
      1);
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsRecordsLatestTarget) {
  base::HistogramTester histogram_tester;

  OptimizationGuideNavigationData data(/*navigation_id=*/3);
  data.SetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget::kPainfulPageLoad,
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch);
  data.SetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget::kPainfulPageLoad,
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches);
  data.RecordMetrics();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      static_cast<int>(
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches),
      1);
}

TEST(OptimizationGuideNavigationDataTest, DeepCopy) {
  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  EXPECT_EQ(3, data->navigation_id());
  EXPECT_EQ(base::nullopt, data->serialized_hint_version_string());
  EXPECT_EQ(base::nullopt, data->GetDecisionForOptimizationType(
                               optimization_guide::proto::NOSCRIPT));
  EXPECT_EQ(base::nullopt,
            data->GetDecisionForOptimizationTarget(
                optimization_guide::OptimizationTarget::kPainfulPageLoad));

  data->set_serialized_hint_version_string("123abc");
  data->SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::kAllowedByHint);
  data->SetDecisionForOptimizationTarget(
      optimization_guide::OptimizationTarget::kPainfulPageLoad,
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches);

  OptimizationGuideNavigationData data_copy(*data);
  EXPECT_EQ(3, data_copy.navigation_id());
  EXPECT_EQ("123abc", *(data_copy.serialized_hint_version_string()));
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            *data_copy.GetDecisionForOptimizationType(
                optimization_guide::proto::NOSCRIPT));
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            *data_copy.GetDecisionForOptimizationTarget(
                optimization_guide::OptimizationTarget::kPainfulPageLoad));
}
