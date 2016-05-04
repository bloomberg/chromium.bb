// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager_delegate_chromeos.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/memory_pressure_monitor_chromeos.h"
#include "base/process/process_handle.h"  // kNullProcessHandle.
#include "base/process/process_metrics.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/arc/arc_process.h"
#include "chrome/browser/chromeos/arc/arc_process_service.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/process.mojom.h"
#include "components/exo/shell_surface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/zygote_host_linux.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_client.h"

using base::ProcessHandle;
using base::TimeDelta;
using content::BrowserThread;

namespace memory {
namespace {

const char kExoShellSurfaceWindowName[] = "ExoShellSurface";
const char kArcProcessNamePrefix[] = "org.chromium.arc.";

// When switching to a new tab the tab's renderer's OOM score needs to be
// updated to reflect its front-most status and protect it from discard.
// However, doing this immediately might slow down tab switch time, so wait
// a little while before doing the adjustment.
const int kFocusedProcessScoreAdjustIntervalMs = 500;

aura::client::ActivationClient* GetActivationClient() {
  if (!ash::Shell::HasInstance())
    return nullptr;
  return aura::client::GetActivationClient(ash::Shell::GetPrimaryRootWindow());
}

// Checks if a window renders ARC apps.
bool IsArcWindow(aura::Window* window) {
  if (!window || window->name() != kExoShellSurfaceWindowName)
    return false;
  std::string application_id = exo::ShellSurface::GetApplicationId(window);
  return application_id.find(kArcProcessNamePrefix) == 0;
}

bool IsArcWindowInForeground() {
  auto activation_client = GetActivationClient();
  return activation_client && IsArcWindow(activation_client->GetActiveWindow());
}

int AppStateToPriority(
    const arc::mojom::ProcessState& process_state) {
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
    return IsArcWindowInForeground() ?
        ProcessPriority::ANDROID_TOP :
        ProcessPriority::ANDROID_TOP_INACTIVE;
  } else if (process_state >= arc::mojom::ProcessState::PERSISTENT) {
    return ProcessPriority::ANDROID_PERSISTENT;
  }
  return ProcessPriority::ANDROID_NON_EXISTS;
}

int TabStatsToPriority(const TabStats& tab) {
  if (tab.is_selected)
    return ProcessPriority::CHROME_SELECTED;

  int priority = 0;

  if (tab.is_app) {
    priority = ProcessPriority::CHROME_APP;
  } else if (tab.is_internal_page) {
    priority = ProcessPriority::CHROME_INTERNAL;
  } else {
    priority = ProcessPriority::CHROME_NORMAL;
  }
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

// Holds the info of a newly focused tab or app window. The focused process is
// set to highest priority (lowest OOM score), but not immediately. To avoid
// redundant settings the OOM score adjusting only happens after a timeout. If
// the process loses focus before the timeout, the adjustment is canceled.
//
// This information might be set on UI thread and looked up on FILE thread. So a
// lock is needed to avoid racing.
class TabManagerDelegate::FocusedProcess {
 public:
  static const int kInvalidArcAppNspid = 0;
  struct Data {
    union {
      // If a chrome tqab.
      base::ProcessHandle pid;
      // If an ARC app.
      int nspid;
    };
    bool is_arc_app;
  };

  void SetTabPid(base::ProcessHandle pid) {
    Data* data = new Data();
    data->is_arc_app = false;
    data->pid = pid;

    base::AutoLock lock(lock_);
    data_.reset(data);
  }

  void SetArcAppNspid(int nspid) {
    Data* data = new Data();
    data->is_arc_app = true;
    data->nspid = nspid;

    base::AutoLock lock(lock_);
    data_.reset(data);
  }

  // Getter. Returns kNullProcessHandle if the process is not a tab.
  base::ProcessHandle GetTabPid() {
    base::AutoLock lock(lock_);
    if (data_ && !data_->is_arc_app)
      return data_->pid;
    return base::kNullProcessHandle;
  }

  // Getter. Returns kInvalidArcAppNspid if the process is not an arc app.
  int GetArcAppNspid() {
    base::AutoLock lock(lock_);
    if (data_ && data_->is_arc_app)
      return data_->nspid;
    return kInvalidArcAppNspid;
  }

  // An atomic operation which checks whether the containing instance is an ARC
  // app. If so it resets the data and returns true. Useful when canceling an
  // ongoing OOM score setting for a focused ARC app because the focus has been
  // shifted away shortly.
  bool ResetIfIsArcApp() {
    base::AutoLock lock(lock_);
    if (data_ && data_->is_arc_app) {
      data_.reset();
      return true;
    }
    return false;
  }

 private:
  std::unique_ptr<Data> data_;
  // Protects rw access to data_;
  base::Lock lock_;
};

// TabManagerDelegate::MemoryStat implementation.

// static
int TabManagerDelegate::MemoryStat::ReadIntFromFile(
    const char* file_name, const int default_val) {
  std::string file_string;
  if (!base::ReadFileToString(base::FilePath(file_name), &file_string)) {
    LOG(WARNING) << "Unable to read file" << file_name;
    return default_val;
  }
  int val = default_val;
  if (!base::StringToInt(
          base::TrimWhitespaceASCII(file_string, base::TRIM_TRAILING),
          &val)) {
    LOG(WARNING) << "Unable to parse string" << file_string;
    return default_val;
  }
  return val;
}

// static
int TabManagerDelegate::MemoryStat::LowMemoryMarginKB() {
  static const int kDefaultLowMemoryMarginMb = 50;
  static const char kLowMemoryMarginConfig[] =
      "/sys/kernel/mm/chromeos-low_mem/margin";
  return ReadIntFromFile(
      kLowMemoryMarginConfig, kDefaultLowMemoryMarginMb) * 1024;
}

// The logic of available memory calculation is copied from
// _is_low_mem_situation() in kernel file include/linux/low-mem-notify.h.
// Maybe we should let kernel report the number directly.
int TabManagerDelegate::MemoryStat::TargetMemoryToFreeKB() {
  static const int kRamVsSwapWeight = 4;
  static const char kMinFilelistConfig[] = "/proc/sys/vm/min_filelist_kbytes";

  base::SystemMemoryInfoKB system_mem;
  base::GetSystemMemoryInfo(&system_mem);
  const int file_mem_kb = system_mem.active_file + system_mem.inactive_file;
  const int min_filelist_kb = ReadIntFromFile(kMinFilelistConfig, 0);
  // Calculate current available memory in system.
  // File-backed memory should be easy to reclaim, unless they're dirty.
  const int available_mem_kb = system_mem.free +
      file_mem_kb - system_mem.dirty - min_filelist_kb +
      system_mem.swap_free / kRamVsSwapWeight;

  return LowMemoryMarginKB() - available_mem_kb;
}

int TabManagerDelegate::MemoryStat::EstimatedMemoryFreedKB(
    base::ProcessHandle pid) {
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(pid));
  base::WorkingSetKBytes mem_usage;
  process_metrics->GetWorkingSetKBytes(&mem_usage);
  return mem_usage.priv;
}

TabManagerDelegate::TabManagerDelegate(
    const base::WeakPtr<TabManager>& tab_manager)
  : TabManagerDelegate(tab_manager, new MemoryStat()) {
}

TabManagerDelegate::TabManagerDelegate(
    const base::WeakPtr<TabManager>& tab_manager,
    TabManagerDelegate::MemoryStat* mem_stat)
    : tab_manager_(tab_manager),
      focused_process_(new FocusedProcess()),
      mem_stat_(mem_stat),
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
  auto activation_client = GetActivationClient();
  if (activation_client)
    activation_client->AddObserver(this);
  BrowserList::GetInstance()->AddObserver(this);
}

