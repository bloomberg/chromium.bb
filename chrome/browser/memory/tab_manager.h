// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_TAB_MANAGER_H_
#define CHROME_BROWSER_MEMORY_TAB_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/memory/tab_manager_observer.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserList;
class GURL;
class TabStripModel;

namespace base {
class TickClock;
}

namespace content {
class WebContents;
}

namespace memory {

#if defined(OS_CHROMEOS)
class TabManagerDelegate;
#endif

// The TabManager periodically updates (see
// |kAdjustmentIntervalSeconds| in the source) the status of renderers
// which are then used by the algorithm embedded here for priority in being
// killed upon OOM conditions.
//
// The algorithm used favors killing tabs that are not selected, not pinned,
// and have been idle for longest, in that order of priority.
//
// On Chrome OS (via the delegate), the kernel (via /proc/<pid>/oom_score_adj)
// will be informed of each renderer's score, which is based on the status, so
// in case Chrome is not able to relieve the pressure quickly enough and the
// kernel is forced to kill processes, it will be able to do so using the same
// algorithm as the one used here.
//
// Note that the browser tests are only active for platforms that use
// TabManager (CrOS only for now) and need to be adjusted accordingly if
// support for new platforms is added.
class TabManager : public TabStripModelObserver {
 public:
  // Needs to be public for DEFINE_WEB_CONTENTS_USER_DATA_KEY.
  class WebContentsData;

  TabManager();
  ~TabManager() override;

  // Number of discard events since Chrome started.
  int discard_count() const { return discard_count_; }

  // See member comment.
  bool recent_tab_discard() const { return recent_tab_discard_; }

  // Start/Stop the Tab Manager.
  void Start();
  void Stop();

  // Returns the list of the stats for all renderers. Must be called on the UI
  // thread. The returned list is sorted by reversed importance.
  TabStatsList GetTabStats() const;

  // Returns true if |contents| is currently discarded.
  bool IsTabDiscarded(content::WebContents* contents) const;

  // Goes through a list of checks to see if a tab is allowed to be discarded by
  // the automatic tab discarding mechanism. Note that this is not used when
  // discarding a particular tab from about:discards.
  bool CanDiscardTab(int64_t target_web_contents_id) const;

  // Discards a tab to free the memory occupied by its renderer. The tab still
  // exists in the tab-strip; clicking on it will reload it.
  void DiscardTab();

  // Discards a tab with the given unique ID. The tab still exists in the
  // tab-strip; clicking on it will reload it. Returns null if the tab cannot
  // be found or cannot be discarded. Otherwise returns the new web_contents
  // of the discarded tab.
  content::WebContents* DiscardTabById(int64_t target_web_contents_id);

  // Method used by the extensions API to discard tabs. If |contents| is null,
  // discards the least important tab using DiscardTab(). Otherwise discards
  // the given contents. Returns the new web_contents or null if no tab
  // was discarded.
  content::WebContents* DiscardTabByExtension(content::WebContents* contents);

  // Log memory statistics for the running processes, then discards a tab.
  // Tab discard happens sometime later, as collecting the statistics touches
  // multiple threads and takes time.
  void LogMemoryAndDiscardTab();

  // Log memory statistics for the running processes, then call the callback.
  void LogMemory(const std::string& title, const base::Closure& callback);

  // Used to set the test TickClock, which then gets used by NowTicks(). See
  // |test_tick_clock_| for more details.
  void set_test_tick_clock(base::TickClock* test_tick_clock);

  // Returns the list of the stats for all renderers. Must be called on the UI
  // thread.
  TabStatsList GetUnsortedTabStats() const;

  void AddObserver(TabManagerObserver* observer);
  void RemoveObserver(TabManagerObserver* observer);

  // Used in tests to change the protection time of the tabs.
  void set_minimum_protection_time_for_tests(
      base::TimeDelta minimum_protection_time);

  // Returns the auto-discardable state of the tab. When true, the tab is
  // eligible to be automatically discarded when critical memory pressure hits,
  // otherwise the tab is ignored and will never be automatically discarded.
  // Note that this property doesn't block the discarding of the tab via other
  // methods (about:discards for instance).
  bool IsTabAutoDiscardable(content::WebContents* contents) const;

  // Sets/clears the auto-discardable state of the tab.
  void SetTabAutoDiscardableState(content::WebContents* contents, bool state);

