// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/oom_priority_manager.h"

#include <algorithm>
#include <set>
#include <vector>

#include "ash/multi_profile_uma.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/memory/low_memory_observer_chromeos.h"
#include "chrome/browser/memory/oom_memory_details.h"
#include "chrome/browser/memory/system_memory_stats_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/zygote_host_linux.h"

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;
using content::WebContents;

namespace memory {
namespace {

// The default interval in seconds after which to adjust the oom_score_adj
// value.
const int kAdjustmentIntervalSeconds = 10;

// For each period of this length we record a statistic to indicate whether
// or not the user experienced a low memory event. If you change this interval
// you must replace Tabs.Discard.DiscardInLastMinute with a new statistic.
const int kRecentTabDiscardIntervalSeconds = 60;

// If there has been no priority adjustment in this interval, we assume the
// machine was suspended and correct our timing statistics.
const int kSuspendThresholdSeconds = kAdjustmentIntervalSeconds * 4;

// When switching to a new tab the tab's renderer's OOM score needs to be
// updated to reflect its front-most status and protect it from discard.
// However, doing this immediately might slow down tab switch time, so wait
// a little while before doing the adjustment.
const int kFocusedTabScoreAdjustIntervalMs = 500;

// Returns a unique ID for a WebContents.  Do not cast back to a pointer, as
// the WebContents could be deleted if the user closed the tab.
int64 IdFromWebContents(WebContents* web_contents) {
  return reinterpret_cast<int64>(web_contents);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OomPriorityManager

OomPriorityManager::TabStats::TabStats()
    : is_app(false),
      is_reloadable_ui(false),
      is_playing_audio(false),
      is_pinned(false),
      is_selected(false),
      is_discarded(false),
      renderer_handle(0),
      tab_contents_id(0) {
}

OomPriorityManager::TabStats::~TabStats() {
}

OomPriorityManager::OomPriorityManager()
    : focused_tab_process_info_(std::make_pair(0, 0)),
      discard_count_(0),
      recent_tab_discard_(false) {
  // Use the old |LowMemoryObserver| when there is no |MemoryPressureMonitor|.
  if (!base::MemoryPressureMonitor::Get())
    low_memory_observer_.reset(new LowMemoryObserver);

  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

OomPriorityManager::~OomPriorityManager() {
  Stop();
}

void OomPriorityManager::Start() {
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, TimeDelta::FromSeconds(kAdjustmentIntervalSeconds),
                 this, &OomPriorityManager::AdjustOomPriorities);
  }
  if (!recent_tab_discard_timer_.IsRunning()) {
    recent_tab_discard_timer_.Start(
        FROM_HERE, TimeDelta::FromSeconds(kRecentTabDiscardIntervalSeconds),
        this, &OomPriorityManager::RecordRecentTabDiscard);
  }
  start_time_ = TimeTicks::Now();
  // If a |LowMemoryObserver| exists we use the old system, otherwise we create
  // a |MemoryPressureListener| to listen for memory events.
  if (low_memory_observer_) {
    low_memory_observer_->Start();
  } else {
    base::MemoryPressureMonitor* monitor = base::MemoryPressureMonitor::Get();
    if (monitor) {
      memory_pressure_listener_.reset(
          new base::MemoryPressureListener(base::Bind(
              &OomPriorityManager::OnMemoryPressure, base::Unretained(this))));
      base::MemoryPressureListener::MemoryPressureLevel level =
          monitor->GetCurrentPressureLevel();
      if (level ==
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
        OnMemoryPressure(level);
      }
    }
  }
}

void OomPriorityManager::Stop() {
  timer_.Stop();
  recent_tab_discard_timer_.Stop();
  if (low_memory_observer_)
    low_memory_observer_->Stop();
  else
    memory_pressure_listener_.reset();
}

std::vector<base::string16> OomPriorityManager::GetTabTitles() {
  TabStatsList stats = GetTabStatsOnUIThread();
  base::AutoLock oom_score_autolock(oom_score_lock_);
  std::vector<base::string16> titles;
  titles.reserve(stats.size());
  TabStatsList::iterator it = stats.begin();
  for (; it != stats.end(); ++it) {
    base::string16 str;
    str.reserve(4096);
    int score = oom_score_map_[it->child_process_host_id];
    str += base::IntToString16(score);
    str += base::ASCIIToUTF16(" - ");
    str += it->title;
    str += base::ASCIIToUTF16(it->is_app ? " app" : "");
    str += base::ASCIIToUTF16(it->is_reloadable_ui ? " reloadable_ui" : "");
    str += base::ASCIIToUTF16(it->is_playing_audio ? " playing_audio" : "");
    str += base::ASCIIToUTF16(it->is_pinned ? " pinned" : "");
    str += base::ASCIIToUTF16(it->is_discarded ? " discarded" : "");
    titles.push_back(str);
  }
  return titles;
}

// TODO(jamescook): This should consider tabs with references to other tabs,
// such as tabs created with JavaScript window.open().  We might want to
// discard the entire set together, or use that in the priority computation.
bool OomPriorityManager::DiscardTab() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TabStatsList stats = GetTabStatsOnUIThread();
  if (stats.empty())
    return false;
  // Loop until we find a non-discarded tab to kill.
  for (TabStatsList::const_reverse_iterator stats_rit = stats.rbegin();
       stats_rit != stats.rend(); ++stats_rit) {
    int64 least_important_tab_id = stats_rit->tab_contents_id;
    if (DiscardTabById(least_important_tab_id))
      return true;
  }
  return false;
}

void OomPriorityManager::LogMemoryAndDiscardTab() {
  LogMemory("Tab Discards Memory details",
            base::Bind(&OomPriorityManager::PurgeMemoryAndDiscardTabs));
}

void OomPriorityManager::LogMemory(const std::string& title,
                                   const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OomMemoryDetails::Log(title, callback);
}

///////////////////////////////////////////////////////////////////////////////
// OomPriorityManager, private:

// static
void OomPriorityManager::PurgeMemoryAndDiscardTabs() {
  if (g_browser_process &&
      g_browser_process->platform_part()->oom_priority_manager()) {
    OomPriorityManager* manager =
        g_browser_process->platform_part()->oom_priority_manager();
    manager->PurgeBrowserMemory();
    manager->DiscardTab();
  }
}

// static
bool OomPriorityManager::IsReloadableUI(const GURL& url) {
  // There are many chrome:// UI URLs, but only look for the ones that users
  // are likely to have open. Most of the benefit is the from NTP URL.
  const char* const kReloadableUrlPrefixes[] = {
      chrome::kChromeUIDownloadsURL,
      chrome::kChromeUIHistoryURL,
      chrome::kChromeUINewTabURL,
      chrome::kChromeUISettingsURL,
  };
  // Prefix-match against the table above. Use strncmp to avoid allocating
  // memory to convert the URL prefix constants into std::strings.
  for (size_t i = 0; i < arraysize(kReloadableUrlPrefixes); ++i) {
    if (!strncmp(url.spec().c_str(), kReloadableUrlPrefixes[i],
                 strlen(kReloadableUrlPrefixes[i])))
      return true;
  }
  return false;
}

bool OomPriorityManager::DiscardTabById(int64 target_web_contents_id) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    TabStripModel* model = browser->tab_strip_model();
    for (int idx = 0; idx < model->count(); idx++) {
      // Can't discard tabs that are already discarded or active.
      if (model->IsTabDiscarded(idx) || (model->active_index() == idx))
        continue;
      WebContents* web_contents = model->GetWebContentsAt(idx);
      int64 web_contents_id = IdFromWebContents(web_contents);
      if (web_contents_id == target_web_contents_id) {
        LOG(WARNING) << "Discarding tab " << idx << " id "
                     << target_web_contents_id;
        // Record statistics before discarding because we want to capture the
        // memory state that lead to the discard.
        RecordDiscardStatistics();
        model->DiscardWebContentsAt(idx);
        recent_tab_discard_ = true;
        return true;
      }
    }
  }
  return false;
}

