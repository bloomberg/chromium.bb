// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/memory/oom_memory_details.h"
#include "chrome/browser/memory/tab_manager_observer.h"
#include "chrome/browser/memory/tab_manager_web_contents_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/system_memory_stats_recorder.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/memory_pressure_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_importance_signals.h"

#if defined(OS_CHROMEOS)
#include "ash/common/multi_profile_uma.h"
#include "ash/common/wm_shell.h"
#include "chrome/browser/memory/tab_manager_delegate_chromeos.h"
#include "components/user_manager/user_manager.h"
#endif

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;
using content::WebContents;

namespace memory {
namespace {

// The default interval in seconds after which to adjust the oom_score_adj
// value.
const int kAdjustmentIntervalSeconds = 10;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
// For each period of this length record a statistic to indicate whether or not
// the user experienced a low memory event. If this interval is changed,
// Tabs.Discard.DiscardInLastMinute must be replaced with a new statistic.
const int kRecentTabDiscardIntervalSeconds = 60;
#endif

// If there has been no priority adjustment in this interval, assume the
// machine was suspended and correct the timing statistics.
const int kSuspendThresholdSeconds = kAdjustmentIntervalSeconds * 4;

// A suspended renderer is suspended for this duration.
constexpr base::TimeDelta kDurationOfRendererSuspension =
    base::TimeDelta::FromSeconds(1200);

// A resumed renderer is resumed for this duration.
constexpr base::TimeDelta kDurationOfRendererResumption =
    base::TimeDelta::FromSeconds(10);

// The time during which a tab is protected from discarding after it stops being
// audible.
const int kAudioProtectionTimeSeconds = 60;

int FindTabStripModelById(int64_t target_web_contents_id,
                          TabStripModel** model) {
  DCHECK(model);
  for (auto* browser : *BrowserList::GetInstance()) {
    TabStripModel* local_model = browser->tab_strip_model();
    for (int idx = 0; idx < local_model->count(); idx++) {
      WebContents* web_contents = local_model->GetWebContentsAt(idx);
      int64_t web_contents_id = TabManager::IdFromWebContents(web_contents);
      if (web_contents_id == target_web_contents_id) {
        *model = local_model;
        return idx;
      }
    }
  }

  return -1;
}

// A wrapper around base::MemoryPressureMonitor::GetCurrentPressureLevel.
// TODO(chrisha): Move this do the default implementation of a delegate.
base::MemoryPressureListener::MemoryPressureLevel
GetCurrentPressureLevel() {
  auto* monitor = base::MemoryPressureMonitor::Get();
  if (monitor)
    return monitor->GetCurrentPressureLevel();
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

// A wrapper to content::SendPressureNotification that doesn't have overloaded
// type ambiguity. Makes use of Bind easier.
// TODO(chrisha): Move this do the default implementation of a delegate.
void NotifyRendererProcess(
    const content::RenderProcessHost* render_process_host,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  content::MemoryPressureController::SendPressureNotification(
      render_process_host, level);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabManager

constexpr base::TimeDelta TabManager::kDefaultTimeToFirstPurge;

TabManager::TabManager()
    : discard_count_(0),
      recent_tab_discard_(false),
      discard_once_(false),
#if !defined(OS_CHROMEOS)
      minimum_protection_time_(base::TimeDelta::FromMinutes(10)),
#endif
      browser_tab_strip_tracker_(this, nullptr, nullptr),
      test_tick_clock_(nullptr),
      under_memory_pressure_(false),
      weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  delegate_.reset(new TabManagerDelegate(weak_ptr_factory_.GetWeakPtr()));
#endif
  browser_tab_strip_tracker_.Init(
      BrowserTabStripTracker::InitWith::ALL_BROWERS);

  // Set up default callbacks. These may be overridden post-construction as
  // testing seams.
  get_current_pressure_level_ = base::Bind(&GetCurrentPressureLevel);
  notify_renderer_process_ = base::Bind(&NotifyRendererProcess);
}

TabManager::~TabManager() {
  Stop();
}

void TabManager::Start() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  // Note that discarding is now enabled by default. This check is kept as a
  // kill switch.
  // TODO(georgesak): remote this when deemed not needed anymore.
  if (!base::FeatureList::IsEnabled(features::kAutomaticTabDiscarding))
    return;

  // Check the variation parameter to see if a tab is to be protected for an
  // amount of time after being backgrounded. The value is in seconds. Default
  // is 10 minutes if the variation is absent.
  std::string minimum_protection_time_string =
      variations::GetVariationParamValue(features::kAutomaticTabDiscarding.name,
                                         "MinimumProtectionTime");
  if (!minimum_protection_time_string.empty()) {
    unsigned int minimum_protection_time_seconds = 0;
    if (base::StringToUint(minimum_protection_time_string,
                           &minimum_protection_time_seconds)) {
      if (minimum_protection_time_seconds > 0)
        minimum_protection_time_ =
            base::TimeDelta::FromSeconds(minimum_protection_time_seconds);
    }
  }
#endif

  // Check if only one discard is allowed.
  discard_once_ = CanOnlyDiscardOnce();

  if (!update_timer_.IsRunning()) {
    update_timer_.Start(FROM_HERE,
                        TimeDelta::FromSeconds(kAdjustmentIntervalSeconds),
                        this, &TabManager::UpdateTimerCallback);
  }

  // MemoryPressureMonitor is not implemented on Linux so far and tabs are never
  // discarded.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  if (!recent_tab_discard_timer_.IsRunning()) {
    recent_tab_discard_timer_.Start(
        FROM_HERE, TimeDelta::FromSeconds(kRecentTabDiscardIntervalSeconds),
        this, &TabManager::RecordRecentTabDiscard);
  }
  start_time_ = NowTicks();
  // Create a |MemoryPressureListener| to listen for memory events.
  base::MemoryPressureMonitor* monitor = base::MemoryPressureMonitor::Get();
  if (monitor) {
    memory_pressure_listener_.reset(new base::MemoryPressureListener(
        base::Bind(&TabManager::OnMemoryPressure, base::Unretained(this))));
    base::MemoryPressureListener::MemoryPressureLevel level =
        monitor->GetCurrentPressureLevel();
    if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
      OnMemoryPressure(level);
    }
  }
#endif
  // purge-and-suspend param is used for Purge+Suspend finch experiment
  // in the following way:
  // https://docs.google.com/document/d/1hPHkKtXXBTlsZx9s-9U17XC-ofEIzPo9FYbBEc7PPbk/edit?usp=sharing
  std::string purge_and_suspend_time = variations::GetVariationParamValue(
      "PurgeAndSuspend", "purge-and-suspend-time");
  unsigned int time_to_first_purge_sec = 0;
  if (purge_and_suspend_time.empty() ||
      !base::StringToUint(purge_and_suspend_time, &time_to_first_purge_sec))
    time_to_first_suspension_ = kDefaultTimeToFirstPurge;
  else
    time_to_first_suspension_ =
        base::TimeDelta::FromSeconds(time_to_first_purge_sec);
}

