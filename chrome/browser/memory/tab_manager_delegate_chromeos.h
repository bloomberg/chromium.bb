// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_TAB_MANAGER_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_MEMORY_TAB_MANAGER_DELEGATE_CHROMEOS_H_

#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/process/process.h"
#include "base/timer/timer.h"
#include "chrome/browser/memory/tab_manager.h"
#include "chrome/browser/memory/tab_stats.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace memory {

// The Chrome OS TabManagerDelegate is responsible for keeping the
// renderers' scores up to date in /proc/<pid>/oom_score_adj.
//
// Note that AdjustOomPriorities will be called on the UI thread by
// TabManager, but the actual work will take place on the file thread
// (see implementation of AdjustOomPriorities).
class TabManagerDelegate : public content::NotificationObserver {
 public:
  TabManagerDelegate();
  ~TabManagerDelegate() override;

  // Return the score of a process.
  int GetOomScore(int child_process_host_id);

  // Called when the timer fires, sets oom_adjust_score for all renderers.
  void AdjustOomPriorities(const TabStatsList& stats_list);

 private:
  FRIEND_TEST_ALL_PREFIXES(TabManagerDelegateTest, GetProcessHandles);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Pair to hold child process host id and ProcessHandle.
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

  // Registrar to receive renderer notifications.
  content::NotificationRegistrar registrar_;
  // Timer to guarantee that the tab is focused for a certain amount of time.
  base::OneShotTimer focus_tab_score_adjust_timer_;
  // This lock is for |oom_score_map_| and |focused_tab_process_info_|.
  base::Lock oom_score_lock_;
  // Map maintaining the child process host id - oom_score mapping.
  typedef base::hash_map<int, int> ProcessScoreMap;
  ProcessScoreMap oom_score_map_;
  // Holds the focused tab's child process host id.
  ProcessInfo focused_tab_process_info_;

  DISALLOW_COPY_AND_ASSIGN(TabManagerDelegate);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_TAB_MANAGER_DELEGATE_CHROMEOS_H_