void OomPriorityManager::RecordDiscardStatistics() {
  // Record a raw count so we can compare to discard reloads.
  discard_count_++;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.Discard.DiscardCount", discard_count_, 1,
                              1000, 50);

  // TODO(jamescook): Maybe incorporate extension count?
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.Discard.TabCount", GetTabCount(), 1, 100,
                              50);
  // Record the discarded tab in relation to the amount of simultaneously
  // logged in users.
  ash::MultiProfileUMA::RecordDiscardedTab(ash::Shell::GetInstance()
                                               ->session_state_delegate()
                                               ->NumberOfLoggedInUsers());

  // TODO(jamescook): If the time stats prove too noisy, then divide up users
  // based on how heavily they use Chrome using tab count as a proxy.
  // Bin into <= 1, <= 2, <= 4, <= 8, etc.
  if (last_discard_time_.is_null()) {
    // This is the first discard this session.
    TimeDelta interval = TimeTicks::Now() - start_time_;
    int interval_seconds = static_cast<int>(interval.InSeconds());
    // Record time in seconds over an interval of approximately 1 day.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.Discard.InitialTime2", interval_seconds,
                                1, 100000, 50);
  } else {
    // Not the first discard, so compute time since last discard.
    TimeDelta interval = TimeTicks::Now() - last_discard_time_;
    int interval_ms = static_cast<int>(interval.InMilliseconds());
    // Record time in milliseconds over an interval of approximately 1 day.
    // Start at 100 ms to get extra resolution in the target 750 ms range.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.Discard.IntervalTime2", interval_ms, 100,
                                100000 * 1000, 50);
  }
  // Record chromeos's concept of system memory usage at the time of the
  // discard.
  RecordMemoryStats(RECORD_MEMORY_STATS_TAB_DISCARDED);

  // Set up to record the next interval.
  last_discard_time_ = TimeTicks::Now();
}