void TabManager::Stop() {
  update_timer_.Stop();
  recent_tab_discard_timer_.Stop();
  memory_pressure_listener_.reset();
}

TabStatsList TabManager::GetTabStats() const {
  TabStatsList stats_list(GetUnsortedTabStats());

  // Sort the collected data so that least desirable to be killed is first, most
  // desirable is last.
  std::sort(stats_list.begin(), stats_list.end(), CompareTabStats);

  return stats_list;
}

std::vector<content::RenderProcessHost*>
TabManager::GetOrderedRenderers() const {
  // Get the tab stats.
  auto tab_stats = GetTabStats();

  std::vector<content::RenderProcessHost*> sorted_renderers;
  std::set<content::RenderProcessHost*> seen_renderers;
  std::set<content::RenderProcessHost*> visible_renderers;
  sorted_renderers.reserve(tab_stats.size());

  // Convert the tab sort order to a process sort order. The process inherits
  // the priority of its highest priority tab.
  for (auto& tab : tab_stats) {
    auto* renderer = tab.render_process_host;

    // Skip renderers associated with visible tabs as handling memory pressure
    // notifications in these processes can cause jank. This code works because
    // visible tabs always come first in |tab_stats|.
    if (tab.is_selected) {
      visible_renderers.insert(renderer);
      continue;
    }
    if (visible_renderers.count(renderer) > 0)
      continue;

    // Skip renderers that have already been encountered. This can occur when
    // multiple tabs are folded into a single renderer process. In this case the
    // process takes the priority of its highest priority contained tab.
    if (!seen_renderers.insert(renderer).second)
      continue;

    sorted_renderers.push_back(renderer);
  }

  return sorted_renderers;
}

