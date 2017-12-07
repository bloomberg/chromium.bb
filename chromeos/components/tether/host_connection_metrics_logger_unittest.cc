// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_connection_metrics_logger.h"

#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

class HostConnectionMetricsLoggerTest : public testing::Test {
 protected:
  HostConnectionMetricsLoggerTest() = default;

  void SetUp() override {
    metrics_logger_ = base::MakeUnique<HostConnectionMetricsLogger>();
  }

  void VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.ProvisioningFailureRate",
        event_type, 1);
  }

  void VerifySuccess(
      HostConnectionMetricsLogger::ConnectionToHostResult_SuccessEventType
          event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.SuccessRate", event_type, 1);
  }

  void VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType
          event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.Failure", event_type, 1);
  }

  void VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.Failure.ClientConnection",
        event_type, 1);
  }

  void VerifyFailure_TetheringTimeout(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureTetheringTimeoutEventType event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.Failure.TetheringTimeout",
        event_type, 1);
  }

  std::unique_ptr<HostConnectionMetricsLogger> metrics_logger_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostConnectionMetricsLoggerTest);
};

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultProvisioningFailure) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_PROVISIONING_FAILED);

  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::
              PROVISIONING_FAILED);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectionResultSuccess) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_SUCCESS);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectionResultFailure) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR);

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_Timeout) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_TIMEOUT);

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::TIMEOUT);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_CanceledByUser) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER);

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::
              CANCELED_BY_USER);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_InternalError) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_INTERNAL_ERROR);

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::
              INTERNAL_ERROR);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringTimeout_SetupRequired) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_REQUIRED);

  VerifyFailure_TetheringTimeout(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_REQUIRED);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          TETHERING_TIMED_OUT);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringTimeout_SetupNotRequired) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_NOT_REQUIRED);

  VerifyFailure_TetheringTimeout(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_NOT_REQUIRED);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          TETHERING_TIMED_OUT);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringUnsupported) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_UNSUPPORTED);

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          TETHERING_UNSUPPORTED);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureNoCellData) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_NO_CELL_DATA);

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::NO_CELL_DATA);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
}

}  // namespace tether

}  // namespace chromeos
