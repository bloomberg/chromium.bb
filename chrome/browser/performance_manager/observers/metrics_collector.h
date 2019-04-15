// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_METRICS_COLLECTOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_METRICS_COLLECTOR_H_

#include <map>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/observers/background_metrics_reporter.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager {

class FrameNodeImpl;
class NodeBase;
class PageNodeImpl;

extern const char kTabFromBackgroundedToFirstFaviconUpdatedUMA[];
extern const char kTabFromBackgroundedToFirstTitleUpdatedUMA[];
extern const char
    kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA[];
extern const base::TimeDelta kMetricsReportDelayTimeout;
extern const int kDefaultFrequencyUkmEQTReported;

// A MetricsCollector observes changes happened inside CoordinationUnit Graph,
// and reports UMA/UKM.
class MetricsCollector : public GraphObserver {
 public:
  MetricsCollector();
  ~MetricsCollector() override;

  // GraphObserver implementation.
  bool ShouldObserve(const NodeBase* node) override;
  void OnNodeAdded(NodeBase* node) override;
  void OnBeforeNodeRemoved(NodeBase* node) override;
  void OnNonPersistentNotificationCreated(FrameNodeImpl* frame_node) override;
  void OnIsVisibleChanged(PageNodeImpl* page_node) override;
  void OnUkmSourceIdChanged(PageNodeImpl* page_node) override;
  void OnFaviconUpdated(PageNodeImpl* page_node) override;
  void OnTitleUpdated(PageNodeImpl* page_node) override;
  void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) override;

 private:
  struct MetricsReportRecord {
    MetricsReportRecord();
    MetricsReportRecord(const MetricsReportRecord& other);
    void UpdateUkmSourceID(ukm::SourceId ukm_source_id);
    void Reset();
    BackgroundMetricsReporter<
        ukm::builders::TabManager_Background_FirstFaviconUpdated,
        kTabFromBackgroundedToFirstFaviconUpdatedUMA,
        internal::UKMFrameReportType::kMainFrameOnly>
        first_favicon_updated;
    BackgroundMetricsReporter<
        ukm::builders::
            TabManager_Background_FirstNonPersistentNotificationCreated,
        kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA,
        internal::UKMFrameReportType::kMainFrameAndChildFrame>
        first_non_persistent_notification_created;
    BackgroundMetricsReporter<
        ukm::builders::TabManager_Background_FirstTitleUpdated,
        kTabFromBackgroundedToFirstTitleUpdatedUMA,
        internal::UKMFrameReportType::kMainFrameOnly>
        first_title_updated;
  };

  struct UkmCollectionState {
    int num_unreported_eqt_measurements = 0u;
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  };

  bool ShouldReportMetrics(const PageNodeImpl* page_node);
  bool IsCollectingExpectedQueueingTimeForUkm(
      const resource_coordinator::CoordinationUnitID& page_node_id);
  void RecordExpectedQueueingTimeForUkm(
      const resource_coordinator::CoordinationUnitID& page_node_id,
      const base::TimeDelta& expected_queueing_time);
  void UpdateUkmSourceIdForPage(
      const resource_coordinator::CoordinationUnitID& page_node_id,
      ukm::SourceId ukm_source_id);
  void UpdateWithFieldTrialParams();
  void ResetMetricsReportRecord(resource_coordinator::CoordinationUnitID cu_id);

  // The metrics_report_record_map_ is used to record whether a metric was
  // already reported to avoid reporting multiple metrics.
  std::map<resource_coordinator::CoordinationUnitID, MetricsReportRecord>
      metrics_report_record_map_;
  std::map<resource_coordinator::CoordinationUnitID, UkmCollectionState>
      ukm_collection_state_map_;
  // The number of reports to wait before reporting ExpectedQueueingTime. For
  // example, if |frequency_ukm_eqt_reported_| is 2, then the first value is not
  // reported, the second one is, the third one isn't, etc.
  int frequency_ukm_eqt_reported_;
  DISALLOW_COPY_AND_ASSIGN(MetricsCollector);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_METRICS_COLLECTOR_H_