TabManagerDelegate::~TabManagerDelegate() {
  BrowserList::GetInstance()->RemoveObserver(this);
  auto activation_client = GetActivationClient();
  if (activation_client)
    activation_client->RemoveObserver(this);
  auto arc_bridge_service = arc::ArcBridgeService::Get();
  if (arc_bridge_service)
    arc_bridge_service->RemoveObserver(this);
}

void TabManagerDelegate::OnBrowserSetLastActive(Browser* browser) {
  // Set OOM score to the selected tab when a browser window is activated.
  // content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED didn't catch the
  // case (like when switching focus between 2 browser windows) so we need to
  // handle it here.
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  int selected_index = tab_strip_model->active_index();
  content::WebContents* contents =
      tab_strip_model->GetWebContentsAt(selected_index);
  if (!contents)
    return;

  base::ProcessHandle pid = contents->GetRenderProcessHost()->GetHandle();
  AdjustFocusedTabScore(pid);
}

void TabManagerDelegate::OnProcessInstanceReady() {
  auto arc_bridge_service = arc::ArcBridgeService::Get();
  DCHECK(arc_bridge_service);

  arc_process_instance_ = arc_bridge_service->process_instance();
  arc_process_instance_version_ = arc_bridge_service->process_version();

  DCHECK(arc_process_instance_);

  if (!IsArcMemoryManagementEnabled())
    return;

  if (arc_process_instance_version_ < 2) {
    VLOG(1) << "ProcessInstance version < 2 does not "
               "support DisableBuiltinOomAdjustment() yet.";
    return;
  }
  // If --enable-arc-memory-management is on, stop Android system-wide
  // oom_adj adjustment since this class will take over oom_score_adj settings.
  arc_process_instance_->DisableBuiltinOomAdjustment();

  if (arc_process_instance_version_ < 3) {
    VLOG(1) << "arc::ProcessInstance version < 3 does not "
               "support DisableLowMemoryKiller() yet.";
    return;
  }
  arc_process_instance_->DisableLowMemoryKiller();
}