bool TabManager::IsTabDiscarded(content::WebContents* contents) const {
  return GetWebContentsData(contents)->IsDiscarded();
}

bool TabManager::CanDiscardTab(int64_t target_web_contents_id) const {
  TabStripModel* model;
  int idx = FindTabStripModelById(target_web_contents_id, &model);

  if (idx == -1)
    return false;

  WebContents* web_contents = model->GetWebContentsAt(idx);

  // Do not discard tabs that don't have a valid URL (most probably they have
  // just been opened and dicarding them would lose the URL).
  // TODO(georgesak): Look into a workaround to be able to kill the tab without
  // losing the pending navigation.
  if (!web_contents->GetLastCommittedURL().is_valid() ||
      web_contents->GetLastCommittedURL().is_empty()) {
    return false;
  }

  // Do not discard tabs in which the user has entered text in a form, lest that
  // state gets lost.
  if (web_contents->GetPageImportanceSignals().had_form_interaction)
    return false;

  // Do not discard tabs that are playing either playing audio or accessing the
  // microphone or camera as it's too distruptive to the user experience. Note
  // that tabs that have recently stopped playing audio by at least
  // |kAudioProtectionTimeSeconds| seconds are protected as well.
  if (IsMediaTab(web_contents))
    return false;

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  // TODO(georgesak): Remove this workaround when the bugs are fixed.
  if (web_contents->GetContentsMimeType() == "application/pdf")
    return false;

  // Do not discard a previously discarded tab if that's the desired behavior.
  if (discard_once_ && GetWebContentsData(web_contents)->DiscardCount() > 0)
    return false;

  // Do not discard a recently used tab.
  if (minimum_protection_time_.InSeconds() > 0) {
    auto delta =
        NowTicks() - GetWebContentsData(web_contents)->LastInactiveTime();
    if (delta < minimum_protection_time_)
      return false;
  }

  // Do not discard a tab that was explicitly disallowed to.
  if (!IsTabAutoDiscardable(web_contents))
    return false;

  return true;
}

void TabManager::DiscardTab() {
#if defined(OS_CHROMEOS)
  // Call Chrome OS specific low memory handling process.
  if (base::FeatureList::IsEnabled(features::kArcMemoryManagement)) {
    delegate_->LowMemoryKill(GetUnsortedTabStats());
    return;
  }
#endif
  DiscardTabImpl();
}

WebContents* TabManager::DiscardTabById(int64_t target_web_contents_id) {
  TabStripModel* model;
  int index = FindTabStripModelById(target_web_contents_id, &model);

  if (index == -1)
    return nullptr;

  VLOG(1) << "Discarding tab " << index << " id " << target_web_contents_id;

  return DiscardWebContentsAt(index, model);
}

WebContents* TabManager::DiscardTabByExtension(content::WebContents* contents) {
  if (contents)
    return DiscardTabById(IdFromWebContents(contents));

  return DiscardTabImpl();
}

void TabManager::LogMemoryAndDiscardTab() {
  LogMemory("Tab Discards Memory details",
            base::Bind(&TabManager::PurgeMemoryAndDiscardTab));
}

void TabManager::LogMemory(const std::string& title,
                           const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OomMemoryDetails::Log(title, callback);
}

void TabManager::set_test_tick_clock(base::TickClock* test_tick_clock) {
  test_tick_clock_ = test_tick_clock;
}

// Things to collect on the browser thread (because TabStripModel isn't thread
// safe):
// 1) whether or not a tab is pinned
// 2) last time a tab was selected
// 3) is the tab currently selected
TabStatsList TabManager::GetUnsortedTabStats() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TabStatsList stats_list;
  stats_list.reserve(32);  // 99% of users have < 30 tabs open.

  // TODO(chrisha): Move this code to a TabStripModel enumeration delegate!
  if (!test_tab_strip_models_.empty()) {
    for (size_t i = 0; i < test_tab_strip_models_.size(); ++i) {
      AddTabStats(test_tab_strip_models_[i].first,   // tab_strip_model
                  test_tab_strip_models_[i].second,  // is_app
                  i == 0,                            // is_active
                  &stats_list);
    }
  } else {
    // The code here can only be tested under a full browser test.
    AddTabStats(&stats_list);
  }

  return stats_list;
}

