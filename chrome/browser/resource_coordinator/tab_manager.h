// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
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
#include "chrome/browser/resource_coordinator/tab_manager_observer.h"
#include "chrome/browser/resource_coordinator/tab_stats.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/navigation_throttle.h"
#include "ui/gfx/native_widget_types.h"

class BrowserList;
class GURL;
class TabStripModel;

namespace base {
class TickClock;
}

namespace content {
class NavigationHandle;
class WebContents;
}

namespace resource_coordinator {

class BackgroundTabNavigationThrottle;

#if defined(OS_CHROMEOS)
class TabManagerDelegate;
#endif
class TabManagerStatsCollector;

// Information about a Browser.
struct BrowserInfo {
  Browser* browser = nullptr;  // Can be nullptr in tests.
  TabStripModel* tab_strip_model = nullptr;
  bool window_is_minimized = false;
  bool browser_is_app = false;
};

// The TabManager periodically updates (see
// |kAdjustmentIntervalSeconds| in the source) the status of renderers
// which are then used by the algorithm embedded here for priority in being
// killed upon OOM conditions.
//
// The algorithm used favors killing tabs that are not active, not in an active
// window, not in a visible window, not pinned, and have been idle for longest,
// in that order of priority.
//
// On Chrome OS (via the delegate), the kernel (via /proc/<pid>/oom_score_adj)
// will be informed of each renderer's score, which is based on the status, so
// in case Chrome is not able to relieve the pressure quickly enough and the
// kernel is forced to kill processes, it will be able to do so using the same
// algorithm as the one used here.
//
// The TabManager also delays background tabs' navigation when needed in order
// to improve users' experience with the foreground tab.
//
// Note that the browser tests are only active for platforms that use
// TabManager (CrOS only for now) and need to be adjusted accordingly if
// support for new platforms is added.
class TabManager : public TabStripModelObserver,
                   public chrome::BrowserListObserver {
 public:
  // Forward declaration of tab signal observer.
  class GRCTabSignalObserver;
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
  bool CanDiscardTab(const TabStats& tab_stats) const;

  // Indicates the criticality of the situation when attempting to discard a
  // tab.
  enum DiscardTabCondition { kProactiveShutdown, kUrgentShutdown };

  // Discards a tab to free the memory occupied by its renderer. The tab still
  // exists in the tab-strip; clicking on it will reload it. If the |condition|
  // is urgent, an aggressive fast-kill will be attempted if the sudden
  // termination disablers are allowed to be ignored (e.g. On ChromeOS, we can
  // ignore an unload handler and fast-kill the tab regardless).
  void DiscardTab(DiscardTabCondition condition);

  // Discards a tab with the given unique ID. The tab still exists in the
  // tab-strip; clicking on it will reload it. Returns null if the tab cannot
  // be found or cannot be discarded. Otherwise returns the new web_contents
  // of the discarded tab.
  content::WebContents* DiscardTabById(int64_t target_web_contents_id,
                                       DiscardTabCondition condition);

  // Method used by the extensions API to discard tabs. If |contents| is null,
  // discards the least important tab using DiscardTab(). Otherwise discards
  // the given contents. Returns the new web_contents or null if no tab
  // was discarded.
  content::WebContents* DiscardTabByExtension(content::WebContents* contents);

  // Log memory statistics for the running processes, then discards a tab.
  // Tab discard happens sometime later, as collecting the statistics touches
  // multiple threads and takes time.
  void LogMemoryAndDiscardTab(DiscardTabCondition condition);

  // Log memory statistics for the running processes, then call the callback.
  void LogMemory(const std::string& title, const base::Closure& callback);

  // Used to set the test TickClock, which then gets used by NowTicks(). See
  // |test_tick_clock_| for more details.
  void set_test_tick_clock(base::TickClock* test_tick_clock);

  // Returns TabStats for all tabs in the current Chrome instance. The tabs are
  // sorted first by most recently used to least recently used Browser and
  // second by index in the Browser. |windows_sorted_by_z_index| is a list of
  // Browser windows sorted by z-index, from topmost to bottommost. If left
  // empty, no window occlusion checks will be performed. Must be called on the
  // UI thread.
  TabStatsList GetUnsortedTabStats(
      const std::vector<gfx::NativeWindow>& windows_sorted_by_z_index =
          std::vector<gfx::NativeWindow>()) const;

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

  // Indicates how TabManager should load pending background tabs.
  enum BackgroundTabLoadingMode {
    kStaggered,  // Load a background tab after another tab has done loading.
    kPaused      // Pause loading background tabs unless the user selects it.
  };

  // Maybe throttle a tab's navigation based on current system status.
  content::NavigationThrottle::ThrottleCheckResult MaybeThrottleNavigation(
      BackgroundTabNavigationThrottle* throttle);

  // Notifies TabManager that one navigation has finished (committed, aborted or
  // replaced). TabManager should clean up the NavigationHandle objects bookkept
  // before.
  void OnDidFinishNavigation(content::NavigationHandle* navigation_handle);

  // Notifies TabManager that one tab has finished loading. TabManager can
  // decide which tab to load next.
  void OnDidStopLoading(content::WebContents* contents);

  // Notifies TabManager that one tab WebContents has been destroyed. TabManager
  // needs to clean up data related to that tab.
  void OnWebContentsDestroyed(content::WebContents* contents);

  // Returns true if |first| is considered less desirable to be killed than
  // |second|.
  static bool CompareTabStats(const TabStats& first, const TabStats& second);

  // Returns a unique ID for a WebContents. Do not cast back to a pointer, as
  // the WebContents could be deleted if the user closed the tab.
  static int64_t IdFromWebContents(content::WebContents* web_contents);

  // Return whether tabs are being loaded during session restore.
  bool IsSessionRestoreLoadingTabs() const {
    return is_session_restore_loading_tabs_;
  }

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
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           GetUnsortedTabStatsIsInVisibleWindow);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, DiscardTabWithNonVisibleTabs);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, MaybeThrottleNavigation);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OnDidFinishNavigation);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OnDidStopLoading);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OnWebContentsDestroyed);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OnDelayedTabSelected);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, TimeoutWhenLoadingBackgroundTabs);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, BackgroundTabLoadingMode);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, BackgroundTabLoadingSlots);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, BackgroundTabsLoadingOrdering);
  FRIEND_TEST_ALL_PREFIXES(TabManagerStatsCollectorTest,
                           HistogramsSessionRestoreSwitchToTab);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownSingleTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, UrgentFastShutdownSingleTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownSharedTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, UrgentFastShutdownSharedTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownWithUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, UrgentFastShutdownWithUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownWithBeforeunloadHandler);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           UrgentFastShutdownWithBeforeunloadHandler);

  // The time of the first purging after a renderer is backgrounded.
  // The initial value was chosen because most of users activate backgrounded
  // tabs within 30 minutes. (c.f. Tabs.StateTransfer.Time_Inactive_Active)
  static constexpr base::TimeDelta kDefaultMinTimeToPurge =
      base::TimeDelta::FromMinutes(30);

  // The min/max time to purge ratio. The max time to purge is set to be
  // min time to purge times this value.
  const int kDefaultMinMaxTimeToPurgeRatio = 2;

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

  static void PurgeMemoryAndDiscardTab(DiscardTabCondition condition);

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

  // Adds all the stats of the tabs in |browser_info| into |stats_list|.
  // |window_is_active| indicates whether |browser_info|'s window is active.
  // |window_is_visible| indicates whether |browser_info|'s window might be
  // visible (true when window visibility is unknown).
  void AddTabStats(const BrowserInfo& browser_info,
                   bool window_is_active,
                   bool window_is_visible,
                   TabStatsList* stats_list) const;

  // Callback for when |update_timer_| fires. Takes care of executing the tasks
  // that need to be run periodically (see comment in implementation).
  void UpdateTimerCallback();

  // Returns WebContents whose contents id matches the given tab_contents_id.
  content::WebContents* GetWebContentsById(int64_t tab_contents_id) const;

  // Returns a random time-to-purge whose min value is min_time_to_purge and max
  // value is max_time_to_purge.
  base::TimeDelta GetTimeToPurge(base::TimeDelta min_time_to_purge,
                                 base::TimeDelta max_time_to_purge) const;

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
  content::WebContents* DiscardWebContentsAt(int index,
                                             TabStripModel* model,
                                             DiscardTabCondition condition);

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
  void TabClosingAt(TabStripModel* tab_strip_model,
                    content::WebContents* contents,
                    int index) override;

  // BrowserListObserver overrides.
  void OnBrowserSetLastActive(Browser* browser) override;

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
  content::WebContents* DiscardTabImpl(DiscardTabCondition condition);

  // Returns true if tabs can be discarded only once.
  bool CanOnlyDiscardOnce() const;

  // Returns true if |web_contents| is the active WebContents in the last active
  // Browser.
  bool IsActiveWebContentsInActiveBrowser(content::WebContents* contents) const;

  // Returns a list of BrowserInfo constructed from either
  // |test_browser_info_list_| or BrowserList. The first BrowserInfo in the list
  // corresponds to the last active Browser.
  std::vector<BrowserInfo> GetBrowserInfoList() const;

  void OnSessionRestoreStartedLoadingTabs();
  void OnSessionRestoreFinishedLoadingTabs();

  // Returns true if TabManager can start loading next tab.
  bool CanLoadNextTab() const;

  // Start |force_load_timer_| to load the next background tab if the timer
  // expires before the current tab loading is finished.
  void StartForceLoadTimer();

  // Start loading the next background tab if needed. This is called when:
  // 1. a tab has finished loading;
  // 2. or a tab has been destroyed;
  // 3. or memory pressure is relieved;
  // 4. or |force_load_timer_| fires.
  void LoadNextBackgroundTabIfNeeded();

  // Resume the tab's navigation if it is pending right now. This is called when
  // a tab is selected.
  void ResumeTabNavigationIfNeeded(content::WebContents* contents);

  // Resume navigation.
  void ResumeNavigation(BackgroundTabNavigationThrottle* throttle);

  // Remove the pending navigation for the provided WebContents. Return the
  // removed NavigationThrottle. Return nullptr if it doesn't exists.
  BackgroundTabNavigationThrottle* RemovePendingNavigationIfNeeded(
      content::WebContents* contents);

  // Returns true if |first| is considered to resume navigation before |second|.
  static bool ComparePendingNavigations(
      const BackgroundTabNavigationThrottle* first,
      const BackgroundTabNavigationThrottle* second);

  // Check if the tab is loading. Use only in tests.
  bool IsTabLoadingForTest(content::WebContents* contents) const;

  // Check if the navigation is delayed. Use only in tests.
  bool IsNavigationDelayedForTest(
      const content::NavigationHandle* navigation_handle) const;

  // Trigger |force_load_timer_| to fire. Use only in tests.
  bool TriggerForceLoadTimerForTest();

  // Set |loading_slots_|. Use only in tests.
  void SetLoadingSlotsForTest(size_t loading_slots) {
    loading_slots_ = loading_slots;
  }

  // Reset |memory_pressure_listener_| in test so that the test is not affected
  // by memory pressure.
  void ResetMemoryPressureListenerForTest() {
    memory_pressure_listener_.reset();
  }

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

  // A backgrounded renderer will be purged between min_time_to_purge_ and
  // max_time_to_purge_.
  base::TimeDelta min_time_to_purge_;
  base::TimeDelta max_time_to_purge_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<TabManagerDelegate> delegate_;
