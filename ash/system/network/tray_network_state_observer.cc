// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network_state_observer.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStatePropertiesPtr;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

namespace {

const int kUpdateFrequencyMs = 1000;

NetworkStatePropertiesPtr GetConnectingOrConnected(
    const NetworkStatePropertiesPtr* connecting_network,
    const NetworkStatePropertiesPtr* connected_network) {
  if (connecting_network &&
      (!connected_network || connecting_network->get()->connect_requested)) {
    // If connecting to a network, and there is either no connected network or
    // the connection was user requested, use the connecting network.
    return connecting_network->Clone();
  }
  if (connected_network)
    return connected_network->Clone();
  return nullptr;
}

}  // namespace

namespace ash {

TrayNetworkStateObserver::TrayNetworkStateObserver(
    service_manager::Connector* connector)
    : update_frequency_(kUpdateFrequencyMs) {
  if (ui::ScopedAnimationDurationScaleMode::duration_scale_mode() !=
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION) {
    update_frequency_ = 0;  // Send updates immediately for tests.
  }
  if (connector)  // May be null in tests.
    BindCrosNetworkConfig(connector);
}

TrayNetworkStateObserver::~TrayNetworkStateObserver() = default;

void TrayNetworkStateObserver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TrayNetworkStateObserver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

DeviceStateProperties* TrayNetworkStateObserver::GetDevice(NetworkType type) {
  auto iter = devices_.find(type);
  if (iter == devices_.end())
    return nullptr;
  return iter->second.get();
}

DeviceStateType TrayNetworkStateObserver::GetDeviceState(NetworkType type) {
  DeviceStateProperties* device = GetDevice(type);
  return device ? device->state : DeviceStateType::kUnavailable;
}

// CrosNetworkConfigObserver

void TrayNetworkStateObserver::OnActiveNetworksChanged(
    std::vector<NetworkStatePropertiesPtr> networks) {
  UpdateActiveNetworks(std::move(networks));
  SendActiveNetworkStateChanged();
}

void TrayNetworkStateObserver::OnNetworkStateListChanged() {
  NotifyNetworkListChanged();
}

void TrayNetworkStateObserver::OnDeviceStateListChanged() {
  GetDeviceStateList();
}

void TrayNetworkStateObserver::BindCrosNetworkConfig(
    service_manager::Connector* connector) {
  // Ensure bindings are reset in case this is called after a failure.
  cros_network_config_observer_binding_.Close();
  cros_network_config_ptr_.reset();

  connector->BindInterface(chromeos::network_config::mojom::kServiceName,
                           &cros_network_config_ptr_);
  chromeos::network_config::mojom::CrosNetworkConfigObserverPtr observer_ptr;
  cros_network_config_observer_binding_.Bind(mojo::MakeRequest(&observer_ptr));
  cros_network_config_ptr_->AddObserver(std::move(observer_ptr));
  GetDeviceStateList();

  // If the connection is lost (e.g. due to a crash), attempt to rebind it.
  cros_network_config_ptr_.set_connection_error_handler(
      base::BindOnce(&TrayNetworkStateObserver::BindCrosNetworkConfig,
                     base::Unretained(this), connector));
}

void TrayNetworkStateObserver::GetDeviceStateList() {
  cros_network_config_ptr_->GetDeviceStateList(base::BindOnce(
      &TrayNetworkStateObserver::OnGetDeviceStateList, base::Unretained(this)));
}

void TrayNetworkStateObserver::OnGetDeviceStateList(
    std::vector<DeviceStatePropertiesPtr> devices) {
  devices_.clear();
  for (auto& device : devices) {
    NetworkType type = device->type;
    if (base::ContainsKey(devices_, type))
      continue;  // Ignore multiple entries with the same type.
    devices_.emplace(std::make_pair(type, std::move(device)));
  }

  GetActiveNetworks();  // Will trigger an observer event.
}

void TrayNetworkStateObserver::GetActiveNetworks() {
  DCHECK(cros_network_config_ptr_);
  cros_network_config_ptr_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                         /*limit=*/0),
      base::BindOnce(&TrayNetworkStateObserver::OnActiveNetworksChanged,
                     base::Unretained(this)));
}

void TrayNetworkStateObserver::UpdateActiveNetworks(
    std::vector<NetworkStatePropertiesPtr> networks) {
  active_cellular_.reset();
  active_vpn_.reset();

  const NetworkStatePropertiesPtr* connected_network = nullptr;
  const NetworkStatePropertiesPtr* connected_non_cellular = nullptr;
  const NetworkStatePropertiesPtr* connecting_network = nullptr;
  const NetworkStatePropertiesPtr* connecting_non_cellular = nullptr;
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (network->type == NetworkType::kVPN) {
      if (!active_vpn_)
        active_vpn_ = network.Clone();
      continue;
    }
    if (network->type == NetworkType::kCellular) {
      if (!active_cellular_)
        active_cellular_ = network.Clone();
    }
    if (chromeos::network_config::StateIsConnected(network->connection_state)) {
      if (!connected_network)
        connected_network = &network;
      if (!connected_non_cellular && network->type != NetworkType::kCellular) {
        connected_non_cellular = &network;
      }
      continue;
    }
    // Active non connected networks are connecting.
    if (chromeos::network_config::NetworkStateMatchesType(
            network.get(), NetworkType::kWireless)) {
      if (!connecting_network)
        connecting_network = &network;
      if (!connecting_non_cellular && network->type != NetworkType::kCellular) {
        connecting_non_cellular = &network;
      }
    }
  }

  VLOG_IF(2, connected_network)
      << __func__ << ": Connected network: " << connected_network->get()->name
      << " State: " << connected_network->get()->connection_state;
  VLOG_IF(2, connecting_network)
      << __func__ << ": Connecting network: " << connecting_network->get()->name
      << " State: " << connecting_network->get()->connection_state;

  default_network_ =
      GetConnectingOrConnected(connecting_network, connected_network);
  VLOG_IF(2, default_network_)
      << __func__ << ": Default network: " << default_network_->name;

  active_non_cellular_ =
      GetConnectingOrConnected(connecting_non_cellular, connected_non_cellular);
}

void TrayNetworkStateObserver::NotifyNetworkListChanged() {
  if (timer_.IsRunning())
    return;
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(update_frequency_),
      base::BindRepeating(&TrayNetworkStateObserver::SendNetworkListChanged,
                          base::Unretained(this)));
}

void TrayNetworkStateObserver::SendActiveNetworkStateChanged() {
  for (auto& observer : observer_list_)
    observer.ActiveNetworkStateChanged();
}

void TrayNetworkStateObserver::SendNetworkListChanged() {
  for (auto& observer : observer_list_)
    observer.NetworkListChanged();
}

void TrayNetworkStateObserver::Observer::ActiveNetworkStateChanged() {}

void TrayNetworkStateObserver::Observer::NetworkListChanged() {}

}  // namespace ash
