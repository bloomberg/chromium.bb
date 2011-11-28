
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_
#define CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/process.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class TabContents;

namespace browser {

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

  void Start();
  void Stop();

  // Returns list of tab titles sorted from most interesting (don't kill)
  // to least interesting (OK to kill).
  std::vector<string16> GetTabTitles();

  // Discards a tab to free the memory occupied by its renderer.
  // Tab still exists in the tab-strip; clicking on it will reload it.
  // Returns true if it successfully found a tab and discarded it.
  bool DiscardTab();

 private:
  FRIEND_TEST_ALL_PREFIXES(OomPriorityManagerTest, Comparator);

  struct TabStats {
    TabStats();
    ~TabStats();
    bool is_pinned;
    bool is_selected;
    base::TimeTicks last_selected;
    base::ProcessHandle renderer_handle;
    string16 title;
    int64 tab_contents_id;  // unique ID per TabContents
  };
  typedef std::vector<TabStats> TabStatsList;

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

  DISALLOW_COPY_AND_ASSIGN(OomPriorityManager);
};

}  // namespace browser

DISABLE_RUNNABLE_METHOD_REFCOUNT(browser::OomPriorityManager);

#endif  // CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_
