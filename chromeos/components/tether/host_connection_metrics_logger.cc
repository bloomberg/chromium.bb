// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_connection_metrics_logger.h"

#include "base/metrics/histogram_macros.h"

namespace chromeos {

namespace tether {

HostConnectionMetricsLogger::HostConnectionMetricsLogger() = default;

HostConnectionMetricsLogger::~HostConnectionMetricsLogger() = default;

void HostConnectionMetricsLogger::RecordConnectionToHostResult(
    ConnectionToHostResult result) {
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
    default:
      NOTREACHED();
  };
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
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.SuccessRate", event_type,
      ConnectionToHostResult_SuccessEventType::SUCCESS_MAX);

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

}  // namespace tether

}  // namespace chromeos
