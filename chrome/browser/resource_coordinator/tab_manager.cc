// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <stddef.h>

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event_argument.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/memory/oom_memory_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_web_contents_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/system_memory_stats_recorder.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_importance_signals.h"
#include "third_party/WebKit/public/platform/WebSuddenTerminationDisablerType.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_CHROMEOS)
#include "ash/multi_profile_uma.h"
#include "ash/shell.h"
#include "chrome/browser/resource_coordinator/tab_manager_delegate_chromeos.h"
#include "components/user_manager/user_manager.h"
#endif

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;
using content::WebContents;

namespace resource_coordinator {
namespace {

// The timeout time after which the next background tab gets loaded if the
// previous tab has not finished loading yet. This is ignored in kPaused loading
// mode.
const TimeDelta kBackgroundTabLoadTimeout = TimeDelta::FromSeconds(10);

// The number of loading slots for background tabs. TabManager will start to
// load the next background tab when the loading slots free up.
const size_t kNumOfLoadingSlots = 1;

// The default interval in seconds after which to adjust the oom_score_adj
// value.
const int kAdjustmentIntervalSeconds = 10;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
// For each period of this length record a statistic to indicate whether or not
// the user experienced a low memory event. If this interval is changed,
// Tabs.Discard.DiscardInLastMinute must be replaced with a new statistic.
const int kRecentTabDiscardIntervalSeconds = 60;
#endif

// The time during which a tab is protected from discarding after it stops being
// audible.
const int kAudioProtectionTimeSeconds = 60;

int FindWebContentsById(const TabStripModel* model,
                        int64_t target_web_contents_id) {
  for (int idx = 0; idx < model->count(); idx++) {
    WebContents* web_contents = model->GetWebContentsAt(idx);
    int64_t web_contents_id = TabManager::IdFromWebContents(web_contents);
    if (web_contents_id == target_web_contents_id)
      return idx;
  }

  return -1;
}

void ReloadWebContentsIfDiscarded(WebContents* contents,
                                  TabManager::WebContentsData* contents_data) {
  if (contents_data->IsDiscarded()) {
    contents->GetController().SetNeedsReload();
    contents->GetController().LoadIfNecessary();
    contents_data->SetDiscardState(false);
  }
}

// Returns a set with browsers in |browser_info_list| that are completely
// covered by another browser in |browser_info_list| (some browsers that match
// this description might not be included in the set if insufficient z-order
// information is provided). Non-browser windows are not taken into
// consideration when computing window occlusion, because there is no simple way
// to know whether they opaquely fill their bounds.
//
// TODO(fdoray): Handle the case where a browser window is completely covered by
// the union of other browser windows but not by a single browser window.
base::flat_set<const BrowserInfo*> GetOccludedBrowsers(
    const std::vector<BrowserInfo>& browser_info_list,
    const std::vector<gfx::NativeWindow>& windows_sorted_by_z_index) {
  base::flat_set<const BrowserInfo*> occluded_browsers;
  std::vector<gfx::Rect> bounds_of_previous_browsers;

  // Traverse windows from topmost to bottommost.
  for (gfx::NativeWindow native_window : windows_sorted_by_z_index) {
    // Find the BrowserInfo corresponding to the current NativeWindow.
    auto browser_info_it = std::find_if(
        browser_info_list.begin(), browser_info_list.end(),
        [&native_window](const BrowserInfo& browser_info) {
          return browser_info.browser->window()->GetNativeWindow() ==
                 native_window;
        });

    // Skip the current NativeWindow if no browser is associated with it or if
    // the associated browser is minimized.
    if (browser_info_it == browser_info_list.end() ||
        browser_info_it->browser->window()->IsMinimized()) {
      continue;
    }

    // Determine if the browser window is occluded by looking for a previously
    // traversed browser window that completely covers it.]
    bool browser_is_occluded = false;
    const gfx::Rect bounds = browser_info_it->browser->window()->GetBounds();
    for (const gfx::Rect other_bounds : bounds_of_previous_browsers) {
      if (other_bounds.Contains(bounds)) {
        browser_is_occluded = true;
        break;
      }
    }

    // Add the current browser to the list of occluded browsers if
    // |browser_is_occluded| is true. Otherwise, add the current window bounds
    // to |bounds_of_previous_browsers| for use in future window occlusion
    // computations.
    if (browser_is_occluded)
      occluded_browsers.insert(&*browser_info_it);
    else
      bounds_of_previous_browsers.push_back(bounds);
  }

  return occluded_browsers;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat> DataAsTraceValue(
    TabManager::BackgroundTabLoadingMode mode,
    size_t num_of_pending_navigations,
    size_t num_of_loading_contents) {
  std::unique_ptr<base::trace_event::TracedValue> data(
      new base::trace_event::TracedValue());
  data->SetInteger("background_tab_loading_mode", mode);
  data->SetInteger("num_of_pending_navigations", num_of_pending_navigations);
  data->SetInteger("num_of_loading_contents", num_of_loading_contents);
  return std::move(data);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabManager

class TabManager::TabManagerSessionRestoreObserver final
    : public SessionRestoreObserver {
 public:
  explicit TabManagerSessionRestoreObserver(TabManager* tab_manager)
      : tab_manager_(tab_manager) {
    SessionRestore::AddObserver(this);
  }

  ~TabManagerSessionRestoreObserver() { SessionRestore::RemoveObserver(this); }

  // SessionRestoreObserver implementation:
  void OnSessionRestoreStartedLoadingTabs() override {
    tab_manager_->OnSessionRestoreStartedLoadingTabs();
  }

  void OnSessionRestoreFinishedLoadingTabs() override {
    tab_manager_->OnSessionRestoreFinishedLoadingTabs();
  }

  void OnWillRestoreTab(WebContents* web_contents) override {
    tab_manager_->OnWillRestoreTab(web_contents);
  }

 private:
  TabManager* tab_manager_;
};

constexpr base::TimeDelta TabManager::kDefaultMinTimeToPurge;

TabManager::TabManager()
    : discard_count_(0),
      recent_tab_discard_(false),
      discard_once_(false),
#if !defined(OS_CHROMEOS)
      minimum_protection_time_(base::TimeDelta::FromMinutes(10)),
#endif
      browser_tab_strip_tracker_(this, nullptr, this),
      test_tick_clock_(nullptr),
      is_session_restore_loading_tabs_(false),
      background_tab_loading_mode_(BackgroundTabLoadingMode::kStaggered),
      force_load_timer_(base::MakeUnique<base::OneShotTimer>()),
      loading_slots_(kNumOfLoadingSlots),
      weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  delegate_.reset(new TabManagerDelegate(weak_ptr_factory_.GetWeakPtr()));
#endif
  browser_tab_strip_tracker_.Init();
  session_restore_observer_.reset(new TabManagerSessionRestoreObserver(this));
  stats_collector_.reset(new TabManagerStatsCollector());
}

TabManager::~TabManager() {
  Stop();
}

void TabManager::Start() {
  background_tab_loading_mode_ = BackgroundTabLoadingMode::kStaggered;

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
  // Create a |MemoryPressureListener| to listen for memory events when
  // MemoryCoordinator is disabled. When MemoryCoordinator is enabled
  // it asks TabManager to do tab discarding.
  base::MemoryPressureMonitor* monitor = base::MemoryPressureMonitor::Get();
  if (monitor && !base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
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
      "PurgeAndSuspendAggressive", "purge-and-suspend-time");
  unsigned int min_time_to_purge_sec = 0;
  if (purge_and_suspend_time.empty() ||
      !base::StringToUint(purge_and_suspend_time, &min_time_to_purge_sec))
    min_time_to_purge_ = kDefaultMinTimeToPurge;
  else
    min_time_to_purge_ = base::TimeDelta::FromSeconds(min_time_to_purge_sec);

  std::string max_purge_and_suspend_time = variations::GetVariationParamValue(
      "PurgeAndSuspendAggressive", "max-purge-and-suspend-time");
  unsigned int max_time_to_purge_sec = 0;
  // If max-purge-and-suspend-time is not specified or
  // max-purge-and-suspend-time is not valid (not number or smaller than
  // min-purge-and-suspend-time), use default max-time-to-purge, i.e.
  // min-time-to-purge times kDefaultMinMaxTimeToPurgeRatio.
  if (max_purge_and_suspend_time.empty() ||
      !base::StringToUint(max_purge_and_suspend_time, &max_time_to_purge_sec) ||
      max_time_to_purge_sec < min_time_to_purge_.InSeconds())
    max_time_to_purge_ = min_time_to_purge_ * kDefaultMinMaxTimeToPurgeRatio;
  else
    max_time_to_purge_ = base::TimeDelta::FromSeconds(max_time_to_purge_sec);
}

void TabManager::Stop() {
  update_timer_.Stop();
  recent_tab_discard_timer_.Stop();
  force_load_timer_->Stop();
  memory_pressure_listener_.reset();
}

int TabManager::FindTabStripModelById(int64_t target_web_contents_id,
                                      TabStripModel** model) const {
  DCHECK(model);

  for (const auto& browser_info : GetBrowserInfoList()) {
    TabStripModel* local_model = browser_info.tab_strip_model;
    int idx = FindWebContentsById(local_model, target_web_contents_id);
    if (idx != -1) {
      *model = local_model;
      return idx;
    }
  }

  return -1;
}

TabStatsList TabManager::GetTabStats() const {
  TabStatsList stats_list(GetUnsortedTabStats());

  // Sort the collected data so that least desirable to be killed is first, most
  // desirable is last.
  std::sort(stats_list.begin(), stats_list.end(), CompareTabStats);

  return stats_list;
}

bool TabManager::IsTabDiscarded(content::WebContents* contents) const {
  return GetWebContentsData(contents)->IsDiscarded();
}

bool TabManager::CanDiscardTab(const TabStats& tab_stats) const {
#if defined(OS_CHROMEOS)
  if (tab_stats.is_active && tab_stats.is_in_visible_window)
    return false;
#else
  if (tab_stats.is_active)
    return false;
#endif  // defined(OS_CHROMEOS)

  TabStripModel* model;
  const int idx = FindTabStripModelById(tab_stats.tab_contents_id, &model);

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

void TabManager::DiscardTab(DiscardTabCondition condition) {
#if defined(OS_CHROMEOS)
  // Call Chrome OS specific low memory handling process.
  if (base::FeatureList::IsEnabled(features::kArcMemoryManagement)) {
    delegate_->LowMemoryKill(condition);
    return;
  }
#endif
  DiscardTabImpl(condition);
}

WebContents* TabManager::DiscardTabById(int64_t target_web_contents_id,
                                        DiscardTabCondition condition) {
  TabStripModel* model;
  int index = FindTabStripModelById(target_web_contents_id, &model);

  if (index == -1)
    return nullptr;

  VLOG(1) << "Discarding tab " << index << " id " << target_web_contents_id;

  return DiscardWebContentsAt(index, model, condition);
}

WebContents* TabManager::DiscardTabByExtension(content::WebContents* contents) {
  if (contents)
    return DiscardTabById(IdFromWebContents(contents), kProactiveShutdown);

  return DiscardTabImpl(kProactiveShutdown);
}

void TabManager::LogMemoryAndDiscardTab(DiscardTabCondition condition) {
  LogMemory("Tab Discards Memory details",
            base::Bind(&TabManager::PurgeMemoryAndDiscardTab, condition));
}

void TabManager::LogMemory(const std::string& title,
                           const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  memory::OomMemoryDetails::Log(title, callback);
}

void TabManager::set_test_tick_clock(base::TickClock* test_tick_clock) {
  test_tick_clock_ = test_tick_clock;
  force_load_timer_.reset(new base::OneShotTimer(test_tick_clock));
}

TabStatsList TabManager::GetUnsortedTabStats(
    const std::vector<gfx::NativeWindow>& windows_sorted_by_z_index) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const auto browser_info_list = GetBrowserInfoList();
  const base::flat_set<const BrowserInfo*> occluded_browsers =
      GetOccludedBrowsers(browser_info_list, windows_sorted_by_z_index);

  TabStatsList stats_list;
  stats_list.reserve(32);  // 99% of users have < 30 tabs open.
  for (const BrowserInfo& browser_info : browser_info_list) {
    const bool window_is_active = stats_list.empty();
    const bool window_is_visible =
        !browser_info.window_is_minimized &&
        !base::ContainsKey(occluded_browsers, &browser_info);
    AddTabStats(browser_info, window_is_active, window_is_visible, &stats_list);
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
  // A renderer can be purged if it's not playing media.
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
  // Protect non auto-discardable tabs.
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

  // A tab with a beforeunload handler usually indicates that the tab has
  // unsaved user state.
  if (first.has_beforeunload_handler != second.has_beforeunload_handler)
    return first.has_beforeunload_handler;

  // Being more recently active is more important.
  return first.last_active > second.last_active;
}

// static
int64_t TabManager::IdFromWebContents(WebContents* web_contents) {
  return reinterpret_cast<int64_t>(web_contents);
}

void TabManager::OnSessionRestoreStartedLoadingTabs() {
  DCHECK(!is_session_restore_loading_tabs_);
  is_session_restore_loading_tabs_ = true;
}

void TabManager::OnSessionRestoreFinishedLoadingTabs() {
  DCHECK(is_session_restore_loading_tabs_);
  is_session_restore_loading_tabs_ = false;
}

bool TabManager::IsTabInSessionRestore(WebContents* web_contents) const {
  return GetWebContentsData(web_contents)->is_in_session_restore();
}

bool TabManager::IsTabRestoredInForeground(WebContents* web_contents) const {
  return GetWebContentsData(web_contents)->is_restored_in_foreground();
}

size_t TabManager::GetBackgroundTabLoadingCount() const {
  if (!IsInBackgroundTabOpeningSession())
    return 0;

  return loading_contents_.size();
}

size_t TabManager::GetBackgroundTabPendingCount() const {
  if (!IsInBackgroundTabOpeningSession())
    return 0;

  return pending_navigations_.size();
}

int TabManager::GetTabCount() const {
  int tab_count = 0;
  for (auto* browser : *BrowserList::GetInstance())
    tab_count += browser->tab_strip_model()->count();
  return tab_count;
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
void TabManager::PurgeMemoryAndDiscardTab(DiscardTabCondition condition) {
  TabManager* manager = g_browser_process->GetTabManager();
  manager->PurgeBrowserMemory();
  manager->DiscardTab(condition);
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
  if (ash::Shell::HasInstance()) {
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

void TabManager::AddTabStats(const BrowserInfo& browser_info,
                             bool window_is_active,
                             bool window_is_visible,
                             TabStatsList* stats_list) const {
  TabStripModel* tab_strip_model = browser_info.tab_strip_model;
  for (int i = 0; i < tab_strip_model->count(); i++) {
    WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    if (!contents->IsCrashed()) {
      TabStats stats;
      stats.is_app = browser_info.browser_is_app;
      stats.is_internal_page = IsInternalPage(contents->GetLastCommittedURL());
      stats.is_media = IsMediaTab(contents);
      stats.is_pinned = tab_strip_model->IsTabPinned(i);
      stats.is_active = tab_strip_model->active_index() == i;
      stats.is_in_active_window = window_is_active;
      stats.is_in_visible_window = window_is_visible;
      stats.is_discarded = GetWebContentsData(contents)->IsDiscarded();
      stats.has_form_entry =
          contents->GetPageImportanceSignals().had_form_interaction;
      stats.discard_count = GetWebContentsData(contents)->DiscardCount();
      stats.last_active = contents->GetLastActiveTime();
      stats.last_hidden = contents->GetLastHiddenTime();
      stats.render_process_host = contents->GetMainFrame()->GetProcess();
      stats.renderer_handle =
          contents->GetMainFrame()->GetProcess()->GetHandle();
      stats.child_process_host_id =
          contents->GetMainFrame()->GetProcess()->GetID();
#if defined(OS_CHROMEOS)
      stats.oom_score = delegate_->GetCachedOomScore(stats.renderer_handle);
#endif
      stats.title = contents->GetTitle();
      stats.tab_contents_id = IdFromWebContents(contents);
      content::RenderFrameHost* render_frame = contents->GetMainFrame();
      DCHECK(render_frame);
      stats.has_beforeunload_handler =
          render_frame->GetSuddenTerminationDisablerState(
              blink::kBeforeUnloadHandler);
      stats_list->push_back(stats);
    }
  }
}

// This function is called when |update_timer_| fires. It will adjust the clock
// if needed (if it detects that the machine was asleep) and will fire the stats
// updating on ChromeOS via the delegate. This function also tries to purge
// cache memory.
void TabManager::UpdateTimerCallback() {
  // If Chrome is shutting down, do not do anything.
  if (g_browser_process->IsShuttingDown())
    return;

  if (BrowserList::GetInstance()->empty())
    return;

  last_adjust_time_ = NowTicks();

#if defined(OS_CHROMEOS)
  TabStatsList stats_list = GetTabStats();
  // This starts the CrOS specific OOM adjustments in /proc/<pid>/oom_score_adj.
  delegate_->AdjustOomPriorities(stats_list);
#endif

  PurgeBackgroundedTabsIfNeeded();
}

base::TimeDelta TabManager::GetTimeToPurge(
    base::TimeDelta min_time_to_purge,
    base::TimeDelta max_time_to_purge) const {
  return base::TimeDelta::FromSeconds(base::RandInt(
      min_time_to_purge.InSeconds(), max_time_to_purge.InSeconds()));
}

bool TabManager::ShouldPurgeNow(content::WebContents* content) const {
  if (GetWebContentsData(content)->is_purged())
    return false;

  base::TimeDelta time_passed =
      NowTicks() - GetWebContentsData(content)->LastInactiveTime();
  return time_passed > GetWebContentsData(content)->time_to_purge();
}

void TabManager::PurgeBackgroundedTabsIfNeeded() {
  auto tab_stats = GetUnsortedTabStats();
  for (auto& tab : tab_stats) {
    if (!tab.render_process_host->IsProcessBackgrounded())
      continue;
    if (!CanSuspendBackgroundedRenderer(tab.child_process_host_id))
      continue;

    WebContents* content = GetWebContentsById(tab.tab_contents_id);
    if (!content)
      continue;

    bool purge_now = ShouldPurgeNow(content);
    if (!purge_now)
      continue;

    // Since |content|'s tab is kept inactive and background for more than
    // time-to-purge time, its purged state changes: false => true.
    GetWebContentsData(content)->set_is_purged(true);
    // TODO(tasak): rename PurgeAndSuspend with a better name, e.g.
    // RequestPurgeCache, because we don't suspend any renderers.
    tab.render_process_host->PurgeAndSuspend();
  }
}

WebContents* TabManager::DiscardWebContentsAt(int index,
                                              TabStripModel* model,
                                              DiscardTabCondition condition) {
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
  //
  // Set |needs_reload| to false so that the tab is not automatically reloaded
  // when activated (otherwise, there would be an immediate reload when the
  // active tab in a non-visible window is discarded). TabManager will
  // explicitly reload the tab when it becomes the active tab in an active
  // window (ReloadWebContentsIfDiscarded).
  //
  // Note: It is important that |needs_reload| is false even when the discarded
  // tab is not active. Otherwise, it would get reloaded by
  // WebContentsImpl::WasShown() and by ReloadWebContentsIfDiscarded() when
  // activated.
  null_contents->GetController().CopyStateFrom(old_contents->GetController(),
                                               /* needs_reload */ false);

  // Make sure to persist the last active time property.
  null_contents->SetLastActiveTime(old_contents->GetLastActiveTime());
  // Copy over the discard count.
  WebContentsData::CopyState(old_contents, null_contents);

  // First try to fast-kill the process, if it's just running a single tab.
  bool fast_shutdown_success =
      old_contents->GetMainFrame()->GetProcess()->FastShutdownIfPossible(1u,
                                                                         false);

#ifdef OS_CHROMEOS
  if (!fast_shutdown_success && condition == kUrgentShutdown) {
    content::RenderFrameHost* main_frame = old_contents->GetMainFrame();
    // We avoid fast shutdown on tabs with beforeunload handlers on the main
    // frame, as that is often an indication of unsaved user state.
    DCHECK(main_frame);
    if (!main_frame->GetSuddenTerminationDisablerState(
            blink::kBeforeUnloadHandler)) {
      fast_shutdown_success =
          old_contents->GetMainFrame()->GetProcess()->FastShutdownIfPossible(
              1u, /* skip_unload_handlers */ true);
    }
    UMA_HISTOGRAM_BOOLEAN(
        "TabManager.Discarding.DiscardedTabCouldUnsafeFastShutdown",
        fast_shutdown_success);
  }
#endif
  UMA_HISTOGRAM_BOOLEAN("TabManager.Discarding.DiscardedTabCouldFastShutdown",
                        fast_shutdown_success);

  // Replace the discarded tab with the null version.
  model->ReplaceWebContentsAt(index, null_contents);
  // Mark the tab so it will reload when clicked on.
  GetWebContentsData(null_contents)->SetDiscardState(true);
  GetWebContentsData(null_contents)->IncrementDiscardCount();

  // Make the tab PURGED to avoid purging null_contents.
  GetWebContentsData(null_contents)->set_is_purged(true);

  // Discard the old tab's renderer.
  // TODO(jamescook): This breaks script connections with other tabs.
  // Find a different approach that doesn't do that, perhaps based on
  // RenderFrameProxyHosts.
  delete old_contents;
  recent_tab_discard_ = true;

  return null_contents;
}

void TabManager::PauseBackgroundTabOpeningIfNeeded() {
  TRACE_EVENT_INSTANT0("navigation",
                       "TabManager::PauseBackgroundTabOpeningIfNeeded",
                       TRACE_EVENT_SCOPE_THREAD);
  if (IsInBackgroundTabOpeningSession()) {
    stats_collector_->TrackPausedBackgroundTabs(pending_navigations_.size());
    stats_collector_->OnBackgroundTabOpeningSessionEnded();
  }

  background_tab_loading_mode_ = BackgroundTabLoadingMode::kPaused;
}

void TabManager::ResumeBackgroundTabOpeningIfNeeded() {
  TRACE_EVENT_INSTANT0("navigation",
                       "TabManager::ResumeBackgroundTabOpeningIfNeeded",
                       TRACE_EVENT_SCOPE_THREAD);
  background_tab_loading_mode_ = BackgroundTabLoadingMode::kStaggered;
  LoadNextBackgroundTabIfNeeded();

  if (IsInBackgroundTabOpeningSession())
    stats_collector_->OnBackgroundTabOpeningSessionStarted();
}

void TabManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // If Chrome is shutting down, do not do anything.
  if (g_browser_process->IsShuttingDown())
    return;

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      ResumeBackgroundTabOpeningIfNeeded();
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      PauseBackgroundTabOpeningIfNeeded();
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      PauseBackgroundTabOpeningIfNeeded();
      LogMemoryAndDiscardTab(kUrgentShutdown);
      break;
    default:
      NOTREACHED();
  }
  // TODO(skuhne): If more memory pressure levels are introduced, consider
  // calling PurgeBrowserMemory() before CRITICAL is reached.
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
  // An active tab is not purged.
  // Calling GetWebContentsData() early ensures that the WebContentsData is
  // created for |new_contents|, which |stats_collector_| expects.
  GetWebContentsData(new_contents)->set_is_purged(false);

  // If |old_contents| is set, that tab has switched from being active to
  // inactive, so record the time of that transition.
  if (old_contents) {
    GetWebContentsData(old_contents)->SetLastInactiveTime(NowTicks());
    // Re-setting time-to-purge every time a tab becomes inactive.
    GetWebContentsData(old_contents)
        ->set_time_to_purge(
            GetTimeToPurge(min_time_to_purge_, max_time_to_purge_));
    // Only record switch-to-tab metrics when a switch happens, i.e.
    // |old_contents| is set.
    stats_collector_->RecordSwitchToTab(old_contents, new_contents);
  }

  // Reload |web_contents| if it is in an active browser and discarded.
  if (IsActiveWebContentsInActiveBrowser(new_contents)) {
    ReloadWebContentsIfDiscarded(new_contents,
                                 GetWebContentsData(new_contents));
  }

  ResumeTabNavigationIfNeeded(new_contents);
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
  // Re-setting time-to-purge every time a tab becomes inactive.
  GetWebContentsData(contents)->set_time_to_purge(
      GetTimeToPurge(min_time_to_purge_, max_time_to_purge_));
}

void TabManager::OnBrowserSetLastActive(Browser* browser) {
  // Reload the active tab in |browser| if it is discarded.
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (contents)
    ReloadWebContentsIfDiscarded(contents, GetWebContentsData(contents));
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

// TODO(jamescook): This should consider tabs with references to other tabs,
// such as tabs created with JavaScript window.open(). Potentially consider
// discarding the entire set together, or use that in the priority computation.
content::WebContents* TabManager::DiscardTabImpl(
    DiscardTabCondition condition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TabStatsList stats = GetTabStats();

  if (stats.empty())
    return nullptr;
  // Loop until a non-discarded tab to kill is found.
  for (TabStatsList::const_reverse_iterator stats_rit = stats.rbegin();
       stats_rit != stats.rend(); ++stats_rit) {
    if (CanDiscardTab(*stats_rit)) {
      WebContents* new_contents =
          DiscardTabById(stats_rit->tab_contents_id, condition);
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

bool TabManager::IsActiveWebContentsInActiveBrowser(
    content::WebContents* contents) const {
  auto browser_info_list = GetBrowserInfoList();
  if (browser_info_list.empty())
    return false;
  return browser_info_list.front().tab_strip_model->GetActiveWebContents() ==
         contents;
}

std::vector<BrowserInfo> TabManager::GetBrowserInfoList() const {
  if (!test_browser_info_list_.empty())
    return test_browser_info_list_;

  std::vector<BrowserInfo> browser_info_list;

  BrowserList* browser_list = BrowserList::GetInstance();
  for (auto browser_iterator = browser_list->begin_last_active();
       browser_iterator != browser_list->end_last_active();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;

    BrowserInfo browser_info;
    browser_info.browser = browser;
    browser_info.tab_strip_model = browser->tab_strip_model();
    browser_info.window_is_minimized = browser->window()->IsMinimized();
    browser_info.browser_is_app = browser->is_app();
    browser_info_list.push_back(browser_info);
  }

  return browser_info_list;
}

content::NavigationThrottle::ThrottleCheckResult
TabManager::MaybeThrottleNavigation(BackgroundTabNavigationThrottle* throttle) {
  content::WebContents* contents =
      throttle->navigation_handle()->GetWebContents();
  DCHECK(!contents->IsVisible());

  // Skip delaying the navigation if this tab is in session restore, whose
  // loading is already controlled by TabLoader.
  if (GetWebContentsData(contents)->is_in_session_restore())
    return content::NavigationThrottle::PROCEED;

  if (background_tab_loading_mode_ == BackgroundTabLoadingMode::kStaggered &&
      !IsInBackgroundTabOpeningSession()) {
    stats_collector_->OnBackgroundTabOpeningSessionStarted();
  }

  stats_collector_->TrackNewBackgroundTab(pending_navigations_.size(),
                                          loading_contents_.size());

  if (!base::FeatureList::IsEnabled(
          features::kStaggeredBackgroundTabOpeningExperiment) ||
      CanLoadNextTab()) {
    loading_contents_.insert(contents);
    stats_collector_->TrackBackgroundTabLoadAutoStarted();
    return content::NavigationThrottle::PROCEED;
  }

  // TODO(zhenw): Try to set the favicon and title from history service if this
  // navigation will be delayed.
  GetWebContentsData(contents)->SetTabLoadingState(TAB_IS_NOT_LOADING);
  pending_navigations_.push_back(throttle);
  std::stable_sort(pending_navigations_.begin(), pending_navigations_.end(),
                   ComparePendingNavigations);

  TRACE_EVENT_INSTANT1(
      "navigation", "TabManager::MaybeThrottleNavigation",
      TRACE_EVENT_SCOPE_THREAD, "data",
      DataAsTraceValue(background_tab_loading_mode_,
                       pending_navigations_.size(), loading_contents_.size()));

  StartForceLoadTimer();
  return content::NavigationThrottle::DEFER;
}

bool TabManager::IsInBackgroundTabOpeningSession() const {
  if (background_tab_loading_mode_ != BackgroundTabLoadingMode::kStaggered)
    return false;

  return !(pending_navigations_.empty() && loading_contents_.empty());
}

bool TabManager::CanLoadNextTab() const {
  if (background_tab_loading_mode_ != BackgroundTabLoadingMode::kStaggered)
    return false;

  // TabManager can only load the next tab when the loading slots free up. The
  // loading slot limit can be exceeded when |force_load_timer_| fires or when
  // the user selects a background tab.
  if (loading_contents_.size() < loading_slots_)
    return true;

  return false;
}

void TabManager::OnWillRestoreTab(WebContents* web_contents) {
  WebContentsData* data = GetWebContentsData(web_contents);
  DCHECK(!data->is_in_session_restore());
  data->SetIsInSessionRestore(true);
  data->SetIsRestoredInForeground(web_contents->IsVisible());
}

void TabManager::OnDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  auto it = pending_navigations_.begin();
  while (it != pending_navigations_.end()) {
    BackgroundTabNavigationThrottle* throttle = *it;
    if (throttle->navigation_handle() == navigation_handle) {
      TRACE_EVENT_INSTANT1("navigation", "TabManager::OnDidFinishNavigation",
                           TRACE_EVENT_SCOPE_THREAD,
                           "found_navigation_handle_to_remove", true);
      pending_navigations_.erase(it);
      break;
    }
    it++;
  }
}

void TabManager::OnDidStopLoading(content::WebContents* contents) {
  DCHECK_EQ(TAB_IS_LOADED, GetWebContentsData(contents)->tab_loading_state());
  bool was_in_background_tab_opening_session =
      IsInBackgroundTabOpeningSession();

  loading_contents_.erase(contents);
  stats_collector_->OnDidStopLoading(contents);
  LoadNextBackgroundTabIfNeeded();

  if (was_in_background_tab_opening_session &&
      !IsInBackgroundTabOpeningSession()) {
    stats_collector_->OnBackgroundTabOpeningSessionEnded();
  }
}

void TabManager::OnWebContentsDestroyed(content::WebContents* contents) {
  bool was_in_background_tab_opening_session =
      IsInBackgroundTabOpeningSession();

  RemovePendingNavigationIfNeeded(contents);
  loading_contents_.erase(contents);
  stats_collector_->OnWebContentsDestroyed(contents);
  LoadNextBackgroundTabIfNeeded();

  if (was_in_background_tab_opening_session &&
      !IsInBackgroundTabOpeningSession()) {
    stats_collector_->OnBackgroundTabOpeningSessionEnded();
  }
}

void TabManager::StartForceLoadTimer() {
  TRACE_EVENT_INSTANT1(
      "navigation", "TabManager::StartForceLoadTimer", TRACE_EVENT_SCOPE_THREAD,
      "data",
      DataAsTraceValue(background_tab_loading_mode_,
                       pending_navigations_.size(), loading_contents_.size()));

  force_load_timer_->Stop();
  force_load_timer_->Start(FROM_HERE, kBackgroundTabLoadTimeout, this,
                           &TabManager::LoadNextBackgroundTabIfNeeded);
}

void TabManager::LoadNextBackgroundTabIfNeeded() {
  TRACE_EVENT_INSTANT2(
      "navigation", "TabManager::LoadNextBackgroundTabIfNeeded",
      TRACE_EVENT_SCOPE_THREAD, "is_force_load_timer_running",
      force_load_timer_->IsRunning(), "data",
      DataAsTraceValue(background_tab_loading_mode_,
                       pending_navigations_.size(), loading_contents_.size()));

  if (background_tab_loading_mode_ != BackgroundTabLoadingMode::kStaggered)
    return;

  // Do not load more background tabs until TabManager can load the next tab.
  // Ignore this constraint if the timer fires to force loading the next
  // background tab.
  if (force_load_timer_->IsRunning() && !CanLoadNextTab())
    return;

  if (pending_navigations_.empty())
    return;

  BackgroundTabNavigationThrottle* throttle = pending_navigations_.front();
  pending_navigations_.erase(pending_navigations_.begin());
  ResumeNavigation(throttle);
  stats_collector_->TrackBackgroundTabLoadAutoStarted();

  StartForceLoadTimer();
}

void TabManager::ResumeTabNavigationIfNeeded(content::WebContents* contents) {
  BackgroundTabNavigationThrottle* throttle =
      RemovePendingNavigationIfNeeded(contents);
  if (throttle) {
    ResumeNavigation(throttle);
    stats_collector_->TrackBackgroundTabLoadUserInitiated();
  }
}

void TabManager::ResumeNavigation(BackgroundTabNavigationThrottle* throttle) {
  content::NavigationHandle* navigation_handle = throttle->navigation_handle();
  GetWebContentsData(navigation_handle->GetWebContents())
      ->SetTabLoadingState(TAB_IS_LOADING);
  loading_contents_.insert(navigation_handle->GetWebContents());

  throttle->ResumeNavigation();
}

BackgroundTabNavigationThrottle* TabManager::RemovePendingNavigationIfNeeded(
    content::WebContents* contents) {
  auto it = pending_navigations_.begin();
  while (it != pending_navigations_.end()) {
    BackgroundTabNavigationThrottle* throttle = *it;
    if (throttle->navigation_handle()->GetWebContents() == contents) {
      pending_navigations_.erase(it);
      return throttle;
    }
    it++;
  }
  return nullptr;
}

// static
bool TabManager::ComparePendingNavigations(
    const BackgroundTabNavigationThrottle* first,
    const BackgroundTabNavigationThrottle* second) {
  bool first_is_internal_page =
      IsInternalPage(first->navigation_handle()->GetURL());
  bool second_is_internal_page =
      IsInternalPage(second->navigation_handle()->GetURL());

  if (first_is_internal_page != second_is_internal_page)
    return !first_is_internal_page;

  return false;
}

bool TabManager::IsTabLoadingForTest(content::WebContents* contents) const {
  if (loading_contents_.count(contents) == 1) {
    DCHECK_EQ(TAB_IS_LOADING,
              GetWebContentsData(contents)->tab_loading_state());
    return true;
  }

  DCHECK_NE(TAB_IS_LOADING, GetWebContentsData(contents)->tab_loading_state());
  return false;
}

bool TabManager::IsNavigationDelayedForTest(
    const content::NavigationHandle* navigation_handle) const {
  for (const auto* it : pending_navigations_) {
    if (it->navigation_handle() == navigation_handle)
      return true;
  }
  return false;
}

bool TabManager::TriggerForceLoadTimerForTest() {
  if (!force_load_timer_->IsRunning() ||
      (test_tick_clock_->NowTicks() < force_load_timer_->desired_run_time())) {
    return false;
  }

  base::Closure timer_callback(force_load_timer_->user_task());
  force_load_timer_->Stop();
  timer_callback.Run();
  return true;
}

}  // namespace resource_coordinator