void OomPriorityManager::RecordRecentTabDiscard() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If we change the interval we need to change the histogram name.
  UMA_HISTOGRAM_BOOLEAN("Tabs.Discard.DiscardInLastMinute",
                        recent_tab_discard_);
  // Reset for the next interval.
  recent_tab_discard_ = false;
}

void OomPriorityManager::PurgeBrowserMemory() {
  // Based on experimental evidence, attempts to free memory from renderers
  // have been too slow to use in OOM situations (V8 garbage collection) or
  // do not lead to persistent decreased usage (image/bitmap caches). This
  // function therefore only targets large blocks of memory in the browser.
  // Note that other objects will listen to MemoryPressureListener events
  // to release memory.
  for (TabContentsIterator it; !it.done(); it.Next()) {
    WebContents* web_contents = *it;
    // Screenshots can consume ~5 MB per web contents for platforms that do
    // touch back/forward.
    web_contents->GetController().ClearAllScreenshots();
  }
}

int OomPriorityManager::GetTabCount() const {
  int tab_count = 0;
  for (chrome::BrowserIterator it; !it.done(); it.Next())
    tab_count += it->tab_strip_model()->count();
  return tab_count;
}

// Returns true if |first| is considered less desirable to be killed
// than |second|.
bool OomPriorityManager::CompareTabStats(TabStats first, TabStats second) {
  // Being currently selected is most important to protect.
  if (first.is_selected != second.is_selected)
    return first.is_selected;

  // Tab with internal web UI like NTP or Settings are good choices to discard,
  // so protect non-Web UI and let the other conditionals finish the sort.
  if (first.is_reloadable_ui != second.is_reloadable_ui)
    return !first.is_reloadable_ui;

  // Being pinned is important to protect.
  if (first.is_pinned != second.is_pinned)
    return first.is_pinned;

  // Being an app is important too, as you're the only visible surface in the
  // window and we don't want to discard that.
  if (first.is_app != second.is_app)
    return first.is_app;

  // Protect streaming audio and video conferencing tabs.
  if (first.is_playing_audio != second.is_playing_audio)
    return first.is_playing_audio;

  // TODO(jamescook): Incorporate sudden_termination_allowed into the sort
  // order.  We don't do this now because pages with unload handlers set
  // sudden_termination_allowed false, and that covers too many common pages
  // with ad networks and statistics scripts.  Ideally we would like to check
  // for beforeUnload handlers, which are likely to present a dialog asking
  // if the user wants to discard state.  crbug.com/123049

  // Being more recently active is more important.
  return first.last_active > second.last_active;
}

void OomPriorityManager::AdjustFocusedTabScoreOnFileThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::AutoLock oom_score_autolock(oom_score_lock_);
  base::ProcessHandle pid = focused_tab_process_info_.second;
  content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(
      pid, chrome::kLowestRendererOomScore);
  oom_score_map_[focused_tab_process_info_.first] =
      chrome::kLowestRendererOomScore;
}

