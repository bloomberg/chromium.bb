// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/oom_priority_manager.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/low_memory_observer.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
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

namespace chromeos {

namespace {

// Name of the experiment to run.
const char kExperiment[] = "LowMemoryMargin";

#define EXPERIMENT_CUSTOM_COUNTS(name, sample, min, max, buckets)          \
    UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, buckets);          \
    if (base::FieldTrialList::TrialExists(kExperiment))                    \
      UMA_HISTOGRAM_CUSTOM_COUNTS(                                         \
          base::FieldTrial::MakeName(name, kExperiment),                   \
          sample, min, max, buckets);

// Record a size in megabytes, over a potential interval up to 32 GB.
#define EXPERIMENT_HISTOGRAM_MEGABYTES(name, sample)                       \
    EXPERIMENT_CUSTOM_COUNTS(name, sample, 1, 32768, 50)

// The default interval in seconds after which to adjust the oom_score_adj
// value.
const int kAdjustmentIntervalSeconds = 10;

// If there has been no priority adjustment in this interval, we assume the
// machine was suspended and correct our timing statistics.
const int kSuspendThresholdSeconds = kAdjustmentIntervalSeconds * 4;

// The default interval in milliseconds to wait before setting the score of
// currently focused tab.
const int kFocusedTabScoreAdjustIntervalMs = 500;

// Returns a unique ID for a WebContents.  Do not cast back to a pointer, as
// the WebContents could be deleted if the user closed the tab.
int64 IdFromTabContents(WebContents* web_contents) {
  return reinterpret_cast<int64>(web_contents);
}

////////////////////////////////////////////////////////////////////////////////
// OomMemoryDetails logs details about all Chrome processes during an out-of-
// memory event in an attempt to identify the culprit, then discards a tab and
// deletes itself.
class OomMemoryDetails : public MemoryDetails {
 public:
  OomMemoryDetails();

  // MemoryDetails overrides:
  virtual void OnDetailsAvailable() OVERRIDE;

 private:
  virtual ~OomMemoryDetails() {}

  TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(OomMemoryDetails);
};

OomMemoryDetails::OomMemoryDetails() {
  AddRef();  // Released in OnDetailsAvailable().
  start_time_ = TimeTicks::Now();
}

void OomMemoryDetails::OnDetailsAvailable() {
  TimeDelta delta = TimeTicks::Now() - start_time_;
  // These logs are collected by user feedback reports.  We want them to help
  // diagnose user-reported problems with frequently discarded tabs.
  LOG(WARNING) << "OOM details (" << delta.InMilliseconds() << " ms):\n"
      << ToLogString();
  if (g_browser_process && g_browser_process->oom_priority_manager())
    g_browser_process->oom_priority_manager()->DiscardTab();
  // Delete ourselves so we don't have to worry about OomPriorityManager
  // deleting us when we're still working.
  Release();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OomPriorityManager

OomPriorityManager::TabStats::TabStats()
  : is_pinned(false),
    is_selected(false),
    is_discarded(false),
    sudden_termination_allowed(false),
    renderer_handle(0),
    tab_contents_id(0) {
}

OomPriorityManager::TabStats::~TabStats() {
}

OomPriorityManager::OomPriorityManager()
    : focused_tab_pid_(0),
      discard_count_(0) {
  // We only need the low memory observer if we want to discard tabs.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoDiscardTabs))
    low_memory_observer_.reset(new LowMemoryObserver);

  registrar_.Add(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
      content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
      content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources());
}

OomPriorityManager::~OomPriorityManager() {
  Stop();
}

void OomPriorityManager::Start() {
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE,
                 TimeDelta::FromSeconds(kAdjustmentIntervalSeconds),
                 this,
                 &OomPriorityManager::AdjustOomPriorities);
  }
  if (low_memory_observer_.get())
    low_memory_observer_->Start();
  start_time_ = TimeTicks::Now();
}

void OomPriorityManager::Stop() {
  timer_.Stop();
  if (low_memory_observer_.get())
    low_memory_observer_->Stop();
}

std::vector<string16> OomPriorityManager::GetTabTitles() {
  TabStatsList stats = GetTabStatsOnUIThread();
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);
  std::vector<string16> titles;
  titles.reserve(stats.size());
  TabStatsList::iterator it = stats.begin();
  for ( ; it != stats.end(); ++it) {
    string16 str;
    str.reserve(4096);
    str += it->title;
    str += ASCIIToUTF16(" (");
    int score = pid_to_oom_score_[it->renderer_handle];
    str += base::IntToString16(score);
    str += ASCIIToUTF16(")");
    str += ASCIIToUTF16(it->sudden_termination_allowed ? " sudden_ok " : "");
    str += ASCIIToUTF16(it->is_discarded ? " discarded" : "");
    titles.push_back(str);
  }
  return titles;
}

