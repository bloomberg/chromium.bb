// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"

#include "chrome/test/base/testing_profile.h"
#include "components/ukm/ukm_source.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"

using content::WebContentsTester;

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

// Helper class to respond to WebContents lifecycle events we can't
// trigger/simulate.
class TestWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit TestWebContentsObserver(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

TestWebContentsObserver::TestWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void TestWebContentsObserver::WebContentsDestroyed() {
  // Simulate the WebContents hiding during destruction. This lets tests
  // validate what is logged when a tab is destroyed.
  web_contents()->WasHidden();
}

TabActivityTestBase::TabActivityTestBase() = default;
TabActivityTestBase::~TabActivityTestBase() = default;

void TabActivityTestBase::Navigate(
    content::WebContents* web_contents,
    const GURL& url,
    ui::PageTransition page_transition = ui::PAGE_TRANSITION_LINK) {
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  navigation->SetTransition(page_transition);
  navigation->Commit();
}

content::WebContents* TabActivityTestBase::AddWebContentsAndNavigate(
    TabStripModel* tab_strip_model,
    const GURL& initial_url,
    ui::PageTransition page_transition) {
  content::WebContents::CreateParams params(profile(), nullptr);
  // Create as a background tab if there are other tabs in the tab strip.
  params.initially_hidden = tab_strip_model->count() > 0;
  content::WebContents* test_contents =
      WebContentsTester::CreateTestWebContents(params);

  // Create the TestWebContentsObserver to observe |test_contents|. When the
  // WebContents is destroyed, the observer will be reset automatically.
  observers_.push_back(
      std::make_unique<TestWebContentsObserver>(test_contents));

  tab_strip_model->AppendWebContents(test_contents, false);
  Navigate(test_contents, initial_url, page_transition);
  return test_contents;
}

void TabActivityTestBase::SwitchToTabAt(TabStripModel* tab_strip_model,
                                        int new_index) {
  int active_index = tab_strip_model->active_index();
  EXPECT_NE(new_index, active_index);

  content::WebContents* active_contents =
      tab_strip_model->GetWebContentsAt(active_index);
  ASSERT_TRUE(active_contents);
  content::WebContents* new_contents =
      tab_strip_model->GetWebContentsAt(new_index);
  ASSERT_TRUE(new_contents);

  // Activate the tab. Normally this would hide the active tab's aura::Window,
  // which is what actually triggers TabActivityWatcher to log the change. For
  // a TestWebContents, we must manually call WasHidden(), and do the reverse
  // for the newly activated tab.
  tab_strip_model->ActivateTabAt(new_index, true /* user_gesture */);
  active_contents->WasHidden();
  new_contents->WasShown();
}

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
