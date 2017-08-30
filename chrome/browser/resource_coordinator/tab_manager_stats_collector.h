// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_STATS_COLLECTOR_H_

#include <cstdint>
#include <memory>
#include <string>
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
// system-related events and properties during session restore or background tab
// opening session.
class TabManagerStatsCollector final : public SessionRestoreObserver {
 public:
  enum SessionType {
    // SessionRestore is the duration from the time when the browser starts to
    // restore tabs until the time when the browser has finished loading those
    // tabs or when the browser stops loading tabs due to memory pressure.
    // During SessionRestore, some other tabs could be open due to user action,
    // but that would not affect the session end time point. For example, a
    // browser starts to restore tab1 and tab2. Tab3 is open due to user
    // clicking a link in tab1. SessionRestore ends after tab1 and tab2 finishes
    // loading, even if tab3 is still loading.
    kSessionRestore,

    // BackgroundTabOpening session is the duration from the time when the
    // browser starts to open background tabs until the time when browser has
    // finished loading those tabs or paused loading due to memory pressure.
    // A new session starts when the browser resumes loading background tabs
    // when memory pressure returns to normal. During the BackgroundTabOpening
    // session, some background tabs could become foreground due to user action,
    // but that would not affect the session end time point. For example, a
    // browser has tab1 in foreground, and starts to open tab2 and tab3 in
    // background. The session will end after tab2 and tab3 finishes loading,
    // even if tab2 and tab3 are brought to foreground before they are loaded.
    // BackgroundTabOpening session excludes the background tabs being
    // controlled by SessionRestore, so that those two activities are clearly
    // separated.
    kBackgroundTabOpening
  };

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

  // The following two functions defines the start and end of a background tab
  // opening session.
  void OnBackgroundTabOpeningSessionStarted();
  void OnBackgroundTabOpeningSessionEnded();

  // Record UMA histograms for system swap metrics.
  void RecordSwapMetrics(SessionType type,
                         const std::string& metric_name,
                         uint64_t count,
                         const base::TimeDelta& interval);

  // Handles the situation when failing to update swap metrics.
  void OnUpdateSwapMetricsFailed();

  // Called by WebContentsData when a tab starts loading. Used to clean up
  // |foreground_contents_switched_to_times_| if we were tracking this tab and
  // OnDidStopLoading has not yet been called for it, which will happen if the
  // user navigates to a new page and |contents| is resused.
  void OnDidStartMainFrameNavigation(content::WebContents* contents);

  // Called by TabManager when a tab finishes loading. Used as the signal to
  // record tab switch load time metrics for |contents|.
  void OnDidStopLoading(content::WebContents* contents);

  // Called by TabManager when a WebContents is destroyed. Used to clean up
  // |foreground_contents_switched_to_times_| if we were tracking this tab and
  // OnDidStopLoading has not yet been called for it.
  void OnWebContentsDestroyed(content::WebContents* contents);

  bool is_in_background_tab_opening_session() const {
    return is_in_background_tab_opening_session_;
  }

 private:
  class SwapMetricsDelegate;
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTest,
                           HistogramsExpectedTaskQueueingDuration);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTabSwitchTest,
                           HistogramsSwitchToTab);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTabSwitchTest,
                           HistogramsTabSwitchLoadTime);

  // Returns true if the browser is currently in more than one session with
  // different types. We do not want to report metrics in this situation to have
  // meaningful results. For example, SessionRestore and BackgroundTabOpening
  // session could overlap.
  bool IsInOverlappedSession();

  // This is called when we enter overlapped session. We need to clear all stats
  // in such situation to avoid reporting mixed metric results.
  void ClearStatsWhenInOverlappedSession();

  // Create and initialize the swap metrics driver if needed. This will set the
  // driver to null when it is in overlapped session situation, so that we do
  // not report swap metrics.
  void CreateAndInitSwapMetricsDriverIfNeeded(SessionType type);

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

  // This is shared between SessionRestore and BackgroundTabOpening because we
  // do not report metrics when those two overlap.
  std::unique_ptr<content::SwapMetricsDriver> swap_metrics_driver_;

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
