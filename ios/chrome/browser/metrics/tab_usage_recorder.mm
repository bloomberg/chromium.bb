// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/tab_usage_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

// The histogram recording the state of the tab the user switches to.
const char kSelectedTabHistogramName[] =
    "Tab.StatusWhenSwitchedBackToForeground";

// The histogram to record the number of page loads before an evicted tab is
// selected.
const char kPageLoadsBeforeEvictedTabSelected[] =
    "Tab.PageLoadsSinceLastSwitchToEvictedTab";

// Records the time it takes for an evicted tab to reload.
const char kEvictedTabReloadTime[] = "Tab.RestoreTime";

// Records success vs failure of an evicted tab's reload.
const char kEvictedTabReloadSuccessRate[] = "Tab.RestoreResult";

// Records whether or not the user switched tabs before an evicted tab finished
// reloading.
const char kDidUserWaitForEvictedTabReload[] = "Tab.RestoreUserPersistence";

// The name of the histogram that records time intervals between tab restores.
const char kTimeBetweenRestores[] = "Tabs.TimeBetweenRestores";

// The name of the histogram that records time intervals since the last restore.
const char kTimeAfterLastRestore[] = "Tabs.TimeAfterLastRestore";

// Name of histogram to record whether a memory warning had been recently
// received when a renderer termination occurred.
const char kRendererTerminationSawMemoryWarning[] =
    "Tab.RendererTermination.RecentlyReceivedMemoryWarning";

// Name of histogram to record the number of alive renderers when a renderer
// termination is received.
const char kRendererTerminationAliveRenderers[] =
    "Tab.RendererTermination.AliveRenderersCount";

// Name of histogram to record the number of renderers that were alive shortly
// before a renderer termination. This metric is being recorded in case the OS
// kills renderers in batches.
const char kRendererTerminationRecentlyAliveRenderers[] =
    "Tab.RendererTermination.RecentlyAliveRenderersCount";

// The recently alive renderer count metric counts all renderers that were alive
// x seconds before a renderer termination. |kSecondsBeforeRendererTermination|
// specifies x.
const int kSecondsBeforeRendererTermination = 2;

TabUsageRecorder::TabUsageRecorder(id<TabUsageRecorderDelegate> delegate)
    : page_loads_(0), evicted_tab_(NULL), recorder_delegate_(delegate) {
  restore_start_time_ = base::TimeTicks::Now();
}

TabUsageRecorder::~TabUsageRecorder() {}

void TabUsageRecorder::InitialRestoredTabs(Tab* active_tab, NSArray* tabs) {
#if !defined(NDEBUG)
  // Debugging check to ensure this is called at most once per run.
  // Specifically, this function is called in either of two cases:
  // 1. For a normal (not post-crash launch), during the tab model's creation.
  // It assumes that the tab model will not be deleted and recreated during the
  // application's lifecycle even if the app is backgrounded/foregrounded.
  // 2. For a post-crash launch, when the session is restored.  In that case,
  // the tab model will not have been created with existing tabs, so this
  // function will not have been called during its creation.
  static bool kColdStartTabsRecorded = false;
  static dispatch_once_t once = 0;
  dispatch_once(&once, ^{
    DCHECK(kColdStartTabsRecorded == false);
    kColdStartTabsRecorded = true;
  });
#endif

  // Do not set eviction reason on active tab since it will be reloaded without
  // being processed as a switch to the foreground tab.
  for (Tab* tab in tabs) {
    if (tab != active_tab) {
      base::WeakNSObject<Tab> weak_tab(tab);
      evicted_tabs_[weak_tab] = EVICTED_DUE_TO_COLD_START;
    }
  }
}

void TabUsageRecorder::TabCreatedForSelection(Tab* tab) {
  tab_created_selected_ = tab;
}

void TabUsageRecorder::SetDelegate(id<TabUsageRecorderDelegate> delegate) {
  recorder_delegate_.reset(delegate);
}

