// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/memory/oom_priority_manager.h"

#include <algorithm>
#include <set>
#include <vector>

#include "ash/multi_profile_uma.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/memory/low_memory_observer.h"
#include "chrome/browser/memory_details.h"
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
#include "ui/base/text/bytes_formatting.h"

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;
using content::WebContents;

namespace chromeos {

namespace {

// Record a size in megabytes, over a potential interval up to 32 GB.
#define HISTOGRAM_MEGABYTES(name, sample)                     \
    UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, 1, 32768, 50)

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

// Records a statistics |sample| for UMA histogram |name| using a linear
// distribution of buckets.
void RecordLinearHistogram(const std::string& name,
                           int sample,
                           int maximum,
                           size_t bucket_count) {
  // Do not use the UMA_HISTOGRAM_... macros here.  They cache the Histogram
  // instance and thus only work if |name| is constant.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      name,
      1,  // Minimum. The 0 bin for underflow is automatically added.
      maximum + 1,  // Ensure bucket size of |maximum| / |bucket_count|.
      bucket_count + 2,  // Account for the underflow and overflow bins.
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

}  // namespace

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
  std::string log_string = ToLogString();
  base::SystemMemoryInfoKB memory;
  if (base::GetSystemMemoryInfo(&memory) && memory.gem_size != -1) {
    log_string += "Graphics ";
    log_string += base::UTF16ToASCII(ui::FormatBytes(memory.gem_size));
  }
  LOG(WARNING) << "OOM details (" << delta.InMilliseconds() << " ms):\n"
      << log_string;
  if (g_browser_process &&
      g_browser_process->platform_part()->oom_priority_manager()) {
    OomPriorityManager* manager =
        g_browser_process->platform_part()->oom_priority_manager();
    manager->PurgeBrowserMemory();
    manager->DiscardTab();
  }
  // Delete ourselves so we don't have to worry about OomPriorityManager
  // deleting us when we're still working.
  Release();
}

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
    : focused_tab_pid_(0),
      low_memory_observer_(new LowMemoryObserver),
      discard_count_(0),
      recent_tab_discard_(false) {
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
  if (!recent_tab_discard_timer_.IsRunning()) {
    recent_tab_discard_timer_.Start(
        FROM_HERE,
        TimeDelta::FromSeconds(kRecentTabDiscardIntervalSeconds),
        this,
        &OomPriorityManager::RecordRecentTabDiscard);
  }
  if (low_memory_observer_.get())
    low_memory_observer_->Start();
  start_time_ = TimeTicks::Now();
}

void OomPriorityManager::Stop() {
  timer_.Stop();
  recent_tab_discard_timer_.Stop();
  if (low_memory_observer_.get())
    low_memory_observer_->Stop();
}