void TabManager::AddObserver(TabManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void TabManager::RemoveObserver(TabManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabManager::set_minimum_protection_time_for_tests(
    base::TimeDelta minimum_protection_time) {
  minimum_protection_time_ = minimum_protection_time;
}

bool TabManager::IsTabAutoDiscardable(content::WebContents* contents) const {
  return GetWebContentsData(contents)->IsAutoDiscardable();
}

void TabManager::SetTabAutoDiscardableState(content::WebContents* contents,
                                            bool state) {
  GetWebContentsData(contents)->SetAutoDiscardableState(state);
}

content::WebContents* TabManager::GetWebContentsById(
    int64_t tab_contents_id) const {
  TabStripModel* model = nullptr;
  int index = FindTabStripModelById(tab_contents_id, &model);
  if (index == -1)
    return nullptr;
  return model->GetWebContentsAt(index);
}

bool TabManager::CanSuspendBackgroundedRenderer(int render_process_id) const {
  // A renderer can be suspended if it's not playing media.
  auto tab_stats = GetUnsortedTabStats();
  for (auto& tab : tab_stats) {
    if (tab.child_process_host_id != render_process_id)
      continue;
    WebContents* web_contents = GetWebContentsById(tab.tab_contents_id);
    if (!web_contents)
      return false;
    if (IsMediaTab(web_contents))
      return false;
  }
  return true;
}

// static
bool TabManager::CompareTabStats(const TabStats& first,
                                 const TabStats& second) {
  // Being currently selected is most important to protect.
  if (first.is_selected != second.is_selected)
    return first.is_selected;

  // Non auto-discardable tabs are more important to protect.
  if (first.is_auto_discardable != second.is_auto_discardable)
    return !first.is_auto_discardable;

  // Protect tabs with pending form entries.
  if (first.has_form_entry != second.has_form_entry)
    return first.has_form_entry;

  // Protect streaming audio and video conferencing tabs as these are similar to
  // active tabs.
  if (first.is_media != second.is_media)
    return first.is_media;

  // Tab with internal web UI like NTP or Settings are good choices to discard,
  // so protect non-Web UI and let the other conditionals finish the sort.
  if (first.is_internal_page != second.is_internal_page)
    return !first.is_internal_page;

  // Being pinned is important to protect.
  if (first.is_pinned != second.is_pinned)
    return first.is_pinned;

  // Being an app is important too, as it's the only visible surface in the
  // window and should not be discarded.
  if (first.is_app != second.is_app)
    return first.is_app;

  // TODO(jamescook): Incorporate sudden_termination_allowed into the sort
  // order. This is currently not done because pages with unload handlers set
  // sudden_termination_allowed false, and that covers too many common pages
  // with ad networks and statistics scripts.  Ideally check for beforeUnload
  // handlers, which are likely to present a dialog asking if the user wants to
  // discard state.  crbug.com/123049.

  // Being more recently active is more important.
  return first.last_active > second.last_active;
}

// static
int64_t TabManager::IdFromWebContents(WebContents* web_contents) {
  return reinterpret_cast<int64_t>(web_contents);
}

///////////////////////////////////////////////////////////////////////////////
// TabManager, private:

void TabManager::OnDiscardedStateChange(content::WebContents* contents,
                                        bool is_discarded) {
  for (TabManagerObserver& observer : observers_)
    observer.OnDiscardedStateChange(contents, is_discarded);
}

void TabManager::OnAutoDiscardableStateChange(content::WebContents* contents,
                                              bool is_auto_discardable) {
  for (TabManagerObserver& observer : observers_)
    observer.OnAutoDiscardableStateChange(contents, is_auto_discardable);
}

// static
void TabManager::PurgeMemoryAndDiscardTab() {
  TabManager* manager = g_browser_process->GetTabManager();
  manager->PurgeBrowserMemory();
  manager->DiscardTab();
}

// static
bool TabManager::IsInternalPage(const GURL& url) {
  // There are many chrome:// UI URLs, but only look for the ones that users
  // are likely to have open. Most of the benefit is the from NTP URL.
  const char* const kInternalPagePrefixes[] = {
      chrome::kChromeUIDownloadsURL, chrome::kChromeUIHistoryURL,
      chrome::kChromeUINewTabURL, chrome::kChromeUISettingsURL,
  };
  // Prefix-match against the table above. Use strncmp to avoid allocating
  // memory to convert the URL prefix constants into std::strings.
  for (size_t i = 0; i < arraysize(kInternalPagePrefixes); ++i) {
    if (!strncmp(url.spec().c_str(), kInternalPagePrefixes[i],
                 strlen(kInternalPagePrefixes[i])))
      return true;
  }
  return false;
}

void TabManager::RecordDiscardStatistics() {
  discard_count_++;

  // TODO(jamescook): Maybe incorporate extension count?
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.Discard.TabCount", GetTabCount(), 1, 100,
                              50);
#if defined(OS_CHROMEOS)
  // Record the discarded tab in relation to the amount of simultaneously
  // logged in users.
  if (ash::WmShell::HasInstance()) {
    ash::MultiProfileUMA::RecordDiscardedTab(
        user_manager::UserManager::Get()->GetLoggedInUsers().size());
  }
#endif
  // TODO(jamescook): If the time stats prove too noisy, then divide up users
  // based on how heavily they use Chrome using tab count as a proxy.
  // Bin into <= 1, <= 2, <= 4, <= 8, etc.
  if (last_discard_time_.is_null()) {
    // This is the first discard this session.
    TimeDelta interval = NowTicks() - start_time_;
    int interval_seconds = static_cast<int>(interval.InSeconds());
    // Record time in seconds over an interval of approximately 1 day.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.Discard.InitialTime2", interval_seconds,
                                1, 100000, 50);
  } else {
    // Not the first discard, so compute time since last discard.
    TimeDelta interval = NowTicks() - last_discard_time_;
    int interval_ms = static_cast<int>(interval.InMilliseconds());
    // Record time in milliseconds over an interval of approximately 1 day.
    // Start at 100 ms to get extra resolution in the target 750 ms range.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.Discard.IntervalTime2", interval_ms, 100,
                                100000 * 1000, 50);
  }
