// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_delegate_chromeos.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
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
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/process.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/zygote_host_linux.h"

using base::ProcessHandle;
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
int AppStateToPriority(const arc::mojom::ProcessState& process_state) {
  // Logic copied from Android:
  // frameworks/base/core/java/android/app/ActivityManager.java
  // Note that ProcessState enumerates from most important (lower value) to
  // least important (higher value), while ProcessPriority enumerates the
  // opposite.
  if (process_state >= arc::mojom::ProcessState::HOME) {
    return ProcessPriority::ANDROID_BACKGROUND;
  } else if (process_state >= arc::mojom::ProcessState::SERVICE) {
    return ProcessPriority::ANDROID_SERVICE;
  } else if (process_state >= arc::mojom::ProcessState::HEAVY_WEIGHT) {
    return ProcessPriority::ANDROID_CANT_SAVE_STATE;
  } else if (process_state >= arc::mojom::ProcessState::IMPORTANT_BACKGROUND) {
    return ProcessPriority::ANDROID_PERCEPTIBLE;
  } else if (process_state >= arc::mojom::ProcessState::IMPORTANT_FOREGROUND) {
    return ProcessPriority::ANDROID_VISIBLE;
  } else if (process_state >= arc::mojom::ProcessState::TOP_SLEEPING) {
    return ProcessPriority::ANDROID_TOP_SLEEPING;
  } else if (process_state >= arc::mojom::ProcessState::FOREGROUND_SERVICE) {
    return ProcessPriority::ANDROID_FOREGROUND_SERVICE;
  } else if (process_state >= arc::mojom::ProcessState::TOP) {
    return ProcessPriority::ANDROID_TOP;
  } else if (process_state >= arc::mojom::ProcessState::PERSISTENT) {
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

bool IsArcMemoryManagementEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableArcMemoryManagement);
}

}  // namespace

TabManagerDelegate::TabManagerDelegate()
    : focused_tab_process_info_(std::make_pair(0, 0)),
      arc_process_instance_(nullptr),
      arc_process_instance_version_(0),
      weak_ptr_factory_(this) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
  auto arc_bridge_service = arc::ArcBridgeService::Get();
  if (arc_bridge_service)
    arc_bridge_service->AddObserver(this);
}

TabManagerDelegate::~TabManagerDelegate() {
  auto arc_bridge_service = arc::ArcBridgeService::Get();
  if (arc_bridge_service)
    arc_bridge_service->RemoveObserver(this);
}

void TabManagerDelegate::OnProcessInstanceReady() {
  auto arc_bridge_service = arc::ArcBridgeService::Get();
  DCHECK(arc_bridge_service);

  arc_process_instance_ = arc_bridge_service->process_instance();
  arc_process_instance_version_ = arc_bridge_service->process_version();

  if (!IsArcMemoryManagementEnabled())
    return;

  DCHECK(arc_process_instance_);
  if (arc_process_instance_version_ < 2) {
    VLOG(1) << "ProcessInstance version < 2 does not "
               "support DisableBuiltinOomAdjustment() yet.";
    return;
  }
  // If --enable-arc-memory-management is on, stop Android system-wide
  // oom_adj adjustment since this class will take over oom_score_adj settings.
  arc_process_instance_->DisableBuiltinOomAdjustment();
}

void TabManagerDelegate::OnProcessInstanceClosed() {
  arc_process_instance_ = nullptr;
  arc_process_instance_version_ = 0;
}

// If able to get the list of ARC procsses, prioritize tabs and apps as a whole.
// Otherwise try to kill tabs only.
void TabManagerDelegate::LowMemoryKill(
    const base::WeakPtr<TabManager>& tab_manager,
    const TabStatsList& tab_list) {
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (arc_process_service &&
      arc_process_service->RequestProcessList(
          base::Bind(&TabManagerDelegate::LowMemoryKillImpl,
                     weak_ptr_factory_.GetWeakPtr(), tab_manager, tab_list))) {
    // LowMemoryKillImpl will be called asynchronously so nothing left to do.
    return;
  }
  // If the list of ARC processes is not available, call LowMemoryKillImpl
  // synchronously with an empty list of apps.
  std::vector<arc::ArcProcess> dummy_apps;
  LowMemoryKillImpl(tab_manager, tab_list, dummy_apps);
}