// TODO(jamescook): This should consider tabs with references to other tabs,
// such as tabs created with JavaScript window.open().  We might want to
// discard the entire set together, or use that in the priority computation.
bool OomPriorityManager::DiscardTab() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabStatsList stats = GetTabStatsOnUIThread();
  if (stats.empty())
    return false;
  // Loop until we find a non-discarded tab to kill.
  for (TabStatsList::const_reverse_iterator stats_rit = stats.rbegin();
       stats_rit != stats.rend();
       ++stats_rit) {
    int64 least_important_tab_id = stats_rit->tab_contents_id;
    if (DiscardTabById(least_important_tab_id))
      return true;
  }
  return false;
}

void OomPriorityManager::LogMemoryAndDiscardTab() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Deletes itself upon completion.
  OomMemoryDetails* details = new OomMemoryDetails();
  details->StartFetch(MemoryDetails::SKIP_USER_METRICS);
}

bool OomPriorityManager::DiscardTabById(int64 target_web_contents_id) {
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    TabStripModel* model = browser->tabstrip_model();
    for (int idx = 0; idx < model->count(); idx++) {
      // Can't discard tabs that are already discarded.
      if (model->IsTabDiscarded(idx))
        continue;
      WebContents* web_contents = model->GetTabContentsAt(idx)->web_contents();
      int64 web_contents_id = IdFromTabContents(web_contents);
      if (web_contents_id == target_web_contents_id) {
        LOG(WARNING) << "Discarding tab " << idx
            << " id " << target_web_contents_id;
        // Record statistics before discarding because we want to capture the
        // memory state that lead to the discard.
        RecordDiscardStatistics();
        model->DiscardTabContentsAt(idx);
        return true;
      }
    }
  }
  return false;
}

void OomPriorityManager::RecordDiscardStatistics() {
  // Record a raw count so we can compare to discard reloads.
  discard_count_++;
  EXPERIMENT_CUSTOM_COUNTS("Tabs.Discard.DiscardCount",
                           discard_count_, 1, 1000, 50);

  // TODO(jamescook): Maybe incorporate extension count?
  EXPERIMENT_CUSTOM_COUNTS("Tabs.Discard.TabCount", GetTabCount(), 1, 100, 50);

  // TODO(jamescook): If the time stats prove too noisy, then divide up users
  // based on how heavily they use Chrome using tab count as a proxy.
  // Bin into <= 1, <= 2, <= 4, <= 8, etc.
  if (last_discard_time_.is_null()) {
    // This is the first discard this session.
    TimeDelta interval = TimeTicks::Now() - start_time_;
    int interval_seconds = static_cast<int>(interval.InSeconds());
    // Record time in seconds over an interval of approximately 1 day.
    EXPERIMENT_CUSTOM_COUNTS(
        "Tabs.Discard.InitialTime2", interval_seconds, 1, 100000, 50);
  } else {
    // Not the first discard, so compute time since last discard.
    TimeDelta interval = TimeTicks::Now() - last_discard_time_;
    int interval_ms = static_cast<int>(interval.InMilliseconds());
    // Record time in milliseconds over an interval of approximately 1 day.
    // Start at 100 ms to get extra resolution in the target 750 ms range.
    EXPERIMENT_CUSTOM_COUNTS(
        "Tabs.Discard.IntervalTime2", interval_ms, 100, 100000 * 1000, 50);
  }
  // Record Chrome's concept of system memory usage at the time of the discard.
  base::SystemMemoryInfoKB memory;
  if (base::GetSystemMemoryInfo(&memory)) {
    int mem_anonymous_mb = (memory.active_anon + memory.inactive_anon) / 1024;
    EXPERIMENT_HISTOGRAM_MEGABYTES("Tabs.Discard.MemAnonymousMB",
                                   mem_anonymous_mb);

    int mem_available_mb =
        (memory.active_file + memory.inactive_file + memory.free) / 1024;
    EXPERIMENT_HISTOGRAM_MEGABYTES("Tabs.Discard.MemAvailableMB",
                                   mem_available_mb);
  }
  // Set up to record the next interval.
  last_discard_time_ = TimeTicks::Now();
}

int OomPriorityManager::GetTabCount() const {
  int tab_count = 0;
  for (BrowserList::const_iterator browser_it = BrowserList::begin();
      browser_it != BrowserList::end(); ++browser_it) {
    Browser* browser = *browser_it;
    tab_count += browser->tabstrip_model()->count();
  }
  return tab_count;
}

