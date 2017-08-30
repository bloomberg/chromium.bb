// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/sessions/session_restore.h"
#include "content/public/browser/swap_metrics_driver.h"

namespace resource_coordinator {

namespace {

const char* kSessionTypeName[] = {"SessionRestore", "BackgroundTabOpening"};

}  // namespace

class TabManagerStatsCollector::SwapMetricsDelegate
    : public content::SwapMetricsDriver::Delegate {
 public:
  explicit SwapMetricsDelegate(
      TabManagerStatsCollector* tab_manager_stats_collector,
      SessionType type)
      : tab_manager_stats_collector_(tab_manager_stats_collector),
        session_type_(type) {}

  ~SwapMetricsDelegate() override = default;

  void OnSwapInCount(uint64_t count, base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        session_type_, "SwapInPerSecond", count, interval);
  }

  void OnSwapOutCount(uint64_t count, base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        session_type_, "SwapOutPerSecond", count, interval);
  }

  void OnDecompressedPageCount(uint64_t count,
                               base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        session_type_, "DecompressedPagesPerSecond", count, interval);
  }

  void OnCompressedPageCount(uint64_t count,
                             base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        session_type_, "CompressedPagesPerSecond", count, interval);
  }

  void OnUpdateMetricsFailed() override {
    tab_manager_stats_collector_->OnUpdateSwapMetricsFailed();
  }

 private:
  TabManagerStatsCollector* tab_manager_stats_collector_;
  const SessionType session_type_;
};

TabManagerStatsCollector::TabManagerStatsCollector()
    : is_session_restore_loading_tabs_(false),
      is_in_background_tab_opening_session_(false) {
  SessionRestore::AddObserver(this);
}

TabManagerStatsCollector::~TabManagerStatsCollector() {
  SessionRestore::RemoveObserver(this);
}

void TabManagerStatsCollector::RecordSwitchToTab(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (!is_session_restore_loading_tabs_ &&
      !is_in_background_tab_opening_session_) {
    return;
  }

  if (IsInOverlappedSession())
    return;

  auto* new_data = TabManager::WebContentsData::FromWebContents(new_contents);
  DCHECK(new_data);

  if (is_session_restore_loading_tabs_) {
    UMA_HISTOGRAM_ENUMERATION(kHistogramSessionRestoreSwitchToTab,
                              new_data->tab_loading_state(),
                              TAB_LOADING_STATE_MAX);
  }
  if (is_in_background_tab_opening_session_) {
    UMA_HISTOGRAM_ENUMERATION(kHistogramBackgroundTabOpeningSwitchToTab,
                              new_data->tab_loading_state(),
                              TAB_LOADING_STATE_MAX);
  }

  if (old_contents)
    foreground_contents_switched_to_times_.erase(old_contents);
  DCHECK(
      !base::ContainsKey(foreground_contents_switched_to_times_, new_contents));
  if (new_data->tab_loading_state() != TAB_IS_LOADED) {
    foreground_contents_switched_to_times_.insert(
        std::make_pair(new_contents, base::TimeTicks::Now()));
  }
}

void TabManagerStatsCollector::RecordExpectedTaskQueueingDuration(
    content::WebContents* contents,
    base::TimeDelta queueing_time) {
  if (!contents->IsVisible())
    return;

  if (IsInOverlappedSession())
    return;

  if (is_session_restore_loading_tabs_) {
    UMA_HISTOGRAM_TIMES(
        kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
        queueing_time);
  }

  if (is_in_background_tab_opening_session_) {
    UMA_HISTOGRAM_TIMES(
        kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
        queueing_time);
  }
}

void TabManagerStatsCollector::OnSessionRestoreStartedLoadingTabs() {
  DCHECK(!is_session_restore_loading_tabs_);

  CreateAndInitSwapMetricsDriverIfNeeded(SessionType::kSessionRestore);

  is_session_restore_loading_tabs_ = true;
  ClearStatsWhenInOverlappedSession();
}

void TabManagerStatsCollector::OnSessionRestoreFinishedLoadingTabs() {
  DCHECK(is_session_restore_loading_tabs_);
  if (swap_metrics_driver_)
    swap_metrics_driver_->UpdateMetrics();
  is_session_restore_loading_tabs_ = false;
}

void TabManagerStatsCollector::OnBackgroundTabOpeningSessionStarted() {
  DCHECK(!is_in_background_tab_opening_session_);

  CreateAndInitSwapMetricsDriverIfNeeded(SessionType::kBackgroundTabOpening);

  is_in_background_tab_opening_session_ = true;
  ClearStatsWhenInOverlappedSession();
}

