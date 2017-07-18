// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

class TabManager;

// TabManagerStatsCollector records UMAs on behalf of TabManager for tab and
// system-related events and properties during session restore.
class TabManagerStatsCollector {
 public:
  explicit TabManagerStatsCollector(TabManager* tab_manager);
  ~TabManagerStatsCollector();

  // Records UMA histograms for the tab state when switching to a different tab
  // during session restore.
  void RecordSwitchToTab(content::WebContents* contents) const;

 private:
  TabManager* tab_manager_;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
