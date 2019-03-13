// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/metrics_collector.h"

#include <set>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace performance_manager {

// Delay the metrics report from GRC to UMA/UKM for 5 minutes from when the main
// frame navigation is committed.
const base::TimeDelta kMetricsReportDelayTimeout =
    base::TimeDelta::FromMinutes(5);

const char kTabFromBackgroundedToFirstFaviconUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstFaviconUpdated";
const char kTabFromBackgroundedToFirstTitleUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstTitleUpdated";
const char kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA[] =
    "TabManager.Heuristics."
    "FromBackgroundedToFirstNonPersistentNotificationCreated";

const int kDefaultFrequencyUkmEQTReported = 5u;

// Gets the number of tabs that are co-resident in all of the render processes
// associated with a |resource_coordinator::CoordinationUnitType::kPage|
// coordination unit.
size_t GetNumCoresidentTabs(const PageNodeImpl* page_cu) {
  std::set<NodeBase*> coresident_tabs;
  for (auto* process_cu : page_cu->GetAssociatedProcessCoordinationUnits()) {
    for (auto* associated_page_cu :
         process_cu->GetAssociatedPageCoordinationUnits()) {
      coresident_tabs.insert(associated_page_cu);
    }
  }
  // A tab cannot be co-resident with itself.
  return coresident_tabs.size() - 1;
}

MetricsCollector::MetricsCollector() {
  UpdateWithFieldTrialParams();
}

MetricsCollector::~MetricsCollector() = default;

bool MetricsCollector::ShouldObserve(const NodeBase* coordination_unit) {
  return coordination_unit->id().type ==
             resource_coordinator::CoordinationUnitType::kFrame ||
         coordination_unit->id().type ==
             resource_coordinator::CoordinationUnitType::kPage ||
         coordination_unit->id().type ==
             resource_coordinator::CoordinationUnitType::kProcess;
}

void MetricsCollector::OnNodeCreated(NodeBase* coordination_unit) {
  if (coordination_unit->id().type ==
      resource_coordinator::CoordinationUnitType::kPage) {
    metrics_report_record_map_.emplace(coordination_unit->id(),
                                       MetricsReportRecord());
  }
}

void MetricsCollector::OnBeforeNodeDestroyed(NodeBase* coordination_unit) {
  if (coordination_unit->id().type ==
      resource_coordinator::CoordinationUnitType::kPage) {
    metrics_report_record_map_.erase(coordination_unit->id());
    ukm_collection_state_map_.erase(coordination_unit->id());
  }
}

void MetricsCollector::OnPagePropertyChanged(
    PageNodeImpl* page_cu,
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  if (property_type ==
      resource_coordinator::mojom::PropertyType::kUKMSourceId) {
    auto page_cu_id = page_cu->id();
    ukm::SourceId ukm_source_id = value;
    UpdateUkmSourceIdForPage(page_cu_id, ukm_source_id);
    MetricsReportRecord& record =
        metrics_report_record_map_.find(page_cu_id)->second;
    record.UpdateUKMSourceID(ukm_source_id);
  }
}

void MetricsCollector::OnProcessPropertyChanged(
    ProcessNodeImpl* process_cu,
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == resource_coordinator::mojom::PropertyType::
                           kExpectedTaskQueueingDuration) {
    for (auto* page_cu : process_cu->GetAssociatedPageCoordinationUnits()) {
      if (IsCollectingExpectedQueueingTimeForUkm(page_cu->id())) {
        int64_t expected_queueing_time;
        if (!page_cu->GetExpectedTaskQueueingDuration(&expected_queueing_time))
          continue;

        RecordExpectedQueueingTimeForUkm(page_cu->id(), expected_queueing_time);
      }
    }
  }
}

void MetricsCollector::OnFrameEventReceived(
    FrameNodeImpl* frame_cu,
    resource_coordinator::mojom::Event event) {
  if (event ==
      resource_coordinator::mojom::Event::kNonPersistentNotificationCreated) {
    auto* page_cu = frame_cu->GetPageNode();
    // Only record metrics while it is backgrounded.
    if (!page_cu || page_cu->is_visible() || !ShouldReportMetrics(page_cu)) {
      return;
    }
    MetricsReportRecord& record =
        metrics_report_record_map_.find(page_cu->id())->second;
    record.first_non_persistent_notification_created.OnSignalReceived(
        frame_cu->IsMainFrame(), page_cu->TimeSinceLastVisibilityChange(),
        coordination_unit_graph().ukm_recorder());
  }
}

