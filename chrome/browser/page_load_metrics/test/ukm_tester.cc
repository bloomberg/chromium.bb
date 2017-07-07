// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/test/ukm_tester.h"

#include <algorithm>
#include <iterator>

#include "base/metrics/metrics_hashes.h"
#include "components/ukm/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Provides a single merged ukm::mojom::UkmEntry proto that contains all metrics
// from the given |entries|. |entries| must be non-empty, and all |entries| must
// have the same |source_id| and |event_hash|.
ukm::mojom::UkmEntryPtr GetMergedEntry(
    const std::vector<const ukm::mojom::UkmEntry*>& entries) {
  EXPECT_FALSE(entries.empty());
  ukm::mojom::UkmEntryPtr merged_entry = ukm::mojom::UkmEntry::New();
  for (const auto* entry : entries) {
    if (merged_entry->event_hash) {
      EXPECT_EQ(merged_entry->source_id, entry->source_id);
      EXPECT_EQ(merged_entry->event_hash, entry->event_hash);
    } else {
      merged_entry->event_hash = entry->event_hash;
      merged_entry->source_id = entry->source_id;
    }
    for (const auto& metric : entry->metrics) {
      merged_entry->metrics.emplace_back(metric->Clone());
    }
  }
  return merged_entry;
}

std::vector<int64_t> GetValuesForMetric(const ukm::mojom::UkmEntry* entry,
                                        const char* metric_name) {
  std::vector<int64_t> values;
  const uint64_t metric_hash = base::HashMetricName(metric_name);
  for (const auto& metric : entry->metrics) {
    if (metric->metric_hash == metric_hash)
      values.push_back(metric->value);
  }
  return values;
}

}  // namespace

