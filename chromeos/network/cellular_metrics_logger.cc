// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_metrics_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// static
const base::TimeDelta CellularMetricsLogger::kInitializationTimeout =
    base::TimeDelta::FromSeconds(15);

// static
CellularMetricsLogger::ActivationState
CellularMetricsLogger::ActivationStateToEnum(const std::string& state) {
  if (state == shill::kActivationStateActivated)
    return ActivationState::kActivated;
  else if (state == shill::kActivationStateActivating)
    return ActivationState::kActivating;
  else if (state == shill::kActivationStateNotActivated)
    return ActivationState::kNotActivated;
  else if (state == shill::kActivationStatePartiallyActivated)
    return ActivationState::kPartiallyActivated;

  return ActivationState::kUnknown;
}

// static
bool CellularMetricsLogger::IsLoggedInUserOwnerOrRegular() {
  if (!LoginState::IsInitialized())
    return false;

  LoginState::LoggedInUserType user_type =
      LoginState::Get()->GetLoggedInUserType();
  return user_type == LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER ||
         user_type == LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR;
}

CellularMetricsLogger::ConnectingNetworkInfo::ConnectingNetworkInfo(
    const std::string& network_guid,
    base::TimeTicks start_time)
    : network_guid(network_guid), start_time(start_time) {}

CellularMetricsLogger::CellularMetricsLogger() = default;

CellularMetricsLogger::~CellularMetricsLogger() {
  if (network_state_handler_)
    OnShuttingDown();

  if (initialized_ && LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
}

void CellularMetricsLogger::Init(NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);

  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  // Devices may already be present before this method is called.
  // Make sure that cellular availability is updated and initialization
  // timer is started properly.
  DeviceListChanged();
  initialized_ = true;
}

void CellularMetricsLogger::DeviceListChanged() {
  NetworkStateHandler::DeviceStateList device_list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &device_list);
  bool new_is_cellular_available = !device_list.empty();
  if (is_cellular_available_ == new_is_cellular_available)
    return;

  is_cellular_available_ = new_is_cellular_available;
  // Start a timer to wait for cellular networks to initialize.
  // This makes sure that intermediate not-connected states are
  // not logged before initialization is completed.
  if (is_cellular_available_) {
    initialization_timer_.Start(
        FROM_HERE, kInitializationTimeout, this,
        &CellularMetricsLogger::OnInitializationTimeout);
  }
}

void CellularMetricsLogger::OnInitializationTimeout() {
  LogActivationState();
  LogCellularUsageCount();
}

void CellularMetricsLogger::LoggedInStateChanged() {
  if (!CellularMetricsLogger::IsLoggedInUserOwnerOrRegular())
    return;

  // This flag enures that activation state is only logged once when
  // the user logs in.
  is_activation_state_logged_ = false;
  LogActivationState();
}

void CellularMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  DCHECK(network_state_handler_);
  LogTimeToConnected(network);
  LogCellularUsageCount();
}

void CellularMetricsLogger::LogTimeToConnected(const NetworkState* network) {
  if (network->type().empty() ||
      !network->Matches(NetworkTypePattern::Cellular())) {
    return;
  }

  if (network->activation_state() != shill::kActivationStateActivated)
    return;

  if (network->IsConnectingState()) {
    if (!connecting_network_info_.has_value())
      connecting_network_info_.emplace(network->guid(), base::TimeTicks::Now());

    return;
  }

  // We could be receiving a connection state change for a network different
  // from the one observed when the start time was recorded. Make sure that we
  // only log the time to connected of the corresponding network.
  if (!connecting_network_info_.has_value() ||
      network->guid() != connecting_network_info_->network_guid) {
    return;
  }

  if (network->IsConnectedState()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Network.Cellular.Connection.TimeToConnected",
        base::TimeTicks::Now() - connecting_network_info_->start_time);
  }

  // This is hit when the network is no longer in connecting state,
  // successfully connected or otherwise. Reset the connecting info
  // so that it is not used for further connection state changes.
  connecting_network_info_.reset();
}

void CellularMetricsLogger::LogActivationState() {
  if (!is_cellular_available_ || is_activation_state_logged_ ||
      !CellularMetricsLogger::IsLoggedInUserOwnerOrRegular())
    return;

  const NetworkState* cellular_network =
      network_state_handler_->FirstNetworkByType(
          NetworkTypePattern::Cellular());

  if (!cellular_network)
    return;

  ActivationState activation_state =
      ActivationStateToEnum(cellular_network->activation_state());
  UMA_HISTOGRAM_ENUMERATION("Network.Cellular.Activation.StatusAtLogin",
                            activation_state);
  is_activation_state_logged_ = true;
}

void CellularMetricsLogger::LogCellularUsageCount() {
  if (!is_cellular_available_)
    return;

  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::NonVirtual(), &network_list);

  bool is_cellular_connected = false;
  bool is_non_cellular_connected = false;
  for (const auto* network : network_list) {
    if (!network->IsConnectedState())
      continue;

    if (network->Matches(NetworkTypePattern::Cellular()))
      is_cellular_connected = true;
    else
      is_non_cellular_connected = true;
  }

  // Discard not-connected states received before the timer runs out.
  if (!is_cellular_connected && initialization_timer_.IsRunning())
    return;

  CellularUsage usage;
  if (is_cellular_connected) {
    usage = is_non_cellular_connected
                ? CellularUsage::kConnectedWithOtherNetwork
                : CellularUsage::kConnectedAndOnlyNetwork;
  } else {
    usage = CellularUsage::kNotConnected;
  }

  if (usage == last_cellular_usage_)
    return;
  last_cellular_usage_ = usage;

  UMA_HISTOGRAM_ENUMERATION("Network.Cellular.Usage.Count", usage);
}

void CellularMetricsLogger::OnShuttingDown() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  network_state_handler_ = nullptr;
}

}  // namespace chromeos
