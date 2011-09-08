// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/oom_priority_manager.h"

#include <algorithm>
#include <vector>

#include "base/process.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/zygote_host_linux.h"

#if !defined(OS_CHROMEOS)
#error This file only meant to be compiled on ChromeOS
#endif

using base::TimeDelta;
using base::TimeTicks;
using base::ProcessHandle;
using base::ProcessMetrics;

namespace browser {

// The default interval in seconds after which to adjust the oom_score_adj
// value.
#define ADJUSTMENT_INTERVAL_SECONDS 10

// The default interval in minutes for considering activation times
// "equal".
#define BUCKET_INTERVAL_MINUTES 10


// static
OomPriorityManager* OomPriorityManager::GetInstance() {
  return Singleton<OomPriorityManager>::get();
}

OomPriorityManager::OomPriorityManager() {
  renderer_stats_.reserve(32);  // 99% of users have < 30 tabs open
}

OomPriorityManager::~OomPriorityManager() {
  Stop();
}

void OomPriorityManager::Start() {
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE,
                 TimeDelta::FromSeconds(ADJUSTMENT_INTERVAL_SECONDS),
                 this,
                 &OomPriorityManager::AdjustOomPriorities);
  }
}

void OomPriorityManager::Stop() {
  timer_.Stop();
}

std::vector<string16> OomPriorityManager::GetTabTitles() {
  base::AutoLock renderer_stats_autolock(renderer_stats_lock_);
  std::vector<string16> titles;
  titles.reserve(renderer_stats_.size());
  StatsList::iterator it = renderer_stats_.begin();
  for ( ; it != renderer_stats_.end(); ++it) {
    titles.push_back(it->title);
  }
  return titles;
}

// Returns true if |first| is considered less desirable to be killed
// than |second|.
bool OomPriorityManager::CompareRendererStats(RendererStats first,
                                                  RendererStats second) {
  // The size of the slop in comparing activation times.  [This is
  // allocated here to avoid static initialization at startup time.]
  static const int64 kTimeBucketInterval =
      TimeDelta::FromMinutes(BUCKET_INTERVAL_MINUTES).ToInternalValue();

  // Being currently selected is most important.
  if (first.is_selected != second.is_selected)
    return first.is_selected == true;

  // Being pinned is second most important.
  if (first.is_pinned != second.is_pinned)
    return first.is_pinned == true;

  // We want to be a little "fuzzy" when we compare these, because
  // it's not really possible for the times to be identical, but if
  // the user selected two tabs at about the same time, we still want
  // to take the one that uses more memory.
  if (abs((first.last_selected - second.last_selected).ToInternalValue()) <
      kTimeBucketInterval)
    return first.last_selected < second.last_selected;

  return first.memory_used < second.memory_used;
}

// Here we collect most of the information we need to sort the
// existing renderers in priority order, and hand out oom_score_adj
// scores based on that sort order.
//
// Things we need to collect on the browser thread (because
// TabStripModel isn't thread safe):
// 1) whether or not a tab is pinned
// 2) last time a tab was selected
// 3) is the tab currently selected
//
// We also need to collect:
// 4) size in memory of a tab
// But we do that in DoAdjustOomPriorities on the FILE thread so that
// we avoid jank, because it accesses /proc.
void OomPriorityManager::AdjustOomPriorities() {
  if (BrowserList::size() == 0)
    return;

  {
    base::AutoLock renderer_stats_autolock(renderer_stats_lock_);
    renderer_stats_.clear();
    for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
         browser_iterator != BrowserList::end(); ++browser_iterator) {
      Browser* browser = *browser_iterator;
      const TabStripModel* model = browser->tabstrip_model();
      for (int i = 0; i < model->count(); i++) {
        TabContents* contents = model->GetTabContentsAt(i)->tab_contents();
        RendererStats stats;
        stats.last_selected = contents->last_selected_time();
        stats.renderer_handle = contents->GetRenderProcessHost()->GetHandle();
        stats.is_pinned = model->IsTabPinned(i);
        stats.memory_used = 0;  // Calculated in DoAdjustOomPriorities.
        stats.is_selected = model->IsTabSelected(i);
        stats.title = contents->GetTitle();
        renderer_stats_.push_back(stats);
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &OomPriorityManager::DoAdjustOomPriorities));
}

void OomPriorityManager::DoAdjustOomPriorities() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::AutoLock renderer_stats_autolock(renderer_stats_lock_);
  for (StatsList::iterator stats_iter = renderer_stats_.begin();
       stats_iter != renderer_stats_.end(); ++stats_iter) {
    scoped_ptr<ProcessMetrics> metrics(ProcessMetrics::CreateProcessMetrics(
        stats_iter->renderer_handle));

    base::WorkingSetKBytes working_set_kbytes;
    if (metrics->GetWorkingSetKBytes(&working_set_kbytes)) {
      // We use the proportional set size (PSS) to calculate memory
      // usage "badness" on Linux.
      stats_iter->memory_used = working_set_kbytes.shared * 1024;
    } else {
      // and if for some reason we can't get PSS, we revert to using
      // resident set size (RSS).  This will be zero if the process
      // has already gone away, but we can live with that, since the
      // process is gone anyhow.
      stats_iter->memory_used = metrics->GetWorkingSetSize();
    }
  }

  // Now we sort the data we collected so that least desirable to be
  // killed is first, most desirable is last.
  std::sort(renderer_stats_.begin(),
            renderer_stats_.end(),
            OomPriorityManager::CompareRendererStats);

  // Now we assign priorities based on the sorted list.  We're
  // assigning priorities in the range of kLowestRendererOomScore to
  // kHighestRendererOomScore (defined in chrome_constants.h).
  // oom_score_adj takes values from -1000 to 1000.  Negative values
  // are reserved for system processes, and we want to give some room
  // below the range we're using to allow for things that want to be
  // above the renderers in priority, so the defined range gives us
  // some variation in priority without taking up the whole range.  In
  // the end, however, it's a pretty arbitrary range to use.  Higher
  // values are more likely to be killed by the OOM killer.
  //
  // We also remove any duplicate PIDs, leaving the most important
  // (least likely to be killed) of the duplicates, so that a
  // particular renderer process takes on the oom_score_adj of the
  // least likely tab to be killed.
  const int kPriorityRange = chrome::kHighestRendererOomScore -
                             chrome::kLowestRendererOomScore;
  float priority_increment =
      static_cast<float>(kPriorityRange) / renderer_stats_.size();
  float priority = chrome::kLowestRendererOomScore;
  std::set<base::ProcessHandle> already_seen;
  for (StatsList::iterator iterator = renderer_stats_.begin();
       iterator != renderer_stats_.end(); ++iterator) {
    if (already_seen.find(iterator->renderer_handle) == already_seen.end()) {
      already_seen.insert(iterator->renderer_handle);
      ZygoteHost::GetInstance()->AdjustRendererOOMScore(
          iterator->renderer_handle, static_cast<int>(priority + 0.5f));
      priority += priority_increment;
    }
  }
}

}  // namespace browser
