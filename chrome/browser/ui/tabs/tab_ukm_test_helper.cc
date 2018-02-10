// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"

#include "components/ukm/ukm_source.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Verifies each expected metric's value. Metrics not in |expected_metrics| are
// ignored. A metric value of |nullopt| implies the metric shouldn't exist.
void ExpectEntryMetrics(const ukm::mojom::UkmEntry& entry,
                        const UkmMetricMap& expected_metrics) {
  // Each expected metric should match a named value in the UKM entry.
  for (const UkmMetricMap::value_type pair : expected_metrics) {
    if (pair.second.has_value()) {
      ukm::TestUkmRecorder::ExpectEntryMetric(&entry, pair.first,
                                              pair.second.value());
    } else {
      // The metric shouldn't exist.
      EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(&entry, pair.first))
          << " for metric: " << pair.first;
    }
  }
}

}  // namespace

UkmEntryChecker::UkmEntryChecker() = default;
UkmEntryChecker::~UkmEntryChecker() = default;

void UkmEntryChecker::ExpectNewEntry(const std::string& entry_name,
                                     const GURL& source_url,
                                     const UkmMetricMap& expected_metrics) {
  num_entries_[entry_name]++;  // There should only be 1 more entry than before.
  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder_.GetEntriesByName(entry_name);
  EXPECT_EQ(num_entries_[entry_name], entries.size())
      << "Expected " << num_entries_[entry_name] << " entries, found "
      << entries.size() << " for " << entry_name;

  // Verify the entry is associated with the correct URL.
  ASSERT_FALSE(entries.empty());
  const ukm::mojom::UkmEntry* entry = entries.back();
  if (!source_url.is_empty())
    ukm_recorder_.ExpectEntrySourceHasUrl(entry, source_url);

  ExpectEntryMetrics(*entry, expected_metrics);
}

void UkmEntryChecker::ExpectNewEntries(
    const std::string& entry_name,
    const SourceUkmMetricMap& expected_data) {
  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder_.GetEntriesByName(entry_name);

  const size_t num_new_entries = expected_data.size();
  const size_t num_entries = entries.size();
  num_entries_[entry_name] += num_new_entries;

  EXPECT_EQ(NumEntries(entry_name), entries.size());
  std::set<ukm::SourceId> found_source_ids;

  for (size_t i = 0; i < num_new_entries; ++i) {
    const ukm::mojom::UkmEntry* entry =
        entries[num_entries - num_new_entries + i];
    const ukm::SourceId& source_id = entry->source_id;
    const auto& expected_data_for_id = expected_data.find(source_id);
    EXPECT_TRUE(expected_data_for_id != expected_data.end());
    EXPECT_EQ(0u, found_source_ids.count(source_id));

    found_source_ids.insert(source_id);
    const std::pair<GURL, UkmMetricMap>& expected_url_metrics =
        expected_data_for_id->second;

    const GURL& source_url = expected_url_metrics.first;
    const UkmMetricMap& expected_metrics = expected_url_metrics.second;
    if (!source_url.is_empty())
      ukm_recorder_.ExpectEntrySourceHasUrl(entry, source_url);

    // Each expected metric should match a named value in the UKM entry.
    ExpectEntryMetrics(*entry, expected_metrics);
  }
}

int UkmEntryChecker::NumNewEntriesRecorded(
    const std::string& entry_name) const {
  const size_t current_ukm_entries =
      ukm_recorder_.GetEntriesByName(entry_name).size();
  const size_t previous_num_entries = NumEntries(entry_name);
  CHECK(current_ukm_entries >= previous_num_entries);
  return current_ukm_entries - previous_num_entries;
}

size_t UkmEntryChecker::NumEntries(const std::string& entry_name) const {
  const auto it = num_entries_.find(entry_name);
  return it != num_entries_.end() ? it->second : 0;
}

const ukm::mojom::UkmEntry* UkmEntryChecker::LastUkmEntry(
    const std::string& entry_name) const {
  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder_.GetEntriesByName(entry_name);
  CHECK(!entries.empty());
  return entries.back();
}

ukm::SourceId UkmEntryChecker::GetSourceIdForUrl(const GURL& source_url) const {
  const ukm::UkmSource* source = ukm_recorder_.GetSourceForUrl(source_url);
  CHECK(source);
  return source->id();
}
