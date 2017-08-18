// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_

#include <cstdint>
#include <memory>

#include "chrome/browser/sessions/session_restore_observer.h"

namespace base {
class TimeDelta;
}

namespace content {
class SwapMetricsDriver;
class WebContents;
}  // namespace content

namespace resource_coordinator {

class TabManager;

// TabManagerStatsCollector records UMAs on behalf of TabManager for tab and
// system-related events and properties during session restore.
class TabManagerStatsCollector : public SessionRestoreObserver {
 public:
  explicit TabManagerStatsCollector(TabManager* tab_manager);
  ~TabManagerStatsCollector();

  // Records UMA histograms for the tab state when switching to a different tab
  // during session restore.
  void RecordSwitchToTab(content::WebContents* contents) const;

  // Record expected task queueing durations of foreground tabs in session
  // restore.
  void RecordExpectedTaskQueueingDuration(content::WebContents* contents,
                                          base::TimeDelta queueing_time);

  // SessionRestoreObserver
  void OnSessionRestoreStartedLoadingTabs() override;
  void OnSessionRestoreFinishedLoadingTabs() override;

  // The following record UMA histograms for system swap metrics during session
  // restore.
  void OnSessionRestoreSwapInCount(uint64_t count, base::TimeDelta interval);
  void OnSessionRestoreSwapOutCount(uint64_t count, base::TimeDelta interval);
  void OnSessionRestoreDecompressedPageCount(uint64_t count,
                                             base::TimeDelta interval);
  void OnSessionRestoreCompressedPageCount(uint64_t count,
                                           base::TimeDelta interval);
  void OnSessionRestoreUpdateMetricsFailed();

 private:
  class SessionRestoreSwapMetricsDelegate;

  TabManager* tab_manager_;
  std::unique_ptr<content::SwapMetricsDriver>
      session_restore_swap_metrics_driver_;
  bool is_session_restore_loading_tabs_;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
