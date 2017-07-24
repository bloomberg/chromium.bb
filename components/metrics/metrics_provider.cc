// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_provider.h"

#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

MetricsProvider::MetricsProvider() {
}

MetricsProvider::~MetricsProvider() {
}

void MetricsProvider::Init() {
}

void MetricsProvider::AsyncInit(const base::Closure& done_callback) {
  done_callback.Run();
}

void MetricsProvider::OnDidCreateMetricsLog() {
}

void MetricsProvider::OnRecordingEnabled() {
}

void MetricsProvider::OnRecordingDisabled() {
}

void MetricsProvider::OnAppEnterBackground() {
}

bool MetricsProvider::ProvideIndependentMetrics(
    SystemProfileProto* system_profile_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  return false;
}

void MetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
}

bool MetricsProvider::HasInitialStabilityMetrics() {
  return false;
}

void MetricsProvider::ProvidePreviousSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  ProvideInitialStabilityMetrics(uma_proto->mutable_system_profile());
  ProvideStabilityMetrics(uma_proto->mutable_system_profile());
}

void MetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  ProvideStabilityMetrics(uma_proto->mutable_system_profile());
  ProvideGeneralMetrics(uma_proto);
}

void MetricsProvider::ProvideInitialStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
}

void MetricsProvider::ProvideStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
}

void MetricsProvider::ClearSavedStabilityMetrics() {
}

void MetricsProvider::ProvideGeneralMetrics(
    ChromeUserMetricsExtension* uma_proto) {
}

void MetricsProvider::RecordHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
}

void MetricsProvider::RecordInitialHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
}

}  // namespace metrics
