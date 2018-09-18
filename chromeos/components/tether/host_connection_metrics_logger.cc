// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_connection_metrics_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "chromeos/chromeos_features.h"

namespace chromeos {

namespace tether {

HostConnectionMetricsLogger::HostConnectionMetricsLogger(
    BleConnectionManager* connection_manager,
    ActiveHost* active_host)
    : connection_manager_(connection_manager),
      active_host_(active_host),
      clock_(base::DefaultClock::GetInstance()) {
  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
    connection_manager_->AddMetricsObserver(this);
  active_host_->AddObserver(this);
}

HostConnectionMetricsLogger::~HostConnectionMetricsLogger() {
  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
    connection_manager_->RemoveMetricsObserver(this);
  active_host_->RemoveObserver(this);
}

void HostConnectionMetricsLogger::RecordConnectionToHostResult(
    ConnectionToHostResult result,
    const std::string& device_id) {
  // Persist this value for later use in RecordConnectionResultSuccess(). It
  // will be cleared once used.
  active_host_device_id_ = device_id;

  switch (result) {
    case ConnectionToHostResult::CONNECTION_RESULT_PROVISIONING_FAILED:
      RecordConnectionResultProvisioningFailure(
          ConnectionToHostResult_ProvisioningFailureEventType::
              PROVISIONING_FAILED);
      break;
    case ConnectionToHostResult::CONNECTION_RESULT_SUCCESS:
      RecordConnectionResultSuccess(
          ConnectionToHostResult_SuccessEventType::SUCCESS);
      break;
    case ConnectionToHostResult::CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_TIMEOUT:
      RecordConnectionResultFailureClientConnection(
          ConnectionToHostResult_FailureClientConnectionEventType::TIMEOUT);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER:
      RecordConnectionResultFailureClientConnection(
          ConnectionToHostResult_FailureClientConnectionEventType::
              CANCELED_BY_USER);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_INTERNAL_ERROR:
      RecordConnectionResultFailureClientConnection(
          ConnectionToHostResult_FailureClientConnectionEventType::
              INTERNAL_ERROR);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_REQUIRED:
      RecordConnectionResultFailureTetheringTimeout(
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_REQUIRED);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_NOT_REQUIRED:
      RecordConnectionResultFailureTetheringTimeout(
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_NOT_REQUIRED);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_TETHERING_UNSUPPORTED:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::TETHERING_UNSUPPORTED);
      break;
    case ConnectionToHostResult::CONNECTION_RESULT_FAILURE_NO_CELL_DATA:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::NO_CELL_DATA);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_FAILED:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::ENABLING_HOTSPOT_FAILED);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_TIMEOUT:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::ENABLING_HOTSPOT_TIMEOUT);
      break;
    case ConnectionToHostResult::CONNECTION_RESULT_FAILURE_NO_RESPONSE:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::NO_RESPONSE);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_INVALID_HOTSPOT_CREDENTIALS:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::INVALID_HOTSPOT_CREDENTIALS);
      break;
    default:
      NOTREACHED();
  };
}

void HostConnectionMetricsLogger::OnAdvertisementReceived(
    const std::string& device_id,
    bool is_background_advertisement) {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  device_id_to_received_background_advertisement_[device_id] =
      is_background_advertisement;
}

void HostConnectionMetricsLogger::OnActiveHostChanged(
    const ActiveHost::ActiveHostChangeInfo& change_info) {
  if (change_info.new_status == ActiveHost::ActiveHostStatus::CONNECTING) {
    connect_to_host_start_time_ = clock_->Now();
  } else if (change_info.new_status ==
             ActiveHost::ActiveHostStatus::CONNECTED) {
    RecordConnectToHostDuration(change_info.new_active_host->GetDeviceId());
  }
}

void HostConnectionMetricsLogger::RecordConnectionResultProvisioningFailure(
    ConnectionToHostResult_ProvisioningFailureEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.ProvisioningFailureRate",
      event_type,
      ConnectionToHostResult_ProvisioningFailureEventType::
          PROVISIONING_FAILURE_MAX);
}

void HostConnectionMetricsLogger::RecordConnectionResultSuccess(
    ConnectionToHostResult_SuccessEventType event_type) {
  DCHECK(!active_host_device_id_.empty());

  bool is_background_advertisement =
      device_id_to_received_background_advertisement_[active_host_device_id_];
  active_host_device_id_.clear();

  if (is_background_advertisement ||
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    UMA_HISTOGRAM_ENUMERATION(
        "InstantTethering.ConnectionToHostResult.SuccessRate.Background",
        event_type, ConnectionToHostResult_SuccessEventType::SUCCESS_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "InstantTethering.ConnectionToHostResult.SuccessRate", event_type,
        ConnectionToHostResult_SuccessEventType::SUCCESS_MAX);
  }

  RecordConnectionResultProvisioningFailure(
      ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

void HostConnectionMetricsLogger::RecordConnectionResultFailure(
    ConnectionToHostResult_FailureEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.Failure", event_type,
      ConnectionToHostResult_FailureEventType::FAILURE_MAX);

  RecordConnectionResultSuccess(
      ConnectionToHostResult_SuccessEventType::FAILURE);
}

void HostConnectionMetricsLogger::RecordConnectionResultFailureClientConnection(
    ConnectionToHostResult_FailureClientConnectionEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.Failure.ClientConnection",
      event_type,
      ConnectionToHostResult_FailureClientConnectionEventType::
          FAILURE_CLIENT_CONNECTION_MAX);

  RecordConnectionResultFailure(
      ConnectionToHostResult_FailureEventType::CLIENT_CONNECTION_ERROR);
}

void HostConnectionMetricsLogger::RecordConnectionResultFailureTetheringTimeout(
    ConnectionToHostResult_FailureTetheringTimeoutEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.Failure.TetheringTimeout",
      event_type,
      ConnectionToHostResult_FailureTetheringTimeoutEventType::
          FAILURE_TETHERING_TIMEOUT_MAX);

  RecordConnectionResultFailure(
      ConnectionToHostResult_FailureEventType::TETHERING_TIMED_OUT);
}

void HostConnectionMetricsLogger::RecordConnectToHostDuration(
    const std::string device_id) {
  DCHECK(!connect_to_host_start_time_.is_null());

  bool is_background_advertisement =
      device_id_to_received_background_advertisement_[device_id];

  base::TimeDelta connect_to_host_duration =
      clock_->Now() - connect_to_host_start_time_;
  connect_to_host_start_time_ = base::Time();

  if (is_background_advertisement ||
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "InstantTethering.Performance.ConnectToHostDuration.Background",
        connect_to_host_duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "InstantTethering.Performance.ConnectToHostDuration",
        connect_to_host_duration);
  }
}

void HostConnectionMetricsLogger::SetClockForTesting(base::Clock* test_clock) {
  clock_ = test_clock;
}

}  // namespace tether

}  // namespace chromeos