void TabManagerDelegate::OnProcessInstanceClosed() {
  arc_process_instance_ = nullptr;
  arc_process_instance_version_ = 0;
}

void TabManagerDelegate::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (IsArcWindow(gained_active)) {
    // Currently there is no way to know which app is displayed in the ARC
    // window, so schedule an early adjustment for all processes to reflect
    // the change.
    // Put a dummy FocusedProcess with nspid = kInvalidArcAppNspid for now to
    // indicate the focused process is an arc app.
    // TODO(cylee): Fix it when we have nspid info in ARC windows.
    focused_process_->SetArcAppNspid(FocusedProcess::kInvalidArcAppNspid);
    // If the timer is already running (possibly for a tab), it'll be reset
    // here.
    focus_process_score_adjust_timer_.Start(
        FROM_HERE,
        TimeDelta::FromMilliseconds(kFocusedProcessScoreAdjustIntervalMs),
        this, &TabManagerDelegate::ScheduleEarlyOomPrioritiesAdjustment);
  }
  if (IsArcWindow(lost_active)) {
    // Do not bother adjusting OOM score if the ARC window is deactivated
    // shortly.
    if (focused_process_->ResetIfIsArcApp() &&
        focus_process_score_adjust_timer_.IsRunning())
      focus_process_score_adjust_timer_.Stop();
  }
}

void TabManagerDelegate::ScheduleEarlyOomPrioritiesAdjustment() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (tab_manager_) {
    AdjustOomPriorities(tab_manager_->GetUnsortedTabStats());
  }
}

// If able to get the list of ARC procsses, prioritize tabs and apps as a whole.
// Otherwise try to kill tabs only.
void TabManagerDelegate::LowMemoryKill(
    const TabStatsList& tab_list) {
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (arc_process_service &&
      arc_process_service->RequestProcessList(
          base::Bind(&TabManagerDelegate::LowMemoryKillImpl,
                     weak_ptr_factory_.GetWeakPtr(), tab_list))) {
    // LowMemoryKillImpl will be called asynchronously so nothing left to do.
    return;
  }
  // If the list of ARC processes is not available, call LowMemoryKillImpl
  // synchronously with an empty list of apps.
  std::vector<arc::ArcProcess> dummy_apps;
  LowMemoryKillImpl(tab_list, dummy_apps);
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
  base::ProcessHandle pid = focused_process_->GetTabPid();
  // The focused process doesn't render a tab. Could happen when the focus
  // just switched to an ARC app. We can not avoid the race.
  if (pid == base::kNullProcessHandle)
    return;
  {
    base::AutoLock oom_score_autolock(oom_score_lock_);
    oom_score_map_[pid] = chrome::kLowestRendererOomScore;
  }
  content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(
      pid, chrome::kLowestRendererOomScore);
}

void TabManagerDelegate::OnFocusTabScoreAdjustmentTimeout() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&TabManagerDelegate::AdjustFocusedTabScoreOnFileThread,
                 base::Unretained(this)));
}

void TabManagerDelegate::AdjustFocusedTabScore(base::ProcessHandle pid) {
  // Clear running timer if one was set for a previous focused tab/app.
  if (focus_process_score_adjust_timer_.IsRunning())
    focus_process_score_adjust_timer_.Stop();
  focused_process_->SetTabPid(pid);

  bool not_lowest_score = false;
  {
    base::AutoLock oom_score_autolock(oom_score_lock_);
    // If the currently focused tab already has a lower score, do not
    // set it. This can happen in case the newly focused tab is script
    // connected to the previous tab.
    ProcessScoreMap::iterator it = oom_score_map_.find(pid);
    not_lowest_score = (it == oom_score_map_.end() ||
                        it->second != chrome::kLowestRendererOomScore);
  }
  if (not_lowest_score) {
    // By starting a timer we guarantee that the tab is focused for
    // certain amount of time. Secondly, it also does not add overhead
    // to the tab switching time.
    // If there's an existing running timer (could be for ARC app), it
    // would be replaced by a new task.
    focus_process_score_adjust_timer_.Start(
        FROM_HERE,
        TimeDelta::FromMilliseconds(kFocusedProcessScoreAdjustIntervalMs),
        this, &TabManagerDelegate::OnFocusTabScoreAdjustmentTimeout);
  }
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
        AdjustFocusedTabScore(render_host->GetHandle());
      }
      // Do not handle the "else" case when it changes to invisible because
      // 1. The behavior is a bit awkward in that when switching from tab A to
      // tab B, the event "invisible of B" comes after "visible of A". It can
      // cause problems when the 2 tabs have the same content (e.g., New Tab
      // Page). To be more clear, if we try to cancel the timer when losing
      // focus it may cancel the timer for the same renderer process.
      // 2. When another window is launched on top of an existing browser
      // window, the selected tab in the existing browser didn't receive this
      // event, so an attempt to cancel timer in this case doesn't work.
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
  if (IsArcMemoryManagementEnabled()) {
    arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
    if (arc_process_service &&
        arc_process_service->RequestProcessList(
            base::Bind(&TabManagerDelegate::AdjustOomPrioritiesImpl,
                       weak_ptr_factory_.GetWeakPtr(), tab_list))) {
      return;
    }
  }
  // Pass in a dummy list if unable to get ARC processes or
  // --enable-arc-memory-management is off.
  AdjustOomPrioritiesImpl(tab_list, std::vector<arc::ArcProcess>());
}