// TODO(georgesak): Remove this #if when RecordMemoryStats is implemented for
// all platforms.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // Record system memory usage at the time of the discard.
  metrics::RecordMemoryStats(metrics::RECORD_MEMORY_STATS_TAB_DISCARDED);
#endif
  // Set up to record the next interval.
  last_discard_time_ = NowTicks();
}

void TabManager::RecordRecentTabDiscard() {
  // If Chrome is shutting down, do not do anything.
  if (g_browser_process->IsShuttingDown())
    return;

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the interval is changed, so should the histogram name.
  UMA_HISTOGRAM_BOOLEAN("Tabs.Discard.DiscardInLastMinute",
                        recent_tab_discard_);
  // Reset for the next interval.
  recent_tab_discard_ = false;
}

void TabManager::PurgeBrowserMemory() {
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

int TabManager::GetTabCount() const {
  int tab_count = 0;
  for (auto* browser : *BrowserList::GetInstance())
    tab_count += browser->tab_strip_model()->count();
  return tab_count;
}

void TabManager::AddTabStats(TabStatsList* stats_list) const {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator browser_iterator =
           browser_list->begin_last_active();
       browser_iterator != browser_list->end_last_active();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;
    // |is_active_window| tells us whether this browser window is active. It is
    // possible that none of the browser windows is active because it's some
    // other application window in the foreground.
    bool is_active_window = browser->window()->IsActive();
    AddTabStats(browser->tab_strip_model(), browser->is_app(), is_active_window,
                stats_list);
  }
}

void TabManager::AddTabStats(const TabStripModel* model,
                             bool is_app,
                             bool active_model,
                             TabStatsList* stats_list) const {
  for (int i = 0; i < model->count(); i++) {
    WebContents* contents = model->GetWebContentsAt(i);
    if (!contents->IsCrashed()) {
      TabStats stats;
      stats.is_app = is_app;
      stats.is_internal_page = IsInternalPage(contents->GetLastCommittedURL());
      stats.is_media = IsMediaTab(contents);
      stats.is_pinned = model->IsTabPinned(i);
      stats.is_selected = active_model && model->IsTabSelected(i);
      stats.is_discarded = GetWebContentsData(contents)->IsDiscarded();
      stats.has_form_entry =
          contents->GetPageImportanceSignals().had_form_interaction;
      stats.discard_count = GetWebContentsData(contents)->DiscardCount();
      stats.last_active = contents->GetLastActiveTime();
      stats.last_hidden = contents->GetLastHiddenTime();
      stats.render_process_host = contents->GetRenderProcessHost();
      stats.renderer_handle = contents->GetRenderProcessHost()->GetHandle();
      stats.child_process_host_id = contents->GetRenderProcessHost()->GetID();
#if defined(OS_CHROMEOS)
      stats.oom_score = delegate_->GetCachedOomScore(stats.renderer_handle);
#endif
      stats.title = contents->GetTitle();
      stats.tab_contents_id = IdFromWebContents(contents);
      stats_list->push_back(stats);
    }
  }
}

