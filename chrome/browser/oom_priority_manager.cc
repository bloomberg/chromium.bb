// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/oom_priority_manager.h"

#include <list>

#include "base/process.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/zygote_host_linux.h"

#if !defined(OS_CHROMEOS)
#error This file only meant to be compiled on ChromeOS
#endif

using base::TimeDelta;
using base::TimeTicks;
using base::ProcessHandle;
using base::ProcessMetrics;

namespace browser {

// The default interval in seconds after which to adjust the oom_adj
// value.
#define ADJUSTMENT_INTERVAL_SECONDS 10

// The default interval in minutes for considering activation times
// "equal".
#define BUCKET_INTERVAL_MINUTES 10

OomPriorityManager::OomPriorityManager() {
  StartTimer();
}

OomPriorityManager::~OomPriorityManager() {
  StopTimer();
}

void OomPriorityManager::StartTimer() {
  if (!timer_.IsRunning()) {
    timer_.Start(TimeDelta::FromSeconds(ADJUSTMENT_INTERVAL_SECONDS),
                 this,
                 &OomPriorityManager::AdjustOomPriorities);
  }
}

void OomPriorityManager::StopTimer() {
  timer_.Stop();
}


namespace {
struct RendererStats {
  bool is_pinned;
  TimeTicks last_selected;
  size_t memory_used;
  ProcessHandle renderer_handle;
};

// Returns true if |first| is considered less desirable to be killed
// than |second|.
bool SortRendererStats(RendererStats first, RendererStats second) {
  // The size of the slop in comparing activation times.  [This is
  // allocated here to avoid static initialization at startup time.]
  static const int64 kTimeBucketInterval =
      TimeDelta::FromMinutes(BUCKET_INTERVAL_MINUTES).ToInternalValue();

  // Being pinned is most important.
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
}  // anonymous namespace

// Post a new task for the file thread to actually adjust the priorities.
void OomPriorityManager::AdjustOomPriorities() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &OomPriorityManager::DoAdjustOomPriorities));
}

// Here we collect all the information we need to sort the existing
// renderers in priority order, and hand out oom_adj scores based on
// that sort order.
//
// Things we need to collect:
// 1) whether or not a tab is pinned
// 2) last time a tab was selected
// 3) size in memory of a tab
void OomPriorityManager::DoAdjustOomPriorities() {
  std::list<RendererStats> renderer_stats;

  if (BrowserList::size() == 0)
    return;

  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    const TabStripModel* model = browser->tabstrip_model();
    for (int i = 0; i < model->count(); i++) {
      TabContents* contents = model->GetTabContentsAt(i);
      RendererStats stats;
      stats.last_selected = contents->last_selected_time();
      stats.renderer_handle = contents->GetRenderProcessHost()->GetHandle();
      scoped_ptr<ProcessMetrics> metrics(
          ProcessMetrics::CreateProcessMetrics(stats.renderer_handle));

      base::WorkingSetKBytes working_set_kbytes;
      if (metrics->GetWorkingSetKBytes(&working_set_kbytes)) {
        // We use the proportional set size (PSS) to calculate memory
        // usage "badness" on Linux.
        stats.memory_used = working_set_kbytes.shared * 1024;
      } else {
        // and if for some reason we can't get PSS, we revert to using
        // resident set size (RSS).
        stats.memory_used = metrics->GetWorkingSetSize();
      }

      stats.is_pinned = model->IsTabPinned(i);
      renderer_stats.push_back(stats);
    }
  }

  // Now we sort the data we collected so that least desirable to be
  // killed is first, most desirable is last.
  renderer_stats.sort(SortRendererStats);

  // Now we assign priorities based on the sorted list.  We're
  // assigning priorities in the range of 5 to 10.  oom_adj takes
  // values from -17 to 15.  Negative values are reserved for system
  // processes, and we want to give some room on either side of the
  // range we're using to allow for things that want to be above or
  // below the renderers in priority, so 5 to 10 gives us some
  // variation in priority without taking up the whole range.  In the
  // end, however, it's a pretty arbitrary range to use.  Higher
  // values are more likely to be killed by the OOM killer.
  const int kMinPriority = 5;
  const int kMaxPriority = 10;
  const int kPriorityRange = kMaxPriority - kMinPriority;
  float priority_increment =
      static_cast<float>(kPriorityRange) / renderer_stats.size();
  float priority = kMinPriority;
  for (std::list<RendererStats>::iterator iterator = renderer_stats.begin();
       iterator != renderer_stats.end(); ++iterator) {
    Singleton<ZygoteHost>()->AdjustRendererOOMScore(
        iterator->renderer_handle,
        static_cast<int>(priority + 0.5f));
    priority += priority_increment;
  }
}

}  // namespace browser
