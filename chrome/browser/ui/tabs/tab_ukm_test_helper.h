// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UKM_TEST_HELPER_H_
#define CHROME_BROWSER_UI_TABS_TAB_UKM_TEST_HELPER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

// A UKM entry consists of named metrics with int64_t values. Use a map to
// specify expected metrics to test against an actual entry for tests.
// A value of |nullopt| implies a value shouldn't exist for the given metric
// name.
using UkmMetricMap = std::map<const char*, base::Optional<int64_t>>;
using SourceUkmMetricMap =
    std::map<ukm::SourceId, std::pair<GURL, UkmMetricMap>>;

class TestWebContentsObserver;

// Base class for tests that rely on logging tab activity. Inherits from
// ChromeRenderViewHostTestHarness to use TestWebContents and Profile.
class TabActivityTestBase : public ChromeRenderViewHostTestHarness {
 public:
  TabActivityTestBase();
  ~TabActivityTestBase() override;

  // Simulates a navigation to |url| using the given transition type.
  void Navigate(content::WebContents* web_contents,
                const GURL& url,
                ui::PageTransition page_transition);

  // Creates a new WebContents suitable for testing, adds it to the tab strip
  // and commits a navigation to |initial_url|. The WebContents is owned by the
  // TabStripModel, so its tab must be closed later, e.g. via CloseAllTabs().
  content::WebContents* AddWebContentsAndNavigate(
      TabStripModel* tab_strip_model,
      const GURL& initial_url,
      ui::PageTransition page_transition = ui::PAGE_TRANSITION_LINK);

  // Sets |new_index| as the active tab in its tab strip, hiding the previously
  // active tab.
  void SwitchToTabAt(TabStripModel* tab_strip_model, int new_index);

 private:
  // Owns the observers we've created.
  std::vector<std::unique_ptr<TestWebContentsObserver>> observers_;
  DISALLOW_COPY_AND_ASSIGN(TabActivityTestBase);
};

// Helper class to check entries have been logged as expected into UKM.
class UkmEntryChecker {
 public:
  UkmEntryChecker();
  ~UkmEntryChecker();

  // Expects that exactly one entry has been recorded for |entry_name|, and that
  // it matches the values and the given URL if |source_url| is none empty.
  // Use this function when there's exactly one entry logged when some event
  // happens.
  // This function increments |num_entries_[entry_name]| by 1 and checks
  // the new value of |num_entries_[entry_name]| is the same as number of
  // entries in ukm.
  void ExpectNewEntry(const std::string& entry_name,
                      const GURL& source_url,
                      const UkmMetricMap& expected_metrics);

  // Expects that |expected_data.size()| number of entries have been recorded
  // for |entry_name|. For each recorded entry (as identified by its source id),
  // checks the values and the given URL if url is none empty.
  // Use this function when there're multiple entries logged when an event
  // happens. We use source ids to identify multiple entries.
  // This function increments |num_entries_[entry_name]| by the size of
  // |expected_data| and checks the new value of |num_entries_[entry_name]| is
  // the same as number of entries in ukm.
  void ExpectNewEntries(const std::string& entry_name,
                        const SourceUkmMetricMap& expected_data);

  // Returns number of new entries that have been recorded for |entry_name|.
  // It is the difference between number of entries in ukm and that recorded by
  // |num_entries_|.
  int NumNewEntriesRecorded(const std::string& entry_name) const;

  // Returns number of entries for |entry_name|.
  size_t NumEntries(const std::string& entry_name) const;

  // Returns the last recorded entry for |entry_name|.
  const ukm::mojom::UkmEntry* LastUkmEntry(const std::string& entry_name) const;

  ukm::SourceId GetSourceIdForUrl(const GURL& source_url) const;

 private:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;

  // Keyed by entry name, and tracks the expected number of entries to ensure we
  // don't log duplicate or incorrect entries.
  // |num_entries_| records number of entries up to last time we call
  // ExpectNewEntry or ExpectNewEntries.
  std::map<std::string, size_t> num_entries_;

  DISALLOW_COPY_AND_ASSIGN(UkmEntryChecker);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_UKM_TEST_HELPER_H_