std::vector<base::string16> OomPriorityManager::GetTabTitles() {
  TabStatsList stats = GetTabStatsOnUIThread();
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);
  std::vector<base::string16> titles;
  titles.reserve(stats.size());
  TabStatsList::iterator it = stats.begin();
  for ( ; it != stats.end(); ++it) {
    base::string16 str;
    str.reserve(4096);
    int score = pid_to_oom_score_[it->renderer_handle];
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

///////////////////////////////////////////////////////////////////////////////
// OomPriorityManager, private:

// static
bool OomPriorityManager::IsReloadableUI(const GURL& url) {
  // There are many chrome:// UI URLs, but only look for the ones that users
  // are likely to have open. Most of the benefit is the from NTP URL.
  const char* kReloadableUrlPrefixes[] = {
      chrome::kChromeUIDownloadsURL,
      chrome::kChromeUIHistoryURL,
      chrome::kChromeUINewTabURL,
      chrome::kChromeUISettingsURL,
  };
  // Prefix-match against the table above. Use strncmp to avoid allocating
  // memory to convert the URL prefix constants into std::strings.
  for (size_t i = 0; i < arraysize(kReloadableUrlPrefixes); ++i) {
    if (!strncmp(url.spec().c_str(),
                 kReloadableUrlPrefixes[i],
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
        LOG(WARNING) << "Discarding tab " << idx
                     << " id " << target_web_contents_id;
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
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Tabs.Discard.DiscardCount", discard_count_, 1, 1000, 50);

  // TODO(jamescook): Maybe incorporate extension count?
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Tabs.Discard.TabCount", GetTabCount(), 1, 100, 50);

  // Record the discarded tab in relation to the amount of simultaneously
  // logged in users.
  ash::MultiProfileUMA::RecordDiscardedTab(
      ash::Shell::GetInstance()->session_state_delegate()->
          NumberOfLoggedInUsers());

  // TODO(jamescook): If the time stats prove too noisy, then divide up users
  // based on how heavily they use Chrome using tab count as a proxy.
  // Bin into <= 1, <= 2, <= 4, <= 8, etc.
  if (last_discard_time_.is_null()) {
    // This is the first discard this session.
    TimeDelta interval = TimeTicks::Now() - start_time_;
    int interval_seconds = static_cast<int>(interval.InSeconds());
    // Record time in seconds over an interval of approximately 1 day.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Tabs.Discard.InitialTime2", interval_seconds, 1, 100000, 50);
  } else {
    // Not the first discard, so compute time since last discard.
    TimeDelta interval = TimeTicks::Now() - last_discard_time_;
    int interval_ms = static_cast<int>(interval.InMilliseconds());
    // Record time in milliseconds over an interval of approximately 1 day.
    // Start at 100 ms to get extra resolution in the target 750 ms range.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Tabs.Discard.IntervalTime2", interval_ms, 100, 100000 * 1000, 50);
  }
  // Record Chrome's concept of system memory usage at the time of the discard.
  base::SystemMemoryInfoKB memory;
  if (base::GetSystemMemoryInfo(&memory)) {
    // TODO(jamescook): Remove this after R25 is deployed to stable. It does
    // not have sufficient resolution in the 2-4 GB range and does not properly
    // account for graphics memory on ARM. Replace with MemAllocatedMB below.
    int mem_anonymous_mb = (memory.active_anon + memory.inactive_anon) / 1024;
    HISTOGRAM_MEGABYTES("Tabs.Discard.MemAnonymousMB", mem_anonymous_mb);

    // Record graphics GEM object size in a histogram with 50 MB buckets.
    int mem_graphics_gem_mb = 0;
    if (memory.gem_size != -1)
      mem_graphics_gem_mb = memory.gem_size / 1024 / 1024;
    RecordLinearHistogram(
        "Tabs.Discard.MemGraphicsMB", mem_graphics_gem_mb, 2500, 50);

    // Record shared memory (used by renderer/GPU buffers).
    int mem_shmem_mb = memory.shmem / 1024;
    RecordLinearHistogram("Tabs.Discard.MemShmemMB", mem_shmem_mb, 2500, 50);

    // On Intel, graphics objects are in anonymous pages, but on ARM they are
    // not. For a total "allocated count" add in graphics pages on ARM.
    int mem_allocated_mb = mem_anonymous_mb;
#if defined(ARCH_CPU_ARM_FAMILY)
    mem_allocated_mb += mem_graphics_gem_mb;
#endif
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Tabs.Discard.MemAllocatedMB", mem_allocated_mb, 256, 32768, 50);

    int mem_available_mb =
        (memory.active_file + memory.inactive_file + memory.free) / 1024;
    HISTOGRAM_MEGABYTES("Tabs.Discard.MemAvailableMB", mem_available_mb);
  }
  // Set up to record the next interval.
  last_discard_time_ = TimeTicks::Now();
}

void OomPriorityManager::RecordRecentTabDiscard() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  for (TabContentsIterator it; !it.done(); it.Next()) {
    WebContents* web_contents = *it;
    // Screenshots can consume ~5 MB per web contents for platforms that do
    // touch back/forward.
    web_contents->GetController().ClearAllScreenshots();
  }
  // TODO(jamescook): Are there other things we could flush? Drive metadata?
}

int OomPriorityManager::GetTabCount() const {
  int tab_count = 0;
  for (chrome::BrowserIterator it; !it.done(); it.Next())
    tab_count += it->tab_strip_model()->count();
  return tab_count;
}

// Returns true if |first| is considered less desirable to be killed
// than |second|.
bool OomPriorityManager::CompareTabStats(TabStats first,
                                         TabStats second) {
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
std::vector<base::ProcessHandle> OomPriorityManager::GetProcessHandles(
    const TabStatsList& stats_list) {
  std::vector<base::ProcessHandle> process_handles;
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

    process_handles.push_back(iterator->renderer_handle);
  }
  return process_handles;
}

void OomPriorityManager::AdjustOomPrioritiesOnFileThread(
    TabStatsList stats_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::AutoLock pid_to_oom_score_autolock(pid_to_oom_score_lock_);

  // Remove any duplicate PIDs. Order of the list is maintained, so each
  // renderer process will take on the oom_score_adj of the most important
  // (least likely to be killed) tab.
  std::vector<base::ProcessHandle> process_handles =
      GetProcessHandles(stats_list);

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
  const int kPriorityRange = chrome::kHighestRendererOomScore -
                             chrome::kLowestRendererOomScore;
  float priority_increment =
      static_cast<float>(kPriorityRange) / process_handles.size();
  for (std::vector<base::ProcessHandle>::iterator iterator =
           process_handles.begin();
       iterator != process_handles.end(); ++iterator) {
    int score = static_cast<int>(priority + 0.5f);
    ProcessScoreMap::iterator it = pid_to_oom_score_.find(*iterator);
    // If a process has the same score as the newly calculated value,
    // do not set it.
    if (it == pid_to_oom_score_.end() || it->second != score) {
      content::ZygoteHost::GetInstance()->AdjustRendererOOMScore(*iterator,
                                                                 score);
      pid_to_oom_score_[*iterator] = score;
    }
    priority += priority_increment;
  }
}

}  // namespace chromeos
