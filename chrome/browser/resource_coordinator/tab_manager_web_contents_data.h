// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_WEB_CONTENTS_DATA_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_WEB_CONTENTS_DATA_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {

// Tabs (WebContentsData) start in the not loading state, and transition to the
// loading state when a navigation begins in the main frame of the associated
// WebContents. The state changes to loaded when we receive the DidStopLoading*
// signal. The state can change from loaded to loading if another navigation
// occurs in the main frame, which happens if the user navigates to a new page
// and the WebContents is reused.
//
// These values are used in the TabManager.SessionRestore.SwitchToTab UMA.
//
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum TabLoadingState {
  TAB_IS_NOT_LOADING = 0,
  TAB_IS_LOADING = 1,
  // A tab is considered loaded when DidStopLoading is called from WebContents
  // for now. We are in the progress to deprecate using it, and use
  // PageAlmostIdle signal from resource coordinator instead.
  TAB_IS_LOADED = 2,
  TAB_LOADING_STATE_MAX,
};

// Internal class used by TabManager to record the needed data for
// WebContentses.
class TabManager::WebContentsData
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TabManager::WebContentsData> {
 public:
  explicit WebContentsData(content::WebContents* web_contents);
  ~WebContentsData() override;

  // WebContentsObserver implementation:
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WasShown() override;
  void WebContentsDestroyed() override;

  // Called by TabManager::ResourceCoordinatorSignalObserver to notify that a
  // tab is considered loaded.
  void NotifyTabIsLoaded();

  // Returns true if the tab has been discarded to save memory.
  bool IsDiscarded();

  // Sets/clears the discard state of the tab.
  void SetDiscardState(bool is_discarded);

  // Returns the number of times the tab has been discarded.
  int DiscardCount();

  // Increments the number of times the tab has been discarded.
  void IncrementDiscardCount();

  // Returns true if audio has recently been audible.
  bool IsRecentlyAudible();

  // Set/clears the state of whether audio has recently been audible.
  void SetRecentlyAudible(bool state);

  // Returns the timestamp of the last time the tab changed its audio state.
  base::TimeTicks LastAudioChangeTime();

  // Sets the timestamp of the last time the tab changed its audio state.
  void SetLastAudioChangeTime(base::TimeTicks timestamp);

  // Returns the timestamp of the last time the tab changed became inactive.
  base::TimeTicks LastInactiveTime();

  // Sets the timestamp of the last time the tab became inactive.
  void SetLastInactiveTime(base::TimeTicks timestamp);

  // Copies the discard state from |old_contents| to |new_contents|.
  static void CopyState(content::WebContents* old_contents,
                        content::WebContents* new_contents);

  // Returns the auto-discardable state of the tab.
  // See tab_manager.h for more information.
  bool IsAutoDiscardable();

  // Sets/clears the auto-discardable state of the tab.
  void SetAutoDiscardableState(bool state);

  // Sets the current purge state.
  // TODO(tasak): remove this after the logic is moved into
  // MemoryCoordinator.
  void set_is_purged(bool state) { is_purged_ = state; }

  // Returns the current state of purge.
  // TODO(tasak): remove this after the logic is moved into
  // MemoryCoordinator.
  bool is_purged() const { return is_purged_; }

  // Sets the time to purge after the tab is backgrounded.
  void set_time_to_purge(const base::TimeDelta& time_to_purge) {
    time_to_purge_ = time_to_purge;
  }

  // Returns the time to first purge after the tab is backgrounded.
  base::TimeDelta time_to_purge() const { return time_to_purge_; }

  // Sets the tab loading state.
  void SetTabLoadingState(TabLoadingState state) {
    tab_data_.tab_loading_state = state;
  }

  // Returns the TabLoadingState of the tab.
  TabLoadingState tab_loading_state() const {
    return tab_data_.tab_loading_state;
  }

  void SetIsInSessionRestore(bool is_in_session_restore) {
    tab_data_.is_in_session_restore = is_in_session_restore;
  }

  bool is_in_session_restore() const { return tab_data_.is_in_session_restore; }

  void SetIsRestoredInForeground(bool is_restored_in_foreground) {
    tab_data_.is_restored_in_foreground = is_restored_in_foreground;
  }

  bool is_restored_in_foreground() const {
    return tab_data_.is_restored_in_foreground;
  }

  int32_t id() const { return tab_data_.id; }

 private:
  // Needed to access tab_data_.
  FRIEND_TEST_ALL_PREFIXES(TabManagerWebContentsDataTest, CopyState);
  FRIEND_TEST_ALL_PREFIXES(TabManagerWebContentsDataTest, TabLoadingState);

  struct Data {
    Data();
    bool operator==(const Data& right) const;
    bool operator!=(const Data& right) const;

    // Unique ID associated with this tab. This stays constant through discards
    // and reloads, and is independent of the underlying WebContents and
    // TabStripModel index, both of which may change.
    int32_t id;
    // Is the tab currently discarded?
    bool is_discarded;
    // Number of times the tab has been discarded.
    int discard_count;
    // Is the tab playing audio?
    bool is_recently_audible;
    // The navigation time associated with this tab. Useful as a reference time
    // from which to measure UKM event timings.
    base::TimeTicks navigation_time;
    // Last time the tab started or stopped playing audio (we record the
    // transition time).
    base::TimeTicks last_audio_change_time;
    // The last time the tab was discarded.
    base::TimeTicks last_discard_time;
    // The last time the tab was reloaded after being discarded.
    base::TimeTicks last_reload_time;
    // The last time the tab switched from being active to inactive.
    base::TimeTicks last_inactive_time;
    // Is tab eligible for auto discarding? Defaults to true.
    bool is_auto_discardable;
    // Current loading state of this tab.
    TabLoadingState tab_loading_state;
    // True if the tab was created by session restore. Remains true until the
    // end of the first navigation or the tab is closed.
    bool is_in_session_restore;
    // True if the tab was created by session restore and initially foreground.
    bool is_restored_in_foreground;
  };

  void ReportUKMWhenTabIsClosed();
  void ReportUKMWhenBackgroundTabIsClosedOrForegrounded(bool is_foregrounded);

  // Contains all the needed data for the tab.
  Data tab_data_;

  // The time to purge after the tab is backgrounded.
  base::TimeDelta time_to_purge_;

  // True if the tab has been purged.
  bool is_purged_;

  ukm::SourceId ukm_source_id_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsData);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_WEB_CONTENTS_DATA_H_