void TabManagerStatsCollector::OnBackgroundTabOpeningSessionEnded() {
  DCHECK(is_in_background_tab_opening_session_);
  if (swap_metrics_driver_)
    swap_metrics_driver_->UpdateMetrics();
  is_in_background_tab_opening_session_ = false;
}

void TabManagerStatsCollector::CreateAndInitSwapMetricsDriverIfNeeded(
    SessionType type) {
  if (IsInOverlappedSession()) {
    swap_metrics_driver_ = nullptr;
    return;
  }

  // Always create a new instance in case there is a SessionType change because
  // this is shared between SessionRestore and BackgroundTabOpening.
  swap_metrics_driver_ = content::SwapMetricsDriver::Create(
      base::WrapUnique<content::SwapMetricsDriver::Delegate>(
          new SwapMetricsDelegate(this, type)),
      base::TimeDelta::FromSeconds(0));
  // The driver could still be null on a platform with no swap driver support.
  if (swap_metrics_driver_)
    swap_metrics_driver_->InitializeMetrics();
}

void TabManagerStatsCollector::RecordSwapMetrics(
    SessionType type,
    const std::string& metric_name,
    uint64_t count,
    const base::TimeDelta& interval) {
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      "TabManager.Experimental." + std::string(kSessionTypeName[type]) + "." +
          metric_name,
      1,      // minimum
      10000,  // maximum
      50,     // bucket_count
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<double>(count) / interval.InSecondsF());
}

void TabManagerStatsCollector::OnUpdateSwapMetricsFailed() {
  swap_metrics_driver_ = nullptr;
}

void TabManagerStatsCollector::OnDidStartMainFrameNavigation(
    content::WebContents* contents) {
  foreground_contents_switched_to_times_.erase(contents);
}

void TabManagerStatsCollector::OnDidStopLoading(
    content::WebContents* contents) {
  if (!base::ContainsKey(foreground_contents_switched_to_times_, contents))
    return;

  if (is_session_restore_loading_tabs_ && !IsInOverlappedSession()) {
    UMA_HISTOGRAM_TIMES(kHistogramSessionRestoreTabSwitchLoadTime,
                        base::TimeTicks::Now() -
                            foreground_contents_switched_to_times_[contents]);
  }
  if (is_in_background_tab_opening_session_ && !IsInOverlappedSession()) {
    UMA_HISTOGRAM_TIMES(kHistogramBackgroundTabOpeningTabSwitchLoadTime,
                        base::TimeTicks::Now() -
                            foreground_contents_switched_to_times_[contents]);
  }

  foreground_contents_switched_to_times_.erase(contents);
}

void TabManagerStatsCollector::OnWebContentsDestroyed(
    content::WebContents* contents) {
  foreground_contents_switched_to_times_.erase(contents);
}

bool TabManagerStatsCollector::IsInOverlappedSession() {
  return is_session_restore_loading_tabs_ &&
         is_in_background_tab_opening_session_;
}

void TabManagerStatsCollector::ClearStatsWhenInOverlappedSession() {
  if (!IsInOverlappedSession())
    return;

  swap_metrics_driver_ = nullptr;
  foreground_contents_switched_to_times_.clear();
}

// static
const char TabManagerStatsCollector::
    kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration[] =
        "TabManager.SessionRestore.ForegroundTab.ExpectedTaskQueueingDuration";

// static
const char TabManagerStatsCollector::
    kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration[] =
        "TabManager.BackgroundTabOpening.ForegroundTab."
        "ExpectedTaskQueueingDuration";

// Static
const char TabManagerStatsCollector::kHistogramSessionRestoreSwitchToTab[] =
    "TabManager.SessionRestore.SwitchToTab";

// Static
const char
    TabManagerStatsCollector::kHistogramBackgroundTabOpeningSwitchToTab[] =
        "TabManager.BackgroundTabOpening.SwitchToTab";

// Static
const char
    TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime[] =
        "TabManager.Experimental.SessionRestore.TabSwitchLoadTime";

// Static
const char TabManagerStatsCollector::
    kHistogramBackgroundTabOpeningTabSwitchLoadTime[] =
        "TabManager.Experimental.BackgroundTabOpening.TabSwitchLoadTime";

}  // namespace resource_coordinator
