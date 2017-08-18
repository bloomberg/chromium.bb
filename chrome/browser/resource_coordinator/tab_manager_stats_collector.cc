// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/sessions/session_restore.h"
#include "content/public/browser/swap_metrics_driver.h"

namespace resource_coordinator {

class TabManagerStatsCollector::SessionRestoreSwapMetricsDelegate
    : public content::SwapMetricsDriver::Delegate {
 public:
  explicit SessionRestoreSwapMetricsDelegate(
      TabManagerStatsCollector* tab_manager_stats_collector)
      : tab_manager_stats_collector_(tab_manager_stats_collector) {}

  ~SessionRestoreSwapMetricsDelegate() override = default;

  void OnSwapInCount(uint64_t count, base::TimeDelta interval) override {
    tab_manager_stats_collector_->OnSessionRestoreSwapInCount(count, interval);
  }

  void OnSwapOutCount(uint64_t count, base::TimeDelta interval) override {
    tab_manager_stats_collector_->OnSessionRestoreSwapOutCount(count, interval);
  }

  void OnDecompressedPageCount(uint64_t count,
                               base::TimeDelta interval) override {
    tab_manager_stats_collector_->OnSessionRestoreDecompressedPageCount(
        count, interval);
  }

  void OnCompressedPageCount(uint64_t count,
                             base::TimeDelta interval) override {
    tab_manager_stats_collector_->OnSessionRestoreCompressedPageCount(count,
                                                                      interval);
  }

  void OnSessionRestoreUpdateMetricsFailed() {
    tab_manager_stats_collector_->OnSessionRestoreUpdateMetricsFailed();
  }

 private:
  TabManagerStatsCollector* tab_manager_stats_collector_;
};

TabManagerStatsCollector::TabManagerStatsCollector(TabManager* tab_manager)
    : tab_manager_(tab_manager), is_session_restore_loading_tabs_(false) {
  std::unique_ptr<content::SwapMetricsDriver::Delegate> delegate(
      base::WrapUnique<content::SwapMetricsDriver::Delegate>(
          new SessionRestoreSwapMetricsDelegate(this)));
  session_restore_swap_metrics_driver_ = content::SwapMetricsDriver::Create(
      std::move(delegate), base::TimeDelta::FromSeconds(0));
  SessionRestore::AddObserver(this);
}

TabManagerStatsCollector::~TabManagerStatsCollector() {
  SessionRestore::RemoveObserver(this);
}

void TabManagerStatsCollector::RecordSwitchToTab(
    content::WebContents* contents) const {
  if (tab_manager_->IsSessionRestoreLoadingTabs()) {
    auto* data = TabManager::WebContentsData::FromWebContents(contents);
    DCHECK(data);
    UMA_HISTOGRAM_ENUMERATION("TabManager.SessionRestore.SwitchToTab",
                              data->tab_loading_state(), TAB_LOADING_STATE_MAX);
  }
}

void TabManagerStatsCollector::RecordExpectedTaskQueueingDuration(
    content::WebContents* contents,
    base::TimeDelta queueing_time) {}

void TabManagerStatsCollector::OnSessionRestoreStartedLoadingTabs() {
  DCHECK(!is_session_restore_loading_tabs_);
  if (session_restore_swap_metrics_driver_)
    session_restore_swap_metrics_driver_->InitializeMetrics();
  is_session_restore_loading_tabs_ = true;
}

void TabManagerStatsCollector::OnSessionRestoreFinishedLoadingTabs() {
  DCHECK(is_session_restore_loading_tabs_);
  if (session_restore_swap_metrics_driver_)
    session_restore_swap_metrics_driver_->UpdateMetrics();
  is_session_restore_loading_tabs_ = false;
}

void TabManagerStatsCollector::OnSessionRestoreSwapInCount(
    uint64_t count,
    base::TimeDelta interval) {
  DCHECK(is_session_restore_loading_tabs_);
  UMA_HISTOGRAM_COUNTS_10000(
      "TabManager.SessionRestore.SwapInPerSecond",
      static_cast<double>(count) / interval.InSecondsF());
}

void TabManagerStatsCollector::OnSessionRestoreSwapOutCount(
    uint64_t count,
    base::TimeDelta interval) {
  DCHECK(is_session_restore_loading_tabs_);
  UMA_HISTOGRAM_COUNTS_10000(
      "TabManager.SessionRestore.SwapOutPerSecond",
      static_cast<double>(count) / interval.InSecondsF());
}

void TabManagerStatsCollector::OnSessionRestoreDecompressedPageCount(
    uint64_t count,
    base::TimeDelta interval) {
  DCHECK(is_session_restore_loading_tabs_);
  UMA_HISTOGRAM_COUNTS_10000(
      "TabManager.SessionRestore.DecompressedPagesPerSecond",
      static_cast<double>(count) / interval.InSecondsF());
}

void TabManagerStatsCollector::OnSessionRestoreCompressedPageCount(
    uint64_t count,
    base::TimeDelta interval) {
  DCHECK(is_session_restore_loading_tabs_);
  UMA_HISTOGRAM_COUNTS_10000(
      "TabManager.SessionRestore.CompressedPagesPerSecond",
      static_cast<double>(count) / interval.InSecondsF());
}

void TabManagerStatsCollector::OnSessionRestoreUpdateMetricsFailed() {
  // If either InitializeMetrics() or UpdateMetrics() fails, it's unlikely an
  // error that can be recovered from, in which case we don't collect swap
  // metrics for session restore.
  session_restore_swap_metrics_driver_.reset();
}

}  // namespace resource_coordinator
