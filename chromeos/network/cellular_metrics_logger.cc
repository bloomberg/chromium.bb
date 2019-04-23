// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_metrics_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

// static
const base::TimeDelta CellularMetricsLogger::kInitializationTimeout =
    base::TimeDelta::FromSeconds(15);

CellularMetricsLogger::CellularMetricsLogger() = default;

CellularMetricsLogger::~CellularMetricsLogger() {
  if (network_state_handler_)
    OnShuttingDown();
}

void CellularMetricsLogger::Init(NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);

  // Devices may already be present before this method is called.
  // Make sure that cellular availability is updated and initialization
  // timer is started properly.
  DeviceListChanged();
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
    initialization_timer_.Start(FROM_HERE, kInitializationTimeout, this,
                                &CellularMetricsLogger::LogCellularUsageCount);
  }
}

void CellularMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  DCHECK(network_state_handler_);
  LogCellularUsageCount();
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