void TabUsageRecorder::RecordTabSwitched(Tab* old_tab, Tab* new_tab) {
  // If a tab was created to be selected, and is selected shortly thereafter,
  // it should not add its state to the "kSelectedTabHistogramName" metric.
  // |tab_created_selected_| is reset at the first tab switch seen after it was
  // created, regardless of whether or not it was the tab selected.
  bool was_just_created = new_tab == tab_created_selected_;
  tab_created_selected_ = NULL;

  // Disregard reselecting the same tab, but only if the mode has not changed
  // since the last time this tab was selected.  I.e. going to incognito and
  // back to normal mode is an event we want to track, but simply going into
  // stack view and back out, without changing modes, isn't.
  if (new_tab == old_tab && new_tab != mode_switch_tab_)
    return;
  mode_switch_tab_ = NULL;

  // Disregard opening a new tab with no previous tab.
  if (!old_tab)
    return;

  // Before knowledge of the previous tab, |old_tab|, is lost, see if it is a
  // previously-evicted tab still reloading.  If it is, record that the
  // user did not wait for the evicted tab to finish reloading.
  if (old_tab == evicted_tab_ && old_tab != new_tab &&
      evicted_tab_reload_start_time_ != base::TimeTicks()) {
    UMA_HISTOGRAM_ENUMERATION(kDidUserWaitForEvictedTabReload,
                              USER_DID_NOT_WAIT, USER_BEHAVIOR_COUNT);
  }
  ResetEvictedTab();

  if (ShouldIgnoreTab(new_tab) || was_just_created)
    return;

  // Should never happen.  Keeping the check to ensure that the prerender logic
  // is never overlooked, should behavior at the tab_model level change.
  DCHECK(![new_tab isPrerenderTab]);

  TabStateWhenSelected tab_state = ExtractTabState(new_tab);
  if (tab_state != IN_MEMORY) {
    // Keep track of the current 'evicted' tab.
    evicted_tab_ = new_tab;
    evicted_tab_state_ = tab_state;
    UMA_HISTOGRAM_COUNTS(kPageLoadsBeforeEvictedTabSelected, page_loads_);
    ResetPageLoads();
  }

  UMA_HISTOGRAM_ENUMERATION(kSelectedTabHistogramName, tab_state,
                            TAB_STATE_COUNT);
}

void TabUsageRecorder::RecordPrimaryTabModelChange(BOOL primary_tab_model,
                                                   Tab* active_tab) {
  if (primary_tab_model) {
    // User just came back to this tab model, so record a tab selection even
    // though the current tab was reselected.
    if (mode_switch_tab_ == active_tab)
      RecordTabSwitched(active_tab, active_tab);
  } else {
    // Keep track of the selected tab when this tab model is moved to
    // background. This way when the tab model is moved to the foreground, and
    // the current tab reselected, it is handled as a tab selection rather than
    // a no-op.
    mode_switch_tab_ = active_tab;
  }
}

void TabUsageRecorder::RecordPageLoadStart(Tab* tab) {
  if (!ShouldIgnoreTab(tab)) {
    page_loads_++;
    if (![[tab webController] isViewAlive]) {
      // On the iPad, there is no notification that a tab is being re-selected
      // after changing modes.  This catches the case where the pre-incognito
      // selected tab is selected again when leaving incognito mode.
      if (mode_switch_tab_ == tab)
        RecordTabSwitched(tab, tab);
      if (evicted_tab_ == tab)
        RecordRestoreStartTime();
    }
  } else {
    // If there is a currently-evicted tab reloading, make sure it is recorded
    // that the user did not wait for it to load.
    if (evicted_tab_ && evicted_tab_reload_start_time_ != base::TimeTicks()) {
      UMA_HISTOGRAM_ENUMERATION(kDidUserWaitForEvictedTabReload,
                                USER_DID_NOT_WAIT, USER_BEHAVIOR_COUNT);
    }
    ResetEvictedTab();
  }
}

void TabUsageRecorder::RecordPageLoadDone(Tab* tab, bool success) {
  if (!tab)
    return;
  if (tab == evicted_tab_) {
    if (success) {
      LOCAL_HISTOGRAM_TIMES(
          kEvictedTabReloadTime,
          base::TimeTicks::Now() - evicted_tab_reload_start_time_);
    }
    UMA_HISTOGRAM_ENUMERATION(kEvictedTabReloadSuccessRate,
                              success ? LOAD_SUCCESS : LOAD_FAILURE,
                              LOAD_DONE_STATE_COUNT);

    UMA_HISTOGRAM_ENUMERATION(kDidUserWaitForEvictedTabReload, USER_WAITED,
                              USER_BEHAVIOR_COUNT);
    ResetEvictedTab();
  }
}

void TabUsageRecorder::RecordReload(Tab* tab) {
  if (!ShouldIgnoreTab(tab)) {
    page_loads_++;
  }
}