#endif

  // Responsible for automatically registering this class as an observer of all
  // TabStripModels. Automatically tracks browsers as they come and go.
  BrowserTabStripTracker browser_tab_strip_tracker_;

  // Pointer to a test clock. If this is set, NowTicks() returns the value of
  // this test clock. Otherwise it returns the system clock's value.
  base::TickClock* test_tick_clock_;

  // Injected BrowserInfo list. Allows this to be tested end-to-end without
  // requiring a full browser environment. If specified these BrowserInfo will
  // be crawled as the authoritative source of tabs, otherwise the BrowserList
  // and associated Browser objects are crawled. The first BrowserInfo in the
  // list corresponds to the last active Browser.
  // TODO(chrisha): Factor out tab-strip model enumeration to a helper class,
  //     and make a delegate that centralizes all testing seams.
  std::vector<BrowserInfo> test_browser_info_list_;

  // List of observers that will receive notifications on state changes.
  base::ObserverList<TabManagerObserver> observers_;

  bool is_session_restore_loading_tabs_;

  class TabManagerSessionRestoreObserver;
  std::unique_ptr<TabManagerSessionRestoreObserver> session_restore_observer_;

  // The mode that TabManager is using to load pending background tabs.
  BackgroundTabLoadingMode background_tab_loading_mode_;

  // When the timer fires, it forces loading the next background tab if needed.
  std::unique_ptr<base::OneShotTimer> force_load_timer_;

  // The list of navigations that are delayed.
  std::vector<BackgroundTabNavigationThrottle*> pending_navigations_;

  // The tabs that are currently loading. We will consider loading the next
  // background tab when these tabs have finished loading or a background tab
  // is brought to foreground.
  std::set<content::WebContents*> loading_contents_;

  // The number of loading slots that TabManager can use to load background tabs
  // in parallel.
  size_t loading_slots_;

  // GRC tab signal observer, receives tab scoped signal from GRC.
  std::unique_ptr<GRCTabSignalObserver> grc_tab_signal_observer_;

  // Records UMAs for tab and system-related events and properties during
  // session restore.
  std::unique_ptr<TabManagerStatsCollector> tab_manager_stats_collector_;

  // Weak pointer factory used for posting delayed tasks.
  base::WeakPtrFactory<TabManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabManager);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_
