// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OOM_PRIORITY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_OOM_PRIORITY_MANAGER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

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
  virtual ~OomPriorityManager();

  // Number of discard events since Chrome started.
  int discard_count() const { return discard_count_; }

  void Start();
  void Stop();

  // Returns list of tab titles sorted from most interesting (don't kill)
  // to least interesting (OK to kill).
  std::vector<string16> GetTabTitles();

  // Discards a tab to free the memory occupied by its renderer.
  // Tab still exists in the tab-strip; clicking on it will reload it.
  // Returns true if it successfully found a tab and discarded it.
  bool DiscardTab();

  // Log memory statistics for the running processes, then discards a tab.
  // Tab discard happens sometime later, as collecting the statistics touches
  // multiple threads and takes time.
  void LogMemoryAndDiscardTab();

 private:
  FRIEND_TEST_ALL_PREFIXES(OomPriorityManagerTest, Comparator);

  struct TabStats {
    TabStats();
    ~TabStats();
    bool is_pinned;
    bool is_selected;
    bool is_discarded;
    bool sudden_termination_allowed;
    base::TimeTicks last_selected;
    base::ProcessHandle renderer_handle;
    string16 title;
    int64 tab_contents_id;  // unique ID per WebContents
  };
  typedef std::vector<TabStats> TabStatsList;

  // Discards a tab with the given unique ID.  Returns true if discard occurred.
  bool DiscardTabById(int64 target_web_contents_id);

  // Records UMA histogram statistics for a tab discard. We record statistics
  // for user triggered discards via chrome://discards/ because that allows us
  // to manually test the system.
  void RecordDiscardStatistics();

  // Returns the number of tabs open in all browser instances.
  int GetTabCount() const;

  TabStatsList GetTabStatsOnUIThread();

  // Called when the timer fires, sets oom_adjust_score for all renderers.
  void AdjustOomPriorities();

  // Called by AdjustOomPriorities.
  void AdjustOomPrioritiesOnFileThread(TabStatsList stats_list);

  // Posts AdjustFocusedTabScore task to the file thread.
  void OnFocusTabScoreAdjustmentTimeout();

  // Sets the score of the focused tab to the least value.
  void AdjustFocusedTabScoreOnFileThread();

  static bool CompareTabStats(TabStats first, TabStats second);

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  base::RepeatingTimer<OomPriorityManager> timer_;
  base::OneShotTimer<OomPriorityManager> focus_tab_score_adjust_timer_;
  content::NotificationRegistrar registrar_;

  // This lock is for pid_to_oom_score_ and focus_tab_pid_.
  base::Lock pid_to_oom_score_lock_;
  // map maintaining the process - oom_score mapping.
  typedef base::hash_map<base::ProcessHandle, int> ProcessScoreMap;
  ProcessScoreMap pid_to_oom_score_;
  base::ProcessHandle focused_tab_pid_;

  // Observer for the kernel low memory signal.  NULL if tab discarding is
  // disabled.
  scoped_ptr<LowMemoryObserver> low_memory_observer_;

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

  DISALLOW_COPY_AND_ASSIGN(OomPriorityManager);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_CHROMEOS_OOM_PRIORITY_MANAGER_H_