int TabManagerDelegate::GetCachedOomScore(ProcessHandle process_handle) {
  base::AutoLock oom_score_autolock(oom_score_lock_);
  auto it = oom_score_map_.find(process_handle);
  if (it != oom_score_map_.end()) {
    return it->second;
  }
  // An impossible value for oom_score_adj.
  return -1001;
}

void TabManagerDelegate::AdjustFocusedTabScoreOnFileThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::ProcessHandle pid = 0;
  {
    base::AutoLock oom_score_autolock(oom_score_lock_);
    pid = focused_tab_process_info_.second;
    oom_score_map_[pid] = chrome::kLowestRendererOomScore;
  }
  content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(
      pid, chrome::kLowestRendererOomScore);
}

void TabManagerDelegate::OnFocusTabScoreAdjustmentTimeout() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&TabManagerDelegate::AdjustFocusedTabScoreOnFileThread,
                 weak_ptr_factory_.GetWeakPtr()));
}

void TabManagerDelegate::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      {
        base::AutoLock oom_score_autolock(oom_score_lock_);
        oom_score_map_.erase(host->GetHandle());
      }
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
        ProcessScoreMap::iterator it;
        bool not_lowest_score = false;
        {
          base::AutoLock oom_score_autolock(oom_score_lock_);
          focused_tab_process_info_ =
              std::make_pair(render_host->GetID(), render_host->GetHandle());

          // If the currently focused tab already has a lower score, do not
          // set it. This can happen in case the newly focused tab is script
          // connected to the previous tab.
          it = oom_score_map_.find(focused_tab_process_info_.second);
          not_lowest_score = it == oom_score_map_.end() ||
                             it->second != chrome::kLowestRendererOomScore;
        }
        if (not_lowest_score) {
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
void TabManagerDelegate::AdjustOomPriorities(const TabStatsList& tab_list) {
  if (!IsArcMemoryManagementEnabled())
    return;

  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (arc_process_service &&
      arc_process_service->RequestProcessList(
          base::Bind(&TabManagerDelegate::AdjustOomPrioritiesImpl,
                     weak_ptr_factory_.GetWeakPtr(), tab_list))) {
    return;
  }
  // Pass in a dummy list if unable to get ARC processes or
  // --enable-arc-memory-management is off.
  AdjustOomPrioritiesImpl(tab_list, std::vector<arc::ArcProcess>());
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

void TabManagerDelegate::LowMemoryKillImpl(
    const base::WeakPtr<TabManager>& tab_manager,
    const TabStatsList& tab_list,
    const std::vector<arc::ArcProcess>& arc_processes) {

  std::vector<TabManagerDelegate::KillCandidate> candidates =
      GetSortedKillCandidates(tab_list, arc_processes);
  for (const auto& entry : candidates) {
    // Never kill persistent process.
    if (entry.priority >= ProcessPriority::ANDROID_PERSISTENT) {
      break;
    }
    if (entry.is_arc_app) {
      if (arc_process_instance_) {
        arc_process_instance_->KillProcess(entry.app->nspid, "LowMemoryKill");
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

void TabManagerDelegate::AdjustOomPrioritiesImpl(
    const TabStatsList& tab_list,
    const std::vector<arc::ArcProcess>& arc_processes) {
  // Least important first.
  auto candidates = GetSortedKillCandidates(tab_list, arc_processes);

  // Now we assign priorities based on the sorted list. We're assigning
  // priorities in the range of kLowestRendererOomScore to
  // kHighestRendererOomScore (defined in chrome_constants.h). oom_score_adj
  // takes values from -1000 to 1000. Negative values are reserved for system
  // processes, and we want to give some room below the range we're using to
  // allow for things that want to be above the renderers in priority, so the
  // defined range gives us some variation in priority without taking up the
  // whole range. In the end, however, it's a pretty arbitrary range to use.
  // Higher values are more likely to be killed by the OOM killer.

  // Break the processes into 2 parts. This is to help lower the chance of
  // altering oom score for many processes on any small change.
  int range_middle =
      (chrome::kLowestRendererOomScore + chrome::kHighestRendererOomScore) / 2;

  // Find some pivot point. For now processes with priority >= CHROME_INTERNAL
  // are prone to be affected by LRU change. Taking them as "high priority"
  // processes.
  auto lower_priority_part = candidates.rend();
  // Iterate in reverse order since the list is sorted by least importance.
  for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
    if (it->priority < ProcessPriority::CHROME_INTERNAL) {
      lower_priority_part = it;
      break;
    }
  }

  ProcessScoreMap new_map;

  // Higher priority part.
  DistributeOomScoreInRange(candidates.rbegin(), lower_priority_part,
                            chrome::kLowestRendererOomScore, range_middle,
                            &new_map);
  // Lower priority part.
  DistributeOomScoreInRange(lower_priority_part, candidates.rend(),
                            range_middle, chrome::kHighestRendererOomScore,
                            &new_map);
  base::AutoLock oom_score_autolock(oom_score_lock_);
  oom_score_map_.swap(new_map);
}

void TabManagerDelegate::SetOomScoreAdjForApp(int nspid, int score) {
  if (!arc_process_instance_)
    return;
  if (arc_process_instance_version_ < 2) {
    VLOG(1) << "ProcessInstance version < 2 does not "
               "support SetOomScoreAdj() yet.";
    return;
  }
  arc_process_instance_->SetOomScoreAdj(nspid, score);
}

void TabManagerDelegate::SetOomScoreAdjForTabs(
    const std::vector<std::pair<base::ProcessHandle, int>>& entries) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&TabManagerDelegate::SetOomScoreAdjForTabsOnFileThread,
                 weak_ptr_factory_.GetWeakPtr(), entries));
}

void TabManagerDelegate::SetOomScoreAdjForTabsOnFileThread(
    const std::vector<std::pair<base::ProcessHandle, int>>& entries) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  for (const auto& entry : entries) {
    content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(entry.first,
                                                               entry.second);
  }
}

void TabManagerDelegate::DistributeOomScoreInRange(
    std::vector<TabManagerDelegate::KillCandidate>::reverse_iterator rbegin,
    std::vector<TabManagerDelegate::KillCandidate>::reverse_iterator rend,
    int range_begin,
    int range_end,
    ProcessScoreMap* new_map) {
  // OOM score setting for tabs involves file system operation so should be
  // done on file thread.
  std::vector<std::pair<base::ProcessHandle, int>> oom_score_for_tabs;

  // Though there might be duplicate process handles, it doesn't matter to
  // overestimate the number of processes here since the we don't need to
  // use up the full range.
  int num = (rend - rbegin);
  const float priority_increment =
      static_cast<float>(range_end - range_begin) / num;

  float priority = range_begin;
  for (auto cur = rbegin; cur != rend; ++cur) {
    int score = static_cast<int>(priority + 0.5f);
    if (cur->is_arc_app) {
      // Use pid as map keys so it's globally unique.
      (*new_map)[cur->app->pid] = score;
      int cur_app_pid_score = 0;
      {
        base::AutoLock oom_score_autolock(oom_score_lock_);
        cur_app_pid_score = oom_score_map_[cur->app->pid];
      }
      if (cur_app_pid_score != score)
        SetOomScoreAdjForApp(cur->app->nspid, score);
    } else {
      base::ProcessHandle process_handle = cur->tab->renderer_handle;
      // 1. tab_list contains entries for already-discarded tabs. If the PID
      // (renderer_handle) is zero, we don't need to adjust the oom_score.
      // 2. Only add unseen process handle so if there's multiple tab maps to
      // the same process, the process is set to an oom score based on its "most
      // important" tab.
      if (process_handle != 0 &&
          new_map->find(process_handle) == new_map->end()) {
        (*new_map)[process_handle] = score;
        int process_handle_score = 0;
        {
          base::AutoLock oom_score_autolock(oom_score_lock_);
          process_handle_score = oom_score_map_[process_handle];
        }
        if (process_handle_score != score)
          oom_score_for_tabs.push_back(std::make_pair(process_handle, score));
      } else {
        continue;  // Skip priority increment.
      }
    }
    priority += priority_increment;
  }

  if (oom_score_for_tabs.size())
    SetOomScoreAdjForTabs(oom_score_for_tabs);
}

}  // namespace memory
