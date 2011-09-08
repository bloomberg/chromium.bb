// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_
#define CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_

#include <vector>

#include "base/string16.h"

namespace browser {

class OomPriorityManagerImpl;

// The OomPriorityManager periodically checks (see
// ADJUSTMENT_INTERVAL_SECONDS in the source) the status of renderers
// and adjusts the out of memory (OOM) adjustment value (in
// /proc/<pid>/oom_score_adj) of the renderers so that they match the
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
  // We need to explicitly manage our destruction, so don't use Singleton.
  static void Create();
  static void Destroy();

  // Returns list of tab titles sorted from most interesting (don't kill)
  // to least interesting (OK to kill).
  static std::vector<string16> GetTabTitles();

 private:
  static OomPriorityManagerImpl* impl_;
};

}  // namespace browser

#endif  // CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_