// Returns true if |first| is considered less desirable to be killed
// than |second|.
bool OomPriorityManager::CompareTabStats(TabStats first,
                                         TabStats second) {
  // Being currently selected is most important.
  if (first.is_selected != second.is_selected)
    return first.is_selected == true;

  // Being pinned is second most important.
  if (first.is_pinned != second.is_pinned)
    return first.is_pinned == true;

  // TODO(jamescook): Incorporate sudden_termination_allowed into the sort
  // order.  We don't do this now because pages with unload handlers set
  // sudden_termination_allowed false, and that covers too many common pages
  // with ad networks and statistics scripts.  Ideally we would like to check
  // for beforeUnload handlers, which are likely to present a dialog asking
  // if the user wants to discard state.  crbug.com/123049

  // Being more recently selected is more important.
  return first.last_selected > second.last_selected;
}

void OomPriorityManager::AdjustFocusedTabScoreOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);
  content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(
      focused_tab_pid_, chrome::kLowestRendererOomScore);
  pid_to_oom_score_[focused_tab_pid_] = chrome::kLowestRendererOomScore;
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
  base::ProcessHandle handle = 0;
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      handle =
          content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details)->handle;
      pid_to_oom_score_.erase(handle);
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      handle = content::Source<content::RenderProcessHost>(source)->
          GetHandle();
      pid_to_oom_score_.erase(handle);
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED: {
      bool visible = *content::Details<bool>(details).ptr();
      if (visible) {
        focused_tab_pid_ =
            content::Source<content::RenderWidgetHost>(source).ptr()->
            GetProcess()->GetHandle();

        // If the currently focused tab already has a lower score, do not
        // set it. This can happen in case the newly focused tab is script
        // connected to the previous tab.
        ProcessScoreMap::iterator it;
        it = pid_to_oom_score_.find(focused_tab_pid_);
        if (it == pid_to_oom_score_.end()
            || it->second != chrome::kLowestRendererOomScore) {
          // By starting a timer we guarantee that the tab is focused for
          // certain amount of time. Secondly, it also does not add overhead
          // to the tab switching time.
          if (focus_tab_score_adjust_timer_.IsRunning())
            focus_tab_score_adjust_timer_.Reset();
          else
            focus_tab_score_adjust_timer_.Start(FROM_HERE,
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
  if (BrowserList::size() == 0)
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TabStatsList stats_list;
  stats_list.reserve(32);  // 99% of users have < 30 tabs open
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    const TabStripModel* model = browser->tabstrip_model();
    for (int i = 0; i < model->count(); i++) {
      WebContents* contents = model->GetTabContentsAt(i)->web_contents();
      if (!contents->IsCrashed()) {
        TabStats stats;
        stats.is_pinned = model->IsTabPinned(i);
        stats.is_selected = model->IsTabSelected(i);
        stats.is_discarded = model->IsTabDiscarded(i);
        stats.sudden_termination_allowed =
            contents->GetRenderProcessHost()->SuddenTerminationAllowed();
        stats.last_selected = contents->GetLastSelectedTime();
        stats.renderer_handle = contents->GetRenderProcessHost()->GetHandle();
        stats.title = contents->GetTitle();
        stats.tab_contents_id = IdFromTabContents(contents);
        stats_list.push_back(stats);
      }
    }
  }
  // Sort the data we collected so that least desirable to be
  // killed is first, most desirable is last.
  std::sort(stats_list.begin(), stats_list.end(), CompareTabStats);
  return stats_list;
}

void OomPriorityManager::AdjustOomPrioritiesOnFileThread(
    TabStatsList stats_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);

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
      static_cast<float>(kPriorityRange) / stats_list.size();
  float priority = chrome::kLowestRendererOomScore;
  std::set<base::ProcessHandle> already_seen;
  int score = 0;
  ProcessScoreMap::iterator it;
  for (TabStatsList::iterator iterator = stats_list.begin();
       iterator != stats_list.end(); ++iterator) {
    if (already_seen.find(iterator->renderer_handle) == already_seen.end()) {
      already_seen.insert(iterator->renderer_handle);
      // If a process has the same score as the newly calculated value,
      // do not set it.
      score = static_cast<int>(priority + 0.5f);
      it = pid_to_oom_score_.find(iterator->renderer_handle);
      if (it == pid_to_oom_score_.end() || it->second != score) {
        content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(
            iterator->renderer_handle, score);
        pid_to_oom_score_[iterator->renderer_handle] = score;
      }
      priority += priority_increment;
    }
  }
}

}  // namespace chromeos
