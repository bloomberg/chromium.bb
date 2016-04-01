// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_delegate_chromeos.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/memory_pressure_monitor_chromeos.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/arc/arc_process.h"
#include "chrome/browser/chromeos/arc/arc_process_service.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_constants.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/process.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/zygote_host_linux.h"

using base::TimeDelta;
using content::BrowserThread;

namespace memory {
namespace {

// When switching to a new tab the tab's renderer's OOM score needs to be
// updated to reflect its front-most status and protect it from discard.
// However, doing this immediately might slow down tab switch time, so wait
// a little while before doing the adjustment.
const int kFocusedTabScoreAdjustIntervalMs = 500;

// TODO(cylee): Check whether the app is in foreground or not.
int AppStateToPriority(
    const arc::ProcessState& process_state) {
  // Logic copied from Android:
  // frameworks/base/core/java/android/app/ActivityManager.java
  // Note that ProcessState enumerates from most important (lower value) to
  // least important (higher value), while ProcessPriority enumerates the
  // opposite.
  if (process_state >= arc::ProcessState::HOME) {
    return ProcessPriority::ANDROID_BACKGROUND;
  } else if (process_state >= arc::ProcessState::SERVICE) {
    return ProcessPriority::ANDROID_SERVICE;
  } else if (process_state > arc::ProcessState::HEAVY_WEIGHT) {
    return ProcessPriority::ANDROID_CANT_SAVE_STATE;
  } else if (process_state >= arc::ProcessState::IMPORTANT_BACKGROUND) {
    return ProcessPriority::ANDROID_PERCEPTIBLE;
  } else if (process_state >= arc::ProcessState::IMPORTANT_FOREGROUND) {
    return ProcessPriority::ANDROID_VISIBLE;
  } else if (process_state >= arc::ProcessState::TOP_SLEEPING) {
    return ProcessPriority::ANDROID_TOP_SLEEPING;
  } else if (process_state >= arc::ProcessState::FOREGROUND_SERVICE) {
    return ProcessPriority::ANDROID_FOREGROUND_SERVICE;
  } else if (process_state >= arc::ProcessState::TOP) {
    return ProcessPriority::ANDROID_TOP;
  } else if (process_state >= arc::ProcessState::PERSISTENT) {
    return ProcessPriority::ANDROID_PERSISTENT;
  }
  return ProcessPriority::ANDROID_NON_EXISTS;
}

// TODO(cylee): Check whether the tab is in foreground or not.
int TabStatsToPriority(const TabStats& tab) {
  int priority = 0;
  if (tab.is_app) {
    priority = ProcessPriority::CHROME_APP;
  } else if (tab.is_internal_page) {
    priority = ProcessPriority::CHROME_INTERNAL;
  } else {
    priority = ProcessPriority::CHROME_NORMAL;
  }

  if (tab.is_selected)
    priority |= ProcessPriority::CHROME_SELECTED;
  if (tab.is_pinned)
    priority |= ProcessPriority::CHROME_PINNED;
  if (tab.is_media)
    priority |= ProcessPriority::CHROME_MEDIA;
  if (tab.has_form_entry)
    priority |= ProcessPriority::CHROME_CANT_SAVE_STATE;

  return priority;
}

}  // namespace

TabManagerDelegate::TabManagerDelegate()
    : focused_tab_process_info_(std::make_pair(0, 0)) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

TabManagerDelegate::~TabManagerDelegate() {}

// If able to get the list of ARC procsses, prioritize tabs and apps as a whole.
// Otherwise try to kill tabs only.
void TabManagerDelegate::LowMemoryKill(
    const base::WeakPtr<TabManager>& tab_manager,
    const TabStatsList& tab_list) {
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (arc_process_service && arc_process_service->RequestProcessList(
      base::Bind(&TabManagerDelegate::LowMemoryKillImpl,
                 tab_manager, tab_list))) {
    // LowMemoryKillImpl will be called asynchronously so nothing left to do.
    return;
  }
  // If the list of ARC processes is not available, call LowMemoryKillImpl
  // synchronously with an empty list of apps.
  std::vector<arc::ArcProcess> dummy_apps;
  LowMemoryKillImpl(tab_manager, tab_list, dummy_apps);
}

int TabManagerDelegate::GetOomScore(int child_process_host_id) {
  base::AutoLock oom_score_autolock(oom_score_lock_);
  int score = oom_score_map_[child_process_host_id];
  return score;
}

void TabManagerDelegate::AdjustFocusedTabScoreOnFileThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::AutoLock oom_score_autolock(oom_score_lock_);
  base::ProcessHandle pid = focused_tab_process_info_.second;
  content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(
      pid, chrome::kLowestRendererOomScore);
  oom_score_map_[focused_tab_process_info_.first] =
      chrome::kLowestRendererOomScore;
}

