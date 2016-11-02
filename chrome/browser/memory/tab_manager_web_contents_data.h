// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_
#define CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/memory/tab_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class TickClock;
}

namespace content {
class WebContents;
}

namespace memory {

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
  void WebContentsDestroyed() override;

  // Returns true if the tab has been discarded to save memory.
  bool IsDiscarded();

  // Sets/clears the discard state of the tab.
  void SetDiscardState(bool state);

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

  // Used to set the test TickClock, which then gets used by NowTicks(). See
  // |test_tick_clock_| for more details.
  void set_test_tick_clock(base::TickClock* test_tick_clock);

  // Returns the auto-discardable state of the tab.
  // See tab_manager.h for more information.
  bool IsAutoDiscardable();

  // Sets/clears the auto-discardable state of the tab.
  void SetAutoDiscardableState(bool state);

  // Sets the current purge-and-suspend state, and update the timestamp,
  // i.e. the last purge-and-suspend modified time.
  // TODO(tasak): remove this after PurgeAndSuspend code is moved into
  // MemoryCoordinator.
  void SetPurgeAndSuspendState(TabManager::PurgeAndSuspendState state);

  // Returns the last purge-and-suspend modified time.
  // TODO(tasak): remove this after PurgeAndSuspend code is moved into
  // MemoryCoordinator.
  base::TimeTicks LastPurgeAndSuspendModifiedTime() const;

  // Sets the timestamp of the last modified time the purge-and-suspend state
  // of the tab was changed for testing.
  void SetLastPurgeAndSuspendModifiedTimeForTesting(base::TimeTicks timestamp);

  // Returns the current state of purge-and-suspend.
  // TODO(tasak): remove this after PurgeAndSuspend code is moved into
  // MemoryCoordinator.
  TabManager::PurgeAndSuspendState GetPurgeAndSuspendState() const;

 private:
  // Needed to access tab_data_.
  FRIEND_TEST_ALL_PREFIXES(TabManagerWebContentsDataTest, CopyState);

  struct Data {
    Data();
    bool operator==(const Data& right) const;
    bool operator!=(const Data& right) const;

    // TODO(georgesak): fix naming (no underscore).
    // Is the tab currently discarded?
    bool is_discarded_;
    // Number of times the tab has been discarded.
    int discard_count_;
    // Is the tab playing audio?
    bool is_recently_audible_;
    // Last time the tab started or stopped playing audio (we record the
    // transition time).
    base::TimeTicks last_audio_change_time_;
    // The last time the tab was discarded.
    base::TimeTicks last_discard_time_;
    // The last time the tab was reloaded after being discarded.
    base::TimeTicks last_reload_time_;
    // The last time the tab switched from being active to inactive.
    base::TimeTicks last_inactive_time_;
    // Site Engagement score (set to -1 if not available).
    double engagement_score_;
    // Is tab eligible for auto discarding? Defaults to true.
    bool is_auto_discardable;
  };

  // Returns either the system's clock or the test clock. See |test_tick_clock_|
  // for more details.
  base::TimeTicks NowTicks() const;

  // Contains all the needed data for the tab.
  Data tab_data_;

  // Pointer to a test clock. If this is set, NowTicks() returns the value of
  // this test clock. Otherwise it returns the system clock's value.
  base::TickClock* test_tick_clock_;

  // The last time purge-and-suspend state was modified.
  base::TimeTicks last_purge_and_suspend_modified_time_;

  // The current state of purge-and-suspend.
  PurgeAndSuspendState purge_and_suspend_state_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsData);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_TAB_MANAGER_WEB_CONTENTS_DATA_H_