// This function is called when |update_timer_| fires. It will adjust the clock
// if needed (if it detects that the machine was asleep) and will fire the stats
// updating on ChromeOS via the delegate. This function also tries to purge
// cache memory and suspend tabs which becomes and keeps backgrounded for a
// while.
void TabManager::UpdateTimerCallback() {
  // If Chrome is shutting down, do not do anything.
  if (g_browser_process->IsShuttingDown())
    return;

  if (BrowserList::GetInstance()->empty())
    return;

  // Check for a discontinuity in time caused by the machine being suspended.
  if (!last_adjust_time_.is_null()) {
    TimeDelta suspend_time = NowTicks() - last_adjust_time_;
    if (suspend_time.InSeconds() > kSuspendThresholdSeconds) {
      // System was probably suspended, move the event timers forward in time so
      // when they get subtracted out later, "uptime" is being counted.
      start_time_ += suspend_time;
      if (!last_discard_time_.is_null())
        last_discard_time_ += suspend_time;
    }
  }
  last_adjust_time_ = NowTicks();

#if defined(OS_CHROMEOS)
  TabStatsList stats_list = GetTabStats();
  // This starts the CrOS specific OOM adjustments in /proc/<pid>/oom_score_adj.
  delegate_->AdjustOomPriorities(stats_list);
#endif

  PurgeAndSuspendBackgroundedTabs();
}

TabManager::PurgeAndSuspendState TabManager::GetNextPurgeAndSuspendState(
    content::WebContents* content,
    base::TimeTicks current_time,
    const base::TimeDelta& time_to_first_suspension) const {
  DCHECK(content);
  PurgeAndSuspendState state =
      GetWebContentsData(content)->GetPurgeAndSuspendState();

  auto time_passed = current_time -
      GetWebContentsData(content)->LastPurgeAndSuspendModifiedTime();
  switch (state) {
    case RUNNING:
      if (time_passed > time_to_first_suspension)
        return SUSPENDED;
      break;
    case RESUMED:
      if (time_passed > kDurationOfRendererResumption)
        return SUSPENDED;
      break;
    case SUSPENDED:
      if (time_passed > kDurationOfRendererSuspension)
        return RESUMED;
      break;
  }
  return state;
}

void TabManager::PurgeAndSuspendBackgroundedTabs() {
  base::TimeTicks current_time = NowTicks();
  auto tab_stats = GetUnsortedTabStats();
  for (auto& tab : tab_stats) {
    if (!tab.render_process_host->IsProcessBackgrounded())
      continue;
    if (!CanSuspendBackgroundedRenderer(tab.child_process_host_id))
      continue;

    WebContents* content = GetWebContentsById(tab.tab_contents_id);
    if (!content)
      continue;

    PurgeAndSuspendState current_state =
        GetWebContentsData(content)->GetPurgeAndSuspendState();
    // If the tab's purge-and-suspend state is not RUNNING, the tab should be
    // backgrounded. Since tab.last_hidden is updated everytime the tab is
    // hidden, we should see tab.last_hidden < last_modified_time.
    DCHECK(current_state == RUNNING ||
           tab.last_hidden <
               GetWebContentsData(content)->LastPurgeAndSuspendModifiedTime());
    PurgeAndSuspendState next_state = GetNextPurgeAndSuspendState(
        content, current_time, time_to_first_suspension_);
    if (current_state == next_state)
      continue;

    // TODO(hajimehoshi): Now calling PurgeAndSuspend is implemented without
    // timers for simplicity, so PurgeAndSuspend is called even after the
    // renderer is purged and suspended once. This should be replaced with
    // timers if we want necessary and sufficient signals.
    GetWebContentsData(content)->SetPurgeAndSuspendState(next_state);
    switch (next_state) {
      case SUSPENDED:
        tab.render_process_host->PurgeAndSuspend();
        break;
      case RESUMED:
        tab.render_process_host->Resume();
        break;
      case RUNNING:
        NOTREACHED();
    }
  }
}