void TabManagerDelegate::OnFocusTabScoreAdjustmentTimeout() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&TabManagerDelegate::AdjustFocusedTabScoreOnFileThread,
                 base::Unretained(this)));
}
void TabManagerDelegate::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  base::AutoLock oom_score_autolock(oom_score_lock_);
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      oom_score_map_.erase(host->GetID());
      // Coming here we know that a renderer was just killed and memory should
      // come back into the pool. However - the memory pressure observer did
      // not yet update its status and therefore we ask it to redo the
      // measurement, calling us again if we have to release more.
      // Note: We do not only accelerate the discarding speed by doing another
      // check in short succession - we also accelerate it because the timer
      // driven MemoryPressureMonitor will continue to produce timed events
      // on top. So the longer the cleanup phase takes, the more tabs will
      // get discarded in parallel.
      base::chromeos::MemoryPressureMonitor* monitor =
          base::chromeos::MemoryPressureMonitor::Get();
      if (monitor)
        monitor->ScheduleEarlyCheck();
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED: {
      bool visible = *content::Details<bool>(details).ptr();
      if (visible) {
        content::RenderProcessHost* render_host =
            content::Source<content::RenderWidgetHost>(source)
                .ptr()
                ->GetProcess();
        focused_tab_process_info_ =
            std::make_pair(render_host->GetID(), render_host->GetHandle());

        // If the currently focused tab already has a lower score, do not
        // set it. This can happen in case the newly focused tab is script
        // connected to the previous tab.
        ProcessScoreMap::iterator it;
        it = oom_score_map_.find(focused_tab_process_info_.first);
        if (it == oom_score_map_.end() ||
            it->second != chrome::kLowestRendererOomScore) {
          // By starting a timer we guarantee that the tab is focused for
          // certain amount of time. Secondly, it also does not add overhead
          // to the tab switching time.
          if (focus_tab_score_adjust_timer_.IsRunning())
            focus_tab_score_adjust_timer_.Reset();
          else
            focus_tab_score_adjust_timer_.Start(
                FROM_HERE,
                TimeDelta::FromMilliseconds(kFocusedTabScoreAdjustIntervalMs),
                this, &TabManagerDelegate::OnFocusTabScoreAdjustmentTimeout);
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Received unexpected notification";
      break;
  }
}

// Here we collect most of the information we need to sort the existing
// renderers in priority order, and hand out oom_score_adj scores based on that
// sort order.
//
// Things we need to collect on the browser thread (because
// TabStripModel isn't thread safe):
// 1) whether or not a tab is pinned
// 2) last time a tab was selected
// 3) is the tab currently selected
void TabManagerDelegate::AdjustOomPriorities(const TabStatsList& stats_list) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&TabManagerDelegate::AdjustOomPrioritiesOnFileThread,
                 base::Unretained(this), stats_list));
}

// static
std::vector<TabManagerDelegate::ProcessInfo>
TabManagerDelegate::GetChildProcessInfos(const TabStatsList& stats_list) {
  std::vector<ProcessInfo> process_infos;
  std::set<base::ProcessHandle> already_seen;
  for (TabStatsList::const_iterator iterator = stats_list.begin();
       iterator != stats_list.end(); ++iterator) {
    // stats_list contains entries for already-discarded tabs. If the PID
    // (renderer_handle) is zero, we don't need to adjust the oom_score.
    if (iterator->renderer_handle == 0)
      continue;

    bool inserted = already_seen.insert(iterator->renderer_handle).second;
    if (!inserted) {
      // We've already seen this process handle.
      continue;
    }

    process_infos.push_back(std::make_pair(iterator->child_process_host_id,
                                           iterator->renderer_handle));
  }
  return process_infos;
}

