// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MEMORY_OOM_PRIORITY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_MEMORY_OOM_PRIORITY_MANAGER_H_

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;

namespace chromeos {

class LowMemoryObserver;

// The OomPriorityManager periodically checks (see
// ADJUSTMENT_INTERVAL_SECONDS in the source) the status of renderers
// and adjusts the out of memory (OOM) adjustment value (in
// /proc/<pid>/oom_score_adj) of the renderers so that they match the
// algorithm embedded here for priority in being killed upon OOM
// conditions.
//
// The algorithm used favors killing tabs that are not selected, not pinned,
// and have been idle for longest, in that order of priority.
class OomPriorityManager : public content::NotificationObserver {
 public:
  OomPriorityManager();
  ~OomPriorityManager() override;

  // Number of discard events since Chrome started.
  int discard_count() const { return discard_count_; }

  // See member comment.
  bool recent_tab_discard() const { return recent_tab_discard_; }

  void Start();
  void Stop();

  // Returns list of tab titles sorted from most interesting (don't kill)
  // to least interesting (OK to kill).
  std::vector<base::string16> GetTabTitles();

  // Discards a tab to free the memory occupied by its renderer.
  // Tab still exists in the tab-strip; clicking on it will reload it.
  // Returns true if it successfully found a tab and discarded it.
  bool DiscardTab();

  // Log memory statistics for the running processes, then discards a tab.
  // Tab discard happens sometime later, as collecting the statistics touches
  // multiple threads and takes time.
  void LogMemoryAndDiscardTab();

 private:
  friend class OomMemoryDetails;
  FRIEND_TEST_ALL_PREFIXES(OomPriorityManagerTest, Comparator);
  FRIEND_TEST_ALL_PREFIXES(OomPriorityManagerTest, IsReloadableUI);
  FRIEND_TEST_ALL_PREFIXES(OomPriorityManagerTest, GetProcessHandles);

  struct TabStats {
    TabStats();
    ~TabStats();
    bool is_app;  // browser window is an app
    bool is_reloadable_ui;  // Reloadable web UI page, like NTP or Settings.
    bool is_playing_audio;
    bool is_pinned;
    bool is_selected;  // selected in the currently active browser window
    bool is_discarded;
    base::TimeTicks last_active;
    base::ProcessHandle renderer_handle;
    int child_process_host_id;
    base::string16 title;
    int64 tab_contents_id;  // unique ID per WebContents
  };
  typedef std::vector<TabStats> TabStatsList;

  // Returns true if the |url| represents an internal Chrome web UI page that
  // can be easily reloaded and hence makes a good choice to discard.
  static bool IsReloadableUI(const GURL& url);

  // Discards a tab with the given unique ID.  Returns true if discard occurred.
  bool DiscardTabById(int64 target_web_contents_id);

  // Records UMA histogram statistics for a tab discard. We record statistics
  // for user triggered discards via chrome://discards/ because that allows us
  // to manually test the system.
  void RecordDiscardStatistics();

  // Record whether we ran out of memory during a recent time interval.
  // This allows us to normalize low memory statistics versus usage.
  void RecordRecentTabDiscard();

  // Purges data structures in the browser that can be easily recomputed.
  void PurgeBrowserMemory();

  // Returns the number of tabs open in all browser instances.
  int GetTabCount() const;

  TabStatsList GetTabStatsOnUIThread();

  // Called when the timer fires, sets oom_adjust_score for all renderers.
  void AdjustOomPriorities();

  // Pair to hold child process host id and ProcessHandle
  typedef std::pair<int, base::ProcessHandle> ProcessInfo;

  // Returns a list of child process host ids and ProcessHandles from
  // |stats_list| with unique pids. If multiple tabs use the same process,
  // returns the first child process host id and corresponding pid. This implies
  // that the processes are selected based on their "most important" tab.
  static std::vector<ProcessInfo> GetChildProcessInfos(
      const TabStatsList& stats_list);

  // Called by AdjustOomPriorities.
  void AdjustOomPrioritiesOnFileThread(TabStatsList stats_list);

  // Posts AdjustFocusedTabScore task to the file thread.
  void OnFocusTabScoreAdjustmentTimeout();

  // Sets the score of the focused tab to the least value.
  void AdjustFocusedTabScoreOnFileThread();

  static bool CompareTabStats(TabStats first, TabStats second);

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called by the memory pressure listener when the memory pressure rises.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  base::RepeatingTimer<OomPriorityManager> timer_;
  base::OneShotTimer<OomPriorityManager> focus_tab_score_adjust_timer_;
  base::RepeatingTimer<OomPriorityManager> recent_tab_discard_timer_;
  content::NotificationRegistrar registrar_;

  // This lock is for |oom_score_map_| and |focused_tab_process_info_|.
  base::Lock oom_score_lock_;
  // Map maintaining the child process host id - oom_score mapping.
  typedef base::hash_map<int, int> ProcessScoreMap;
  ProcessScoreMap oom_score_map_;
  // Holds the focused tab's child process host id.
  ProcessInfo focused_tab_process_info_;

  // The old observer for the kernel low memory signal. This is null if
  // the new MemoryPressureListener is used.
  // TODO(skuhne): Remove this when the enhanced memory observer is turned on
  // by default.
  scoped_ptr<LowMemoryObserver> low_memory_observer_;

  // A listener to global memory pressure events. This will be used if the
  // memory pressure system was instantiated - otherwise the LowMemoryObserver
  // will be used.
  scoped_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Wall-clock time when the priority manager started running.
  base::TimeTicks start_time_;

  // Wall-clock time of last tab discard during this browsing session, or 0 if
  // no discard has happened yet.
  base::TimeTicks last_discard_time_;

  // Wall-clock time of last priority adjustment, used to correct the above
  // times for discontinuities caused by suspend/resume.
  base::TimeTicks last_adjust_time_;

  // Number of times we have discarded a tab, for statistics.
  int discard_count_;

  // Whether a tab discard event has occurred during the last time interval,
  // used for statistics normalized by usage.
  bool recent_tab_discard_;

  DISALLOW_COPY_AND_ASSIGN(OomPriorityManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MEMORY_OOM_PRIORITY_MANAGER_H_