WebContents* TabManager::DiscardWebContentsAt(int index, TabStripModel* model) {
  // Can't discard active index.
  if (model->active_index() == index)
    return nullptr;

  WebContents* old_contents = model->GetWebContentsAt(index);

  // Can't discard tabs that are already discarded.
  if (GetWebContentsData(old_contents)->IsDiscarded())
    return nullptr;

  // Record statistics before discarding to capture the memory state that leads
  // to the discard.
  RecordDiscardStatistics();

  UMA_HISTOGRAM_BOOLEAN(
      "TabManager.Discarding.DiscardedTabHasBeforeUnloadHandler",
      old_contents->NeedToFireBeforeUnload());

  WebContents* null_contents =
      WebContents::Create(WebContents::CreateParams(model->profile()));
  // Copy over the state from the navigation controller to preserve the
  // back/forward history and to continue to display the correct title/favicon.
  null_contents->GetController().CopyStateFrom(old_contents->GetController());

  // Make sure to persist the last active time property.
  null_contents->SetLastActiveTime(old_contents->GetLastActiveTime());
  // Copy over the discard count.
  WebContentsData::CopyState(old_contents, null_contents);

  // Replace the discarded tab with the null version.
  model->ReplaceWebContentsAt(index, null_contents);
  // Mark the tab so it will reload when clicked on.
  GetWebContentsData(null_contents)->SetDiscardState(true);
  GetWebContentsData(null_contents)->IncrementDiscardCount();

  // Discard the old tab's renderer.
  // TODO(jamescook): This breaks script connections with other tabs.
  // Find a different approach that doesn't do that, perhaps based on navigation
  // to swappedout://.
  delete old_contents;
  recent_tab_discard_ = true;

  return null_contents;
}

void TabManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // If Chrome is shutting down, do not do anything.
  if (g_browser_process->IsShuttingDown())
    return;

  // If no task runner has been set, then use the same one that the memory
  // pressure subsystem uses.
  if (!task_runner_.get())
    task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Under critical pressure try to discard a tab.
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    LogMemoryAndDiscardTab();
  }
  // TODO(skuhne): If more memory pressure levels are introduced, consider
  // calling PurgeBrowserMemory() before CRITICAL is reached.

  // If this is the beginning of a period of memory pressure then kick off
  // notification of child processes.
  // NOTE: This mechanism relies on having a MemoryPressureMonitor
  // implementation that supports "CurrentPressureLevel". This is true on all
  // platforms on which TabManager is used.
#if !defined(OS_CHROMEOS)
  // Running GC under memory pressure can cause thrashing. Disable it on
  // ChromeOS until the thrashing is fixed. crbug.com/588172.
  if (!under_memory_pressure_)
    DoChildProcessDispatch();
#endif
}

void TabManager::TabChangedAt(content::WebContents* contents,
                              int index,
                              TabChangeType change_type) {
  if (change_type != TabChangeType::ALL)
    return;
  auto* data = GetWebContentsData(contents);
  bool old_state = data->IsRecentlyAudible();
  bool current_state = contents->WasRecentlyAudible();
  if (old_state != current_state) {
    data->SetRecentlyAudible(current_state);
    data->SetLastAudioChangeTime(NowTicks());
  }
}

void TabManager::ActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) {
  GetWebContentsData(new_contents)->SetDiscardState(false);
  GetWebContentsData(new_contents)->SetPurgeAndSuspendState(RUNNING);
  // If |old_contents| is set, that tab has switched from being active to
  // inactive, so record the time of that transition.
  if (old_contents)
    GetWebContentsData(old_contents)->SetLastInactiveTime(NowTicks());
}

void TabManager::TabInsertedAt(TabStripModel* tab_strip_model,
                               content::WebContents* contents,
                               int index,
                               bool foreground) {
  // Only interested in background tabs, as foreground tabs get taken care of by
  // ActiveTabChanged.
  if (foreground)
    return;

  // A new background tab is similar to having a tab switch from being active to
  // inactive.
  GetWebContentsData(contents)->SetLastInactiveTime(NowTicks());
}

