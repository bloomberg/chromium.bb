// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_METRICS_LOGGER_H_
#define CHROMEOS_NETWORK_CELLULAR_METRICS_LOGGER_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;
class NetworkState;
class CellularMetricsLoggerTest;

// Class for tracking cellular network related metrics.
//
// This class adds observers on network state and logs histograms
// when relevant cellular network events are triggered.
//
// Note: This class does not start logging metrics until Init() is
// invoked.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularMetricsLogger
    : public NetworkStateHandlerObserver,
      public LoginState::Observer {
 public:
  CellularMetricsLogger();
  ~CellularMetricsLogger() override;

  void Init(NetworkStateHandler* network_state_handler);

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  // NetworkStateHandlerObserver::
  void DeviceListChanged() override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

 private:
  friend class CellularMetricsLoggerTest;
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest, CellularUsageCountTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularUsageCountDongleTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularActivationStateAtLoginTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularTimeToConnectedTest);

  // The amount of time after cellular device is added to device list,
  // after which cellular device is considered initialized.
  static const base::TimeDelta kInitializationTimeout;

  // Stores information about cellular network that is in the connecting state.
  struct ConnectingNetworkInfo {
    ConnectingNetworkInfo(const std::string& network_guid,
                          base::TimeTicks start_time);
    std::string network_guid;
    base::TimeTicks start_time;
  };

  // Usage type for cellular network. These values are persisted to logs.
  // Entries should not be renumbered and numberic values should never
  // be reused.
  enum class CellularUsage {
    kConnectedAndOnlyNetwork = 0,
    kConnectedWithOtherNetwork = 1,
    kNotConnected = 2,
    kMaxValue = kNotConnected
  };

  // Activation state for cellular network.
  // These values are persisted to logs. Entries should not be renumbered
  // and numberic values should never be reused.
  enum class ActivationState {
    kActivated = 0,
    kActivating = 1,
    kNotActivated = 2,
    kPartiallyActivated = 3,
    kUnknown = 4,
    kMaxValue = kUnknown
  };

  // Convert shill activation state string to ActivationState enum.
  static ActivationState ActivationStateToEnum(const std::string& state);

  // Checks whether the current logged in user type is an owner or regular.
  static bool IsLoggedInUserOwnerOrRegular();

  void OnInitializationTimeout();

  // Tracks cellular network connection state and logs time to connected.
  void LogTimeToConnected(const NetworkState* network);

  // Logs the activation state of cellular network if available and
  // if |log_activation_state_| is true.
  void LogActivationState();

  // This checks the state of connected networks and logs
  // cellular network usage histogram. Histogram is only logged
  // when usage state changes.
  void LogCellularUsageCount();

  // Tracks the last cellular network usage state.
  base::Optional<CellularUsage> last_cellular_usage_;

  // Tracks whether cellular device is available or not.
  bool is_cellular_available_ = false;

  NetworkStateHandler* network_state_handler_ = nullptr;

  // A timer to wait for cellular initialization. This is useful
  // to avoid tracking intermediate states when cellular network is
  // starting up.
  base::OneShotTimer initialization_timer_;

  // Tracks whether activation state is already logged for this
  // session.
  bool is_activation_state_logged_ = false;

  // Stores info about cellular network in when it is in
  // connecting state.
  base::Optional<ConnectingNetworkInfo> connecting_network_info_;

  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(CellularMetricsLogger);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_METRICS_LOGGER_H_