// static
std::vector<TabManagerDelegate::KillCandidate>
TabManagerDelegate::GetSortedKillCandidates(
    const TabStatsList& tab_list,
    const std::vector<arc::ArcProcess>& arc_processes) {

  std::vector<KillCandidate> candidates;
  candidates.reserve(tab_list.size() + arc_processes.size());

  for (const auto& tab : tab_list) {
    candidates.push_back(KillCandidate(&tab, TabStatsToPriority(tab)));
  }

  for (const auto& app : arc_processes) {
    candidates.push_back(
        KillCandidate(&app, AppStateToPriority(app.process_state)));
  }

  // Sort candidates according to priority.
  // TODO(cylee): Missing LRU property. Fix it when apps has the information.
  std::sort(candidates.begin(), candidates.end());

  return candidates;
}

// static
void TabManagerDelegate::LowMemoryKillImpl(
    const base::WeakPtr<TabManager>& tab_manager,
    const TabStatsList& tab_list,
    const std::vector<arc::ArcProcess>& arc_processes) {
  arc::ProcessInstance* arc_process_instance =
      arc::ArcBridgeService::Get() ?
      arc::ArcBridgeService::Get()->process_instance() :
      nullptr;

  std::vector<TabManagerDelegate::KillCandidate> candidates =
      GetSortedKillCandidates(tab_list, arc_processes);
  for (const auto& entry : candidates) {
    // Never kill persistent process.
    if (entry.priority >= ProcessPriority::ANDROID_PERSISTENT) {
      break;
    }
    if (entry.is_arc_app) {
      if (arc_process_instance) {
        arc_process_instance->KillProcess(entry.app->nspid, "LowMemoryKill");
        break;
      }
    } else {
      int64_t tab_id = entry.tab->tab_contents_id;
      // Check |tab_manager| is alive before taking tabs into consideration.
      if (tab_manager &&
          tab_manager->CanDiscardTab(tab_id) &&
          tab_manager->DiscardTabById(tab_id)) {
        break;
      }
    }
  }
}

void TabManagerDelegate::AdjustOomPrioritiesOnFileThread(
    TabStatsList stats_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::AutoLock oom_score_autolock(oom_score_lock_);

  // Remove any duplicate PIDs. Order of the list is maintained, so each
  // renderer process will take on the oom_score_adj of the most important
  // (least likely to be killed) tab.
  std::vector<ProcessInfo> process_infos = GetChildProcessInfos(stats_list);

  // Now we assign priorities based on the sorted list. We're assigning
  // priorities in the range of kLowestRendererOomScore to
  // kHighestRendererOomScore (defined in chrome_constants.h). oom_score_adj
  // takes values from -1000 to 1000. Negative values are reserved for system
  // processes, and we want to give some room below the range we're using to
  // allow for things that want to be above the renderers in priority, so the
  // defined range gives us some variation in priority without taking up the
  // whole range. In the end, however, it's a pretty arbitrary range to use.
  // Higher values are more likely to be killed by the OOM killer.
  float priority = chrome::kLowestRendererOomScore;
  const int kPriorityRange =
      chrome::kHighestRendererOomScore - chrome::kLowestRendererOomScore;
  float priority_increment =
      static_cast<float>(kPriorityRange) / process_infos.size();
  for (const auto& process_info : process_infos) {
    int score = static_cast<int>(priority + 0.5f);
    ProcessScoreMap::iterator it = oom_score_map_.find(process_info.first);
    // If a process has the same score as the newly calculated value,
    // do not set it.
    if (it == oom_score_map_.end() || it->second != score) {
      content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(
          process_info.second, score);
      oom_score_map_[process_info.first] = score;
    }
    priority += priority_increment;
  }
}

}  // namespace memory