bool TabManager::IsMediaTab(WebContents* contents) const {
  if (contents->WasRecentlyAudible())
    return true;

  scoped_refptr<MediaStreamCaptureIndicator> media_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  if (media_indicator->IsCapturingUserMedia(contents) ||
      media_indicator->IsBeingMirrored(contents)) {
    return true;
  }

  auto delta = NowTicks() - GetWebContentsData(contents)->LastAudioChangeTime();
  return delta < TimeDelta::FromSeconds(kAudioProtectionTimeSeconds);
}

TabManager::WebContentsData* TabManager::GetWebContentsData(
    content::WebContents* contents) const {
  WebContentsData::CreateForWebContents(contents);
  auto* web_contents_data = WebContentsData::FromWebContents(contents);
  web_contents_data->set_test_tick_clock(test_tick_clock_);
  return web_contents_data;
}

TimeTicks TabManager::NowTicks() const {
  if (!test_tick_clock_)
    return TimeTicks::Now();

  return test_tick_clock_->NowTicks();
}

void TabManager::DoChildProcessDispatch() {
  // If Chrome is shutting down, do not do anything.
  if (g_browser_process->IsShuttingDown())
    return;

  if (!under_memory_pressure_)
    under_memory_pressure_ = true;

  // If the memory pressure condition has ended then stop dispatching messages.
  auto level = get_current_pressure_level_.Run();
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    under_memory_pressure_ = false;
    notified_renderers_.clear();
    return;
  }

  // Get a vector of active renderers, from highest to lowest priority.
  auto renderers = GetOrderedRenderers();

  // The following code requires at least one renderer to be present or it will
  // busyloop. It's possible for no renderers to exist (we eliminate visible
  // renderers to avoid janking them), so bail early if that's the case.
  if (renderers.empty())
    return;

  // Notify a single renderer of memory pressure.
  bool notified = false;
  while (!notified) {
    // Notify the lowest priority renderer that hasn't been notified yet.
    for (auto rit = renderers.rbegin(); rit != renderers.rend(); ++rit) {
      // If this renderer has already been notified then look at the next one.
      if (!notified_renderers_.insert(*rit).second)
        continue;

      // Notify the renderer.
      notify_renderer_process_.Run(*rit, level);
      notified = true;
      break;
    }

    // If all renderers were processed and none were notified, then all
    // renderers have already been notified. Clear the list and start again.
    if (!notified)
      notified_renderers_.clear();

    // This loop can only run at most twice. If it doesn't exit the first time
    // through, by the second time through |notified_renderers_| will be empty.
    // Since |renderers| is always non-empty, the first renderer encountered
    // during the second pass will be notified.
  }

  // Schedule another notification. Use a weak pointer so this doesn't explode
  // during tear down.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TabManager::DoChildProcessDispatch,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kRendererNotificationDelayInSeconds));
}

// TODO(jamescook): This should consider tabs with references to other tabs,
// such as tabs created with JavaScript window.open(). Potentially consider
// discarding the entire set together, or use that in the priority computation.
content::WebContents* TabManager::DiscardTabImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TabStatsList stats = GetTabStats();

  if (stats.empty())
    return nullptr;
  // Loop until a non-discarded tab to kill is found.
  for (TabStatsList::const_reverse_iterator stats_rit = stats.rbegin();
       stats_rit != stats.rend(); ++stats_rit) {
    int64_t least_important_tab_id = stats_rit->tab_contents_id;
    if (CanDiscardTab(least_important_tab_id)) {
      WebContents* new_contents = DiscardTabById(least_important_tab_id);
      if (new_contents)
        return new_contents;
    }
  }
  return nullptr;
}

// Check the variation parameter to see if a tab can be discarded only once or
// multiple times.
// Default is to only discard once per tab.
bool TabManager::CanOnlyDiscardOnce() const {
#if defined(OS_WIN) || defined(OS_MACOSX)
  // On Windows and MacOS, default to discarding only once unless otherwise
  // specified by the variation parameter.
  // TODO(georgesak): Add Linux when automatic discarding is enabled for that
  // platform.
  std::string allow_multiple_discards = variations::GetVariationParamValue(
      features::kAutomaticTabDiscarding.name, "AllowMultipleDiscards");
  return (allow_multiple_discards != "true");
#else
  return false;
#endif
}

}  // namespace memory
