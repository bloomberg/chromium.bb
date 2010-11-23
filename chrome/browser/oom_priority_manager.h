// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_
#define CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_

#include <list>

#include "base/timer.h"
#include "base/process.h"

namespace browser {

// The OomPriorityManager periodically checks (see
// ADJUSTMENT_INTERVAL_SECONDS in the source) the status of renderers
// and adjusts the out of memory (OOM) adjustment value (in
// /proc/<pid>/oom_adj) of the renderers so that they match the
// algorithm embedded here for priority in being killed upon OOM
// conditions.
//
// The algorithm used favors killing tabs that are not pinned, have
// been idle for longest, and take up the most memory, in that order
// of priority.  We round the idle times to the nearest few minutes
// (see BUCKET_INTERVAL_MINUTES in the source) so that we can bucket
// them, as no two tabs will have exactly the same idle time.
class OomPriorityManager {
 public:
  OomPriorityManager();
  ~OomPriorityManager();

 private:
  struct RendererStats {
    bool is_pinned;
    base::TimeTicks last_selected;
    size_t memory_used;
    base::ProcessHandle renderer_handle;
  };
  typedef std::list<RendererStats> StatsList;

  void StartTimer();
  void StopTimer();

  // Posts DoAdjustOomPriorities task to the file thread.  Called when
  // the timer fires.
  void AdjustOomPriorities();

  // Called by AdjustOomPriorities.  Runs on the file thread.
  void DoAdjustOomPriorities(StatsList list);

  static bool CompareRendererStats(RendererStats first, RendererStats second);

  base::RepeatingTimer<OomPriorityManager> timer_;

  DISALLOW_COPY_AND_ASSIGN(OomPriorityManager);
};
}  // namespace browser

DISABLE_RUNNABLE_METHOD_REFCOUNT(browser::OomPriorityManager);

#endif  // CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_