namespace page_load_metrics {
namespace test {

UkmTester::UkmTester(ukm::TestUkmRecorder* test_ukm_recorder)
    : test_ukm_recorder_(test_ukm_recorder) {}

const ukm::UkmSource* UkmTester::GetSourceForUrl(const char* url) const {
  std::vector<const ukm::UkmSource*> matching_sources = GetSourcesForUrl(url);
  EXPECT_LE(1ul, matching_sources.size());
  return matching_sources.empty() ? nullptr : matching_sources.back();
}

std::vector<const ukm::UkmSource*> UkmTester::GetSourcesForUrl(
    const char* url) const {
  std::vector<const ukm::UkmSource*> matching_sources;
  for (const auto& candidate : test_ukm_recorder_->GetSources()) {
    if (candidate.second->url() == url)
      matching_sources.push_back(candidate.second.get());
  }
  return matching_sources;
}

std::vector<const ukm::mojom::UkmEntry*> UkmTester::GetEntriesForSourceID(
    ukm::SourceId source_id,
    const char* event_name) const {
  const uint64_t event_hash = base::HashMetricName(event_name);
  std::vector<const ukm::mojom::UkmEntry*> entries;
  for (size_t i = 0; i < entries_count(); ++i) {
    const ukm::mojom::UkmEntry* entry = test_ukm_recorder_->GetEntry(i);
    if (entry->source_id == source_id && entry->event_hash == event_hash) {
      entries.push_back(entry);
    }
  }
  return entries;
}

ukm::mojom::UkmEntryPtr UkmTester::GetMergedEntryForSourceID(
    ukm::SourceId source_id,
    const char* event_name) const {
  ukm::mojom::UkmEntryPtr entry =
      GetMergedEntry(GetEntriesForSourceID(source_id, event_name));
  EXPECT_EQ(source_id, entry->source_id);
  EXPECT_EQ(base::HashMetricName(event_name), entry->event_hash);
  return entry;
}

bool UkmTester::HasEntry(const ukm::UkmSource& source,
                         const char* event_name) const {
  return CountEntries(source, event_name) > 0;
}

int UkmTester::CountEntries(const ukm::UkmSource& source,
                            const char* event_name) const {
  return GetEntriesForSourceID(source.id(), event_name).size();
}

void UkmTester::ExpectEntry(const ukm::UkmSource& source,
                            const char* event_name,
                            const std::vector<std::pair<const char*, int64_t>>&
                                expected_metrics) const {
  // Produce a sorted view of the expected metrics, since order is not
  // significant.
  std::vector<std::pair<uint64_t, int64_t>> sorted_expected_metrics;
  std::transform(expected_metrics.begin(), expected_metrics.end(),
                 std::back_inserter(sorted_expected_metrics),
                 [](const std::pair<const char*, int64_t>& metric) {
                   return std::make_pair(base::HashMetricName(metric.first),
                                         metric.second);
                 });
  std::sort(sorted_expected_metrics.begin(), sorted_expected_metrics.end());

  std::vector<const ukm::mojom::UkmEntry*> candidate_entries =
      GetEntriesForSourceID(source.id(), event_name);

  // Produce a view of each matching entry's metrics that can be compared with
  // sorted_expected_metrics.
  std::vector<std::vector<std::pair<uint64_t, int64_t>>> candidate_metrics;
  std::transform(candidate_entries.begin(), candidate_entries.end(),
                 std::back_inserter(candidate_metrics),
                 [](const ukm::mojom::UkmEntry* candidate_entry) {
                   std::vector<std::pair<uint64_t, int64_t>> metrics;
                   std::transform(candidate_entry->metrics.begin(),
                                  candidate_entry->metrics.end(),
                                  std::back_inserter(metrics),
                                  [](const ukm::mojom::UkmMetricPtr& metric) {
                                    return std::make_pair(metric->metric_hash,
                                                          metric->value);
                                  });
                   std::sort(metrics.begin(), metrics.end());
                   return metrics;
                 });

  if (std::find(candidate_metrics.begin(), candidate_metrics.end(),
                sorted_expected_metrics) == candidate_metrics.end()) {
    FAIL() << "Failed to find metrics for event: " << event_name;
  }
}

int UkmTester::CountMetricsForEventName(const ukm::UkmSource& source,
                                        const char* event_name) const {
  ukm::mojom::UkmEntryPtr entry =
      GetMergedEntryForSourceID(source.id(), event_name);
  return entry.get() ? entry->metrics.size() : 0;
}

bool UkmTester::HasMetric(const ukm::UkmSource& source,
                          const char* event_name,
                          const char* metric_name) const {
  return CountMetrics(source, event_name, metric_name) > 0;
}

int UkmTester::CountMetrics(const ukm::UkmSource& source,
                            const char* event_name,
                            const char* metric_name) const {
  ukm::mojom::UkmEntryPtr entry =
      GetMergedEntryForSourceID(source.id(), event_name);
  return GetValuesForMetric(entry.get(), metric_name).size();
}

void UkmTester::ExpectMetric(const ukm::UkmSource& source,
                             const char* event_name,
                             const char* metric_name,
                             int64_t expected_value) const {
  ExpectMetrics(source, event_name, metric_name, {expected_value});
}

void UkmTester::ExpectMetrics(
    const ukm::UkmSource& source,
    const char* event_name,
    const char* metric_name,
    const std::vector<int64_t>& expected_values) const {
  ukm::mojom::UkmEntryPtr entry =
      GetMergedEntryForSourceID(source.id(), event_name);

  // Make sure both vectors are sorted before comparing, since order is not
  // significant.
  std::vector<int64_t> sorted_expected_values(expected_values);
  std::sort(sorted_expected_values.begin(), sorted_expected_values.end());

  std::vector<int64_t> actual_values =
      GetValuesForMetric(entry.get(), metric_name);
  std::sort(actual_values.begin(), actual_values.end());

  EXPECT_EQ(actual_values, sorted_expected_values)
      << "Failed to find expected_values for metric: " << metric_name;
}

}  // namespace test
}  // namespace page_load_metrics