void OomPriorityManager::OnFocusTabScoreAdjustmentTimeout() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OomPriorityManager::AdjustFocusedTabScoreOnFileThread,
                 base::Unretained(this)));
}

void OomPriorityManager::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  base::AutoLock oom_score_autolock(oom_score_lock_);
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* host =
          content::Source<content::RenderProcessHost>(source).ptr();
      oom_score_map_.erase(host->GetID());
      if (!low_memory_observer_) {
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
      }
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
                this, &OomPriorityManager::OnFocusTabScoreAdjustmentTimeout);
        }
      }
      break;
    }
    default:
      NOTREACHED() << L"Received unexpected notification";
      break;
  }
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
void OomPriorityManager::AdjustOomPriorities() {
  if (BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH)->empty())
    return;

  // Check for a discontinuity in time caused by the machine being suspended.
  if (!last_adjust_time_.is_null()) {
    TimeDelta suspend_time = TimeTicks::Now() - last_adjust_time_;
    if (suspend_time.InSeconds() > kSuspendThresholdSeconds) {
      // We were probably suspended, move our event timers forward in time so
      // when we subtract them out later we are counting "uptime".
      start_time_ += suspend_time;
      if (!last_discard_time_.is_null())
        last_discard_time_ += suspend_time;
    }
  }
  last_adjust_time_ = TimeTicks::Now();

  TabStatsList stats_list = GetTabStatsOnUIThread();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OomPriorityManager::AdjustOomPrioritiesOnFileThread,
                 base::Unretained(this), stats_list));
}

OomPriorityManager::TabStatsList OomPriorityManager::GetTabStatsOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TabStatsList stats_list;
  stats_list.reserve(32);  // 99% of users have < 30 tabs open
  bool browser_active = true;
  const BrowserList* ash_browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  for (BrowserList::const_reverse_iterator browser_iterator =
           ash_browser_list->begin_last_active();
       browser_iterator != ash_browser_list->end_last_active();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;
    bool is_browser_for_app = browser->is_app();
    const TabStripModel* model = browser->tab_strip_model();
    for (int i = 0; i < model->count(); i++) {
      WebContents* contents = model->GetWebContentsAt(i);
      if (!contents->IsCrashed()) {
        TabStats stats;
        stats.is_app = is_browser_for_app;
        stats.is_reloadable_ui =
            IsReloadableUI(contents->GetLastCommittedURL());
        stats.is_playing_audio = chrome::IsPlayingAudio(contents);
        stats.is_pinned = model->IsTabPinned(i);
        stats.is_selected = browser_active && model->IsTabSelected(i);
        stats.is_discarded = model->IsTabDiscarded(i);
        stats.last_active = contents->GetLastActiveTime();
        stats.renderer_handle = contents->GetRenderProcessHost()->GetHandle();
        stats.child_process_host_id = contents->GetRenderProcessHost()->GetID();
        stats.title = contents->GetTitle();
        stats.tab_contents_id = IdFromWebContents(contents);
        stats_list.push_back(stats);
      }
    }
    // We process the active browser window in the first iteration.
    browser_active = false;
  }
  // Sort the data we collected so that least desirable to be
  // killed is first, most desirable is last.
  std::sort(stats_list.begin(), stats_list.end(), CompareTabStats);
  return stats_list;
}

// static
std::vector<OomPriorityManager::ProcessInfo>
OomPriorityManager::GetChildProcessInfos(const TabStatsList& stats_list) {
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

void OomPriorityManager::AdjustOomPrioritiesOnFileThread(
    TabStatsList stats_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::AutoLock oom_score_autolock(oom_score_lock_);

  // Remove any duplicate PIDs. Order of the list is maintained, so each
  // renderer process will take on the oom_score_adj of the most important
  // (least likely to be killed) tab.
  std::vector<ProcessInfo> process_infos = GetChildProcessInfos(stats_list);

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

void OomPriorityManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // For the moment we only do something when we reach a critical state.
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    LogMemoryAndDiscardTab();
  }
  // TODO(skuhne): If more memory pressure levels are introduced, we might
  // consider to call PurgeBrowserMemory() before CRITICAL is reached.
}

}  // namespace memory