  // Returns true when a given renderer can be purged if the specified
  // renderer is eligible for purging.
  // TODO(tasak): rename this to CanPurgeBackgroundedRenderer.
  bool CanSuspendBackgroundedRenderer(int render_process_id) const;

  // Returns true if |first| is considered less desirable to be killed than
  // |second|.
  static bool CompareTabStats(const TabStats& first, const TabStats& second);

  // Returns a unique ID for a WebContents. Do not cast back to a pointer, as
  // the WebContents could be deleted if the user closed the tab.
  static int64_t IdFromWebContents(content::WebContents* web_contents);

 private:
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, PurgeBackgroundRenderer);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ActivateTabResetPurgeState);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ShouldPurgeAtDefaultTime);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, DefaultTimeToPurgeInCorrectRange);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, AutoDiscardable);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, CanOnlyDiscardOnce);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ChildProcessNotifications);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, Comparator);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, DiscardedTabKeepsLastActiveTime);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, DiscardWebContentsAt);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, InvalidOrEmptyURL);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, IsInternalPage);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OomPressureListener);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ProtectPDFPages);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ProtectRecentlyUsedTabs);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ProtectVideoTabs);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ReloadDiscardedTabContextMenu);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, TabManagerBasics);

  // The time of the first purging after a renderer is backgrounded.
  // The initial value was chosen because most of users activate backgrounded
  // tabs within 30 minutes. (c.f. Tabs.StateTransfer.Time_Inactive_Active)
  static constexpr base::TimeDelta kDefaultMinTimeToPurge =
      base::TimeDelta::FromMinutes(30);

  // The min/max time to purge ratio. The max time to purge is set to be
  // min time to purge times this value.
  const int kMinMaxTimeToPurgeRatio = 2;

  // This is needed so WebContentsData can call OnDiscardedStateChange, and
  // can use PurgeState.
  friend class WebContentsData;

  // Finds TabStripModel which has a WebContents whose id is the given
  // web_contents_id, and returns the WebContents index and the TabStripModel.
  int FindTabStripModelById(int64_t target_web_contents_id,
                            TabStripModel** model) const;

  // Called by WebContentsData whenever the discard state of a WebContents
  // changes, so that observers can be informed.
  void OnDiscardedStateChange(content::WebContents* contents,
                              bool is_discarded);

  // Called by WebContentsData whenever the auto-discardable state of a
  // WebContents changes, so that observers can be informed.
  void OnAutoDiscardableStateChange(content::WebContents* contents,
                                    bool is_auto_discardable);

  static void PurgeMemoryAndDiscardTab();

  // Returns true if the |url| represents an internal Chrome web UI page that
  // can be easily reloaded and hence makes a good choice to discard.
  static bool IsInternalPage(const GURL& url);

  // Records UMA histogram statistics for a tab discard. Record statistics for
  // user triggered discards via chrome://discards/ because that allows to
  // manually test the system.
  void RecordDiscardStatistics();

  // Record whether an out of memory occured during a recent time interval. This
  // allows the normalization of low memory statistics versus usage.
  void RecordRecentTabDiscard();

  // Purges data structures in the browser that can be easily recomputed.
  void PurgeBrowserMemory();

  // Returns the number of tabs open in all browser instances.
  int GetTabCount() const;

  // Adds all the stats of the tabs to |stats_list|.
  void AddTabStats(TabStatsList* stats_list) const;

  // Adds all the stats of the tabs in |tab_strip_model| into |stats_list|.
  // If |active_model| is true, consider its first tab as being active.
  void AddTabStats(const TabStripModel* model,
                   bool is_app,
                   bool active_model,
                   TabStatsList* stats_list) const;

  // Callback for when |update_timer_| fires. Takes care of executing the tasks
  // that need to be run periodically (see comment in implementation).
  void UpdateTimerCallback();

  // Returns WebContents whose contents id matches the given tab_contents_id.
  content::WebContents* GetWebContentsById(int64_t tab_contents_id) const;

  // Returns a random time-to-purge whose min value is min_time_to_purge and max
  // value is min_time_to_purge * kMinMaxTimeToPurgeRatio.
  base::TimeDelta GetTimeToPurge(base::TimeDelta min_time_to_purge) const;

  // Returns true if the tab specified by |content| is now eligible to have
  // its memory purged.
  bool ShouldPurgeNow(content::WebContents* content) const;

  // Purges renderers in backgrounded tabs if the following conditions are
  // satisfied:
  // - the renderers are not purged yet,
  // - the renderers are not playing media,
  //   (CanPurgeBackgroundedRenderer returns true)
  // - the renderers are left inactive and background for time-to-purge.
  // If renderers are purged, their internal states become 'purged'.
  // The state is reset to be 'not purged' only when they are activated
  // (=ActiveTabChanged is invoked).
  void PurgeBackgroundedTabsIfNeeded();

  // Does the actual discard by destroying the WebContents in |model| at |index|
  // and replacing it by an empty one. Returns the new WebContents or NULL if
  // the operation fails (return value used only in testing).
  content::WebContents* DiscardWebContentsAt(int index, TabStripModel* model);

  // Called by the memory pressure listener when the memory pressure rises.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // TabStripModelObserver overrides.
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override;
  void TabInsertedAt(TabStripModel* tab_strip_model,
                     content::WebContents* contents,
                     int index,
                     bool foreground) override;

  // Returns true if the tab is currently playing audio or has played audio
  // recently, or if the tab is currently accessing the camera, microphone or
  // mirroring the display.
  bool IsMediaTab(content::WebContents* contents) const;

  // Returns the WebContentsData associated with |contents|. Also takes care of
  // creating one if needed.
  WebContentsData* GetWebContentsData(content::WebContents* contents) const;

  // Returns either the system's clock or the test clock. See |test_tick_clock_|
  // for more details.
  base::TimeTicks NowTicks() const;

  // Implementation of DiscardTab. Returns null if no tab was discarded.
  // Otherwise returns the new web_contents of the discarded tab.
  content::WebContents* DiscardTabImpl();

  // Returns true if tabs can be discarded only once.
  bool CanOnlyDiscardOnce() const;

  // Timer to periodically update the stats of the renderers.
  base::RepeatingTimer update_timer_;

  // Timer to periodically report whether a tab has been discarded since the
  // last time the timer has fired.
  base::RepeatingTimer recent_tab_discard_timer_;

  // A listener to global memory pressure events.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Wall-clock time when the priority manager started running.
  base::TimeTicks start_time_;

  // Wall-clock time of last tab discard during this browsing session, or 0 if
  // no discard has happened yet.
  base::TimeTicks last_discard_time_;

  // Wall-clock time of last priority adjustment, used to correct the above
  // times for discontinuities caused by suspend/resume.
  base::TimeTicks last_adjust_time_;

  // Number of times a tab has been discarded, for statistics.
  int discard_count_;

  // Whether a tab discard event has occurred during the last time interval,
  // used for statistics normalized by usage.
  bool recent_tab_discard_;

  // Whether a tab can only ever discarded once.
  bool discard_once_;

  // This allows protecting tabs for a certain amount of time after being
  // backgrounded.
  base::TimeDelta minimum_protection_time_;

  // A backgrounded renderer will be purged when this time passes.
  base::TimeDelta min_time_to_purge_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<TabManagerDelegate> delegate_;
#endif

  // Responsible for automatically registering this class as an observer of all
  // TabStripModels. Automatically tracks browsers as they come and go.
  BrowserTabStripTracker browser_tab_strip_tracker_;

  // Pointer to a test clock. If this is set, NowTicks() returns the value of
  // this test clock. Otherwise it returns the system clock's value.
  base::TickClock* test_tick_clock_;

  // Injected tab strip models. Allows this to be tested end-to-end without
  // requiring a full browser environment. If specified these tab strips will be
  // crawled as the authoritative source of tabs, otherwise the BrowserList and
  // associated Browser objects are crawled. The first of these is considered to
  // be the 'active' tab strip model.
  // TODO(chrisha): Factor out tab-strip model enumeration to a helper class,
  //     and make a delegate that centralizes all testing seams.
  using TestTabStripModel = std::pair<const TabStripModel*, bool>;
  std::vector<TestTabStripModel> test_tab_strip_models_;

  // List of observers that will receive notifications on state changes.
  base::ObserverList<TabManagerObserver> observers_;

  // Weak pointer factory used for posting delayed tasks.
  base::WeakPtrFactory<TabManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabManager);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_TAB_MANAGER_H_