// Excludes persistent ARC apps, but still preserves active chrome tabs and
// top ARC apps. The latter ones should not be killed by TabManager since
// we still want to adjust their oom_score_adj.
// static
std::vector<TabManagerDelegate::Candidate>
TabManagerDelegate::GetSortedCandidates(
    const TabStatsList& tab_list,
    const std::vector<arc::ArcProcess>& arc_processes) {

  std::vector<Candidate> candidates;
  candidates.reserve(tab_list.size() + arc_processes.size());

  for (const auto& tab : tab_list) {
    candidates.push_back(Candidate(&tab, TabStatsToPriority(tab)));
  }

  for (const auto& app : arc_processes) {
    Candidate candidate(&app, AppStateToPriority(app.process_state));
    // Skip persistent android processes since we should never kill them.
    // Also don't ajust OOM score so their score remains min oom_score_adj.
    if (candidate.priority >= ProcessPriority::ANDROID_PERSISTENT)
      continue;
    candidates.push_back(candidate);
  }

  // Sort candidates according to priority.
  // TODO(cylee): Missing LRU property. Fix it when apps has the information.
  std::sort(candidates.begin(), candidates.end());

  return candidates;
}

bool TabManagerDelegate::KillArcProcess(const int nspid) {
  if (!arc_process_instance_)
    return false;
  arc_process_instance_->KillProcess(nspid, "LowMemoryKill");
  return true;
}

bool TabManagerDelegate::KillTab(int64_t tab_id) {
  // Check |tab_manager_| is alive before taking tabs into consideration.
  return tab_manager_ &&
      tab_manager_->CanDiscardTab(tab_id) &&
      tab_manager_->DiscardTabById(tab_id);
}

void TabManagerDelegate::LowMemoryKillImpl(
    const TabStatsList& tab_list,
    const std::vector<arc::ArcProcess>& arc_processes) {

  std::vector<TabManagerDelegate::Candidate> candidates =
      GetSortedCandidates(tab_list, arc_processes);

  int target_memory_to_free_kb = mem_stat_->TargetMemoryToFreeKB();
  for (const auto& entry : candidates) {
    // Never kill selected tab or Android foreground app, regardless whether
    // they're in the active window. Since the user experience would be bad.
    if ((!entry.is_arc_app &&
         entry.priority >= ProcessPriority::CHROME_SELECTED) ||
        (entry.is_arc_app &&
         entry.priority >= ProcessPriority::ANDROID_TOP_INACTIVE)) {
      continue;
    }
    if (entry.is_arc_app) {
        int estimated_memory_freed_kb =
            mem_stat_->EstimatedMemoryFreedKB(entry.app->pid);
        if (KillArcProcess(entry.app->nspid))
          target_memory_to_free_kb -= estimated_memory_freed_kb;
    } else {
      int64_t tab_id = entry.tab->tab_contents_id;
      int estimated_memory_freed_kb =
          mem_stat_->EstimatedMemoryFreedKB(entry.tab->renderer_handle);
      if (KillTab(tab_id))
        target_memory_to_free_kb -= estimated_memory_freed_kb;
    }
    if (target_memory_to_free_kb < 0)
      break;
  }
}

void TabManagerDelegate::AdjustOomPrioritiesImpl(
    const TabStatsList& tab_list,
    const std::vector<arc::ArcProcess>& arc_processes) {
  // Least important first.
  auto candidates = GetSortedCandidates(tab_list, arc_processes);

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
  // altering OOM score for many processes on any small change.
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
                 base::Unretained(this), entries));
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
    std::vector<TabManagerDelegate::Candidate>::reverse_iterator rbegin,
    std::vector<TabManagerDelegate::Candidate>::reverse_iterator rend,
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
      // the same process, the process is set to an OOM score based on its "most
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
