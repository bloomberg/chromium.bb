// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_scheduler_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

namespace {

// The InstantTethering.HostScanBatchDuration metric meaures the duration of a
// batch of host scans. A "batch" of host scans here refers to a group of host
// scans which occur just after each other. For example, two 30-second scans
// with a 5-second gap between them are considered a single 65-second batch host
// scan for this metric. For two back-to-back scans to be considered part of the
// same batch metric, they must be at most kMaxNumSecondsBetweenBatchScans
// seconds apart.
const int64_t kMaxNumSecondsBetweenBatchScans = 60;

// Minimum value for the scan length metric.
const int64_t kMinScanMetricSeconds = 1;

// Maximum value for the scan length metric.
const int64_t kMaxScanMetricsDays = 1;

// Number of buckets in the metric.
const int kNumMetricsBuckets = 1000;

}  // namespace

HostScanSchedulerImpl::HostScanSchedulerImpl(
    NetworkStateHandler* network_state_handler,
    HostScanner* host_scanner)
    : network_state_handler_(network_state_handler),
      host_scanner_(host_scanner),
      timer_(base::MakeUnique<base::OneShotTimer>()),
      clock_(base::MakeUnique<base::DefaultClock>()),
      weak_ptr_factory_(this) {
  network_state_handler_->AddObserver(this, FROM_HERE);
  host_scanner_->AddObserver(this);
}

HostScanSchedulerImpl::~HostScanSchedulerImpl() {
  network_state_handler_->SetTetherScanState(false);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  host_scanner_->RemoveObserver(this);

  // If the most recent batch of host scans has already been logged, return
  // early.
  if (!host_scanner_->IsScanActive() && !timer_->IsRunning())
    return;

  // If a scan is still active during shutdown, there is not enough time to wait
  // for the scan to finish before logging its full duration. Instead, mark the
  // current time as the end of the scan so that it can be logged.
  if (host_scanner_->IsScanActive())
    last_scan_end_timestamp_ = clock_->Now();

  LogHostScanBatchMetric();
}

void HostScanSchedulerImpl::ScheduleScan() {
  EnsureScan();
}

void HostScanSchedulerImpl::DefaultNetworkChanged(const NetworkState* network) {
  if ((!network || !network->IsConnectingOrConnected()) &&
      !IsTetherNetworkConnectingOrConnected()) {
    EnsureScan();
  }
}

void HostScanSchedulerImpl::ScanRequested() {
  EnsureScan();
}

void HostScanSchedulerImpl::ScanFinished() {
  network_state_handler_->SetTetherScanState(false);

  last_scan_end_timestamp_ = clock_->Now();
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kMaxNumSecondsBetweenBatchScans),
                base::Bind(&HostScanSchedulerImpl::LogHostScanBatchMetric,
                           weak_ptr_factory_.GetWeakPtr()));
}

void HostScanSchedulerImpl::SetTestDoubles(
    std::unique_ptr<base::Timer> test_timer,
    std::unique_ptr<base::Clock> test_clock) {
  timer_ = std::move(test_timer);
  clock_ = std::move(test_clock);
}

void HostScanSchedulerImpl::EnsureScan() {
  if (host_scanner_->IsScanActive())
    return;

  // If the timer is running, this new scan is part of the same batch as the
  // previous scan, so the timer should be stopped (it will be restarted after
  // the new scan finishes). If the timer is not running, the new scan is part
  // of a new batch, so the start timestamp should be recorded.
  if (timer_->IsRunning())
    timer_->Stop();
  else
    last_scan_batch_start_timestamp_ = clock_->Now();

  host_scanner_->StartScan();
  network_state_handler_->SetTetherScanState(true);
}

bool HostScanSchedulerImpl::IsTetherNetworkConnectingOrConnected() {
  return network_state_handler_->ConnectingNetworkByType(
             NetworkTypePattern::Tether()) ||
         network_state_handler_->ConnectedNetworkByType(
             NetworkTypePattern::Tether());
}

void HostScanSchedulerImpl::LogHostScanBatchMetric() {
  DCHECK(!last_scan_batch_start_timestamp_.is_null());
  DCHECK(!last_scan_end_timestamp_.is_null());

  base::TimeDelta batch_duration =
      last_scan_end_timestamp_ - last_scan_batch_start_timestamp_;
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "InstantTethering.HostScanBatchDuration", batch_duration,
      base::TimeDelta::FromSeconds(kMinScanMetricSeconds) /* min */,
      base::TimeDelta::FromDays(kMaxScanMetricsDays) /* max */,
      kNumMetricsBuckets /* bucket_count */);

  PA_LOG(INFO) << "Logging host scan batch duration. Duration was "
               << batch_duration.InSeconds() << " seconds.";
}

}  // namespace tether

}  // namespace chromeos
