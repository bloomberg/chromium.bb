// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"

namespace chromeos {

namespace tether {

// Wrapper around metrics reporting for host connection results. Clients are
// expected to report the result of a host connection attempt once it has
// concluded.
class HostConnectionMetricsLogger {
 public:
  enum ConnectionToHostResult {
    CONNECTION_RESULT_PROVISIONING_FAILED,
    CONNECTION_RESULT_SUCCESS,
    CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR,
    CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_TIMEOUT,
    CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER,
    CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_INTERNAL_ERROR,
    CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_REQUIRED,
    CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_NOT_REQUIRED
  };

  // Record the result of an attempted host connection.
  virtual void RecordConnectionToHostResult(ConnectionToHostResult result);

  HostConnectionMetricsLogger();
  virtual ~HostConnectionMetricsLogger();

 private:
  friend class HostConnectionMetricsLoggerTest;
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultProvisioningFailure);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultSuccess);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailure);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_Timeout);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_CanceledByUser);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_InternalError);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureTetheringTimeout_SetupRequired);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureTetheringTimeout_SetupNotRequired);

  // An Instant Tethering connection can fail for several different reasons.
  // Though traditionally success and each failure case would be logged to a
  // single enum, we have chosen to start at a top-level of view of simply
  // "success vs failure", and then iteratively breaking down the failure count
  // into its types (and possibly sub-types). Because of this cascading nature,
  // when a failure sub-type occurs, the code path in question must report that
  // sub-type as well as all the super-types above it. This would be cumbersome,
  // and thus HostConnectionMetricsLogger exists to provide a simple API which
  // handles all the other metrics that need to be reported.
  //
  // The most top-level metric is
  // InstantTethering.ConnectionToHostResult.ProvisioningFailureRate. Its
  // breakdown, and the breakdown of its subsquent metrics, is described in
  // tools/metrics/histograms/histograms.xml.
  enum ConnectionToHostResult_ProvisioningFailureEventType {
    PROVISIONING_FAILED = 0,
    OTHER = 1,
    PROVISIONING_FAILURE_MAX
  };

  enum ConnectionToHostResult_SuccessEventType {
    SUCCESS = 0,
    FAILURE = 1,
    SUCCESS_MAX
  };

  enum ConnectionToHostResult_FailureEventType {
    UNKNOWN_ERROR = 0,
    TETHERING_TIMED_OUT = 1,
    CLIENT_CONNECTION_ERROR = 2,
    FAILURE_MAX
  };

  enum ConnectionToHostResult_FailureClientConnectionEventType {
    TIMEOUT = 0,
    CANCELED_BY_USER = 1,
    INTERNAL_ERROR = 2,
    FAILURE_CLIENT_CONNECTION_MAX
  };

  enum ConnectionToHostResult_FailureTetheringTimeoutEventType {
    FIRST_TIME_SETUP_WAS_REQUIRED = 0,
    FIRST_TIME_SETUP_WAS_NOT_REQUIRED = 1,
    FAILURE_TETHERING_TIMEOUT_MAX
  };

  // Record if a host connection attempt never went through due to provisioning
  // failure, or otherwise continued.
  void RecordConnectionResultProvisioningFailure(
      ConnectionToHostResult_ProvisioningFailureEventType event_type);

  // Record if a host connection attempt succeeded or failed. Failure is
  // covered by the RecordConnectionResultFailure() method.
  void RecordConnectionResultSuccess(
      ConnectionToHostResult_SuccessEventType event_type);

  // Record how a host connection attempt failed. Failure due to client error or
  // tethering timeout is covered by the
  // RecordConnectionResultFailureClientConnection() or
  // RecordConnectionResultFailureTetheringTimeout() methods, respectively.
  void RecordConnectionResultFailure(
      ConnectionToHostResult_FailureEventType event_type);

  // Record how a host connection attempt failed due to a client error.
  void RecordConnectionResultFailureClientConnection(
      ConnectionToHostResult_FailureClientConnectionEventType event_type);

  // Record the conditions of when host connection attempt failed due to
  // the host timing out during tethering.
  void RecordConnectionResultFailureTetheringTimeout(
      ConnectionToHostResult_FailureTetheringTimeoutEventType event_type);

  DISALLOW_COPY_AND_ASSIGN(HostConnectionMetricsLogger);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_
