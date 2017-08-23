// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/sessions/session_restore_observer.h"

namespace base {
class TimeDelta;
}

namespace content {
class SwapMetricsDriver;
class WebContents;
}  // namespace content

namespace resource_coordinator {

// TabManagerStatsCollector records UMAs on behalf of TabManager for tab and
// system-related events and properties during session restore.
class TabManagerStatsCollector : public SessionRestoreObserver {
 public:
  TabManagerStatsCollector();
  ~TabManagerStatsCollector();

  // Records UMA histograms for the tab state when switching to a different tab
  // during session restore.
  void RecordSwitchToTab(content::WebContents* old_contents,
                         content::WebContents* new_contents);

  // Record expected task queueing durations of foreground tabs in session
  // restore.
  void RecordExpectedTaskQueueingDuration(content::WebContents* contents,
                                          base::TimeDelta queueing_time);

  // SessionRestoreObserver
  void OnSessionRestoreStartedLoadingTabs() override;
  void OnSessionRestoreFinishedLoadingTabs() override;

  // The following two functions define the start and end of a background tab
  // opening session.
  void OnBackgroundTabOpeningSessionStarted();
  void OnBackgroundTabOpeningSessionEnded();

  // The following record UMA histograms for system swap metrics during session
  // restore.
  void OnSessionRestoreSwapInCount(uint64_t count, base::TimeDelta interval);
  void OnSessionRestoreSwapOutCount(uint64_t count, base::TimeDelta interval);
  void OnSessionRestoreDecompressedPageCount(uint64_t count,
                                             base::TimeDelta interval);
  void OnSessionRestoreCompressedPageCount(uint64_t count,
                                           base::TimeDelta interval);
  void OnSessionRestoreUpdateMetricsFailed();

  // Called by TabManager when a tab finishes loading. Used as the signal to
  // record tab switch load time metrics for |contents|.
  void OnDidStopLoading(content::WebContents* contents);

  // Called by TabManager when a WebContents is destroyed. Used to clean up
  // |foreground_contents_switched_to_times_| if we were tracking this tab and
  // OnDidStopLoading has not yet been called for it.
  void OnWebContentsDestroyed(content::WebContents* contents);

 private:
  class SessionRestoreSwapMetricsDelegate;
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTest,
                           HistogramsExpectedTaskQueueingDuration);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTabSwitchTest,
                           HistogramsSwitchToTab);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTabSwitchTest,
                           HistogramsTabSwitchLoadTime);

  static const char
      kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration[];
  static const char
      kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration[];
  static const char kHistogramSessionRestoreSwitchToTab[];
  static const char kHistogramBackgroundTabOpeningSwitchToTab[];
  static const char kHistogramSessionRestoreTabSwitchLoadTime[];
  static const char kHistogramBackgroundTabOpeningTabSwitchLoadTime[];

  bool is_session_restore_loading_tabs_;
  bool is_in_background_tab_opening_session_;

  std::unique_ptr<content::SwapMetricsDriver>
      session_restore_swap_metrics_driver_;
  // The set of foreground tabs during session restore or background tab opening
  // sessions that were switched to have not yet finished loading, mapped to the
  // time that they were switched to. It's possible to have multiple background
  // opening sessions or session restores happening simultaneously in different
  // windows, which means there can be multiple foreground tabs that have been
  // switched to that haven't finished loading. Because of that, we need to
  // track each foreground tab and its corresponding switch-to time.
  std::unordered_map<content::WebContents*, base::TimeTicks>
      foreground_contents_switched_to_times_;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