void MetricsCollector::OnPageEventReceived(
    PageNodeImpl* page_cu,
    resource_coordinator::mojom::Event event) {
  if (event == resource_coordinator::mojom::Event::kTitleUpdated) {
    // Only record metrics while it is backgrounded.
    if (page_cu->is_visible() || !ShouldReportMetrics(page_cu))
      return;
    MetricsReportRecord& record =
        metrics_report_record_map_.find(page_cu->id())->second;
    record.first_title_updated.OnSignalReceived(
        true, page_cu->TimeSinceLastVisibilityChange(),
        coordination_unit_graph().ukm_recorder());
  } else if (event == resource_coordinator::mojom::Event::kFaviconUpdated) {
    // Only record metrics while it is backgrounded.
    if (page_cu->is_visible() || !ShouldReportMetrics(page_cu))
      return;
    MetricsReportRecord& record =
        metrics_report_record_map_.find(page_cu->id())->second;
    record.first_favicon_updated.OnSignalReceived(
        true, page_cu->TimeSinceLastVisibilityChange(),
        coordination_unit_graph().ukm_recorder());
  }
}

void MetricsCollector::OnIsVisibleChanged(PageNodeImpl* page_node) {
  // The page becomes visible again, clear all records in order to
  // report metrics when page becomes invisible next time.
  if (page_node->is_visible())
    ResetMetricsReportRecord(page_node->id());
}

bool MetricsCollector::ShouldReportMetrics(const PageNodeImpl* page_cu) {
  return page_cu->TimeSinceLastNavigation() > kMetricsReportDelayTimeout;
}

bool MetricsCollector::IsCollectingExpectedQueueingTimeForUkm(
    const resource_coordinator::CoordinationUnitID& page_cu_id) {
  UkmCollectionState& state = ukm_collection_state_map_[page_cu_id];
  return state.ukm_source_id != ukm::kInvalidSourceId &&
         ++state.num_unreported_eqt_measurements >= frequency_ukm_eqt_reported_;
}

void MetricsCollector::RecordExpectedQueueingTimeForUkm(
    const resource_coordinator::CoordinationUnitID& page_cu_id,
    int64_t expected_queueing_time) {
  UkmCollectionState& state = ukm_collection_state_map_[page_cu_id];
  state.num_unreported_eqt_measurements = 0u;
  ukm::builders::ResponsivenessMeasurement(state.ukm_source_id)
      .SetExpectedTaskQueueingDuration(expected_queueing_time)
      .Record(coordination_unit_graph().ukm_recorder());
}

void MetricsCollector::UpdateUkmSourceIdForPage(
    const resource_coordinator::CoordinationUnitID& page_cu_id,
    ukm::SourceId ukm_source_id) {
  UkmCollectionState& state = ukm_collection_state_map_[page_cu_id];

  state.ukm_source_id = ukm_source_id;
  // Updating the |ukm_source_id| restarts usage collection.
  state.num_unreported_eqt_measurements = 0u;
}

void MetricsCollector::UpdateWithFieldTrialParams() {
  frequency_ukm_eqt_reported_ = base::GetFieldTrialParamByFeatureAsInt(
      ukm::kUkmFeature, "FrequencyUKMExpectedQueueingTime",
      kDefaultFrequencyUkmEQTReported);
}

void MetricsCollector::ResetMetricsReportRecord(
    resource_coordinator::CoordinationUnitID cu_id) {
  DCHECK(metrics_report_record_map_.find(cu_id) !=
         metrics_report_record_map_.end());
  metrics_report_record_map_.find(cu_id)->second.Reset();
}

MetricsCollector::MetricsReportRecord::MetricsReportRecord() = default;

MetricsCollector::MetricsReportRecord::MetricsReportRecord(
    const MetricsReportRecord& other) = default;

void MetricsCollector::MetricsReportRecord::UpdateUKMSourceID(
    int64_t ukm_source_id) {
  first_favicon_updated.SetUKMSourceID(ukm_source_id);
  first_non_persistent_notification_created.SetUKMSourceID(ukm_source_id);
  first_title_updated.SetUKMSourceID(ukm_source_id);
}

void MetricsCollector::MetricsReportRecord::Reset() {
  first_favicon_updated.Reset();
  first_non_persistent_notification_created.Reset();
  first_title_updated.Reset();
}

}  // namespace performance_manager