void TabUsageRecorder::RendererTerminated(Tab* terminated_tab, bool visible) {
  if (!visible) {
    DCHECK(!TabAlreadyEvicted(terminated_tab));
    base::WeakNSObject<Tab> weak_tab(terminated_tab);
    evicted_tabs_[weak_tab] = EVICTED_DUE_TO_RENDERER_TERMINATION;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  termination_timestamps_.push_back(now);

  // Log if a memory warning was seen recently.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  BOOL saw_memory_warning =
      [defaults boolForKey:previous_session_info_constants::
                               kDidSeeMemoryWarningShortlyBeforeTerminating];
  UMA_HISTOGRAM_BOOLEAN(kRendererTerminationSawMemoryWarning,
                        saw_memory_warning);

  // Log number of live tabs after the renderer termination. This count does not
  // include |terminated_tab_|.
  NSUInteger live_tabs_count = [recorder_delegate_ liveTabsCount];
  UMA_HISTOGRAM_COUNTS_100(kRendererTerminationAliveRenderers, live_tabs_count);

  // Clear |termination_timestamps_| of timestamps older than
  // |kSecondsBeforeRendererTermination| ago.
  base::TimeDelta seconds_before =
      base::TimeDelta::FromSeconds(kSecondsBeforeRendererTermination);
  base::TimeTicks timestamp_boundary = now - seconds_before;
  while (termination_timestamps_.front() < timestamp_boundary) {
    termination_timestamps_.pop_front();
  }

  // Log number of recently alive tabs, where recently alive is defined to mean
  // alive within the past |kSecondsBeforeRendererTermination|.
  NSUInteger recently_live_tabs_count =
      live_tabs_count + termination_timestamps_.size();
  UMA_HISTOGRAM_COUNTS_100(kRendererTerminationRecentlyAliveRenderers,
                           recently_live_tabs_count);
}

void TabUsageRecorder::AppDidEnterBackground() {
  base::TimeTicks time_now = base::TimeTicks::Now();
  LOCAL_HISTOGRAM_TIMES(kTimeAfterLastRestore, time_now - restore_start_time_);

  if (evicted_tab_ && evicted_tab_reload_start_time_ != base::TimeTicks()) {
    UMA_HISTOGRAM_ENUMERATION(kDidUserWaitForEvictedTabReload, USER_LEFT_CHROME,
                              USER_BEHAVIOR_COUNT);
    ResetEvictedTab();
  }
  ClearDeletedTabs();
}

void TabUsageRecorder::AppWillEnterForeground() {
  ClearDeletedTabs();
  restore_start_time_ = base::TimeTicks::Now();
}

void TabUsageRecorder::ResetPageLoads() {
  page_loads_ = 0;
}

int TabUsageRecorder::EvictedTabsMapSize() {
  return evicted_tabs_.size();
}

void TabUsageRecorder::ResetAll() {
  ResetEvictedTab();
  ResetPageLoads();
  evicted_tabs_.clear();
}

void TabUsageRecorder::ResetEvictedTab() {
  evicted_tab_ = NULL;
  evicted_tab_state_ = IN_MEMORY;
  evicted_tab_reload_start_time_ = base::TimeTicks();
}

bool TabUsageRecorder::ShouldIgnoreTab(Tab* tab) {
  // Do not count chrome:// urls to avoid data noise.  For example, if they were
  // counted, every new tab created would add noise to the page load count.
  web::NavigationItem* pending_item =
      tab.webState->GetNavigationManager()->GetPendingItem();
  if (pending_item)
    return pending_item->GetURL().SchemeIs(kChromeUIScheme);
  return tab.lastCommittedURL.SchemeIs(kChromeUIScheme);
}

bool TabUsageRecorder::TabAlreadyEvicted(Tab* tab) {
  base::WeakNSObject<Tab> weak_tab(tab);
  auto tab_item = evicted_tabs_.find(weak_tab);
  return tab_item != evicted_tabs_.end();
}

TabUsageRecorder::TabStateWhenSelected TabUsageRecorder::ExtractTabState(
    Tab* tab) {
  if ([[tab webController] isViewAlive])
    return IN_MEMORY;

  base::WeakNSObject<Tab> weak_tab(tab);
  auto tab_item = evicted_tabs_.find(weak_tab);
  if (tab_item != evicted_tabs_.end()) {
    TabStateWhenSelected tabState = tab_item->second;
    evicted_tabs_.erase(weak_tab);
    return tabState;
  }
  return EVICTED;
}

void TabUsageRecorder::RecordRestoreStartTime() {
  base::TimeTicks time_now = base::TimeTicks::Now();
  // Record the time delta since the last eviction reload was seen.
  LOCAL_HISTOGRAM_TIMES(kTimeBetweenRestores, time_now - restore_start_time_);
  restore_start_time_ = time_now;
  evicted_tab_reload_start_time_ = time_now;
}

void TabUsageRecorder::ClearDeletedTabs() {
  base::WeakNSObject<Tab> empty_tab(nil);
  auto tab_item = evicted_tabs_.find(empty_tab);
  while (tab_item != evicted_tabs_.end()) {
    evicted_tabs_.erase(empty_tab);
    tab_item = evicted_tabs_.find(empty_tab);
  }
}
