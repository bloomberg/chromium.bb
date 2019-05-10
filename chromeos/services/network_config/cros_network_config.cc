// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/cros_network_config.h"

#include "chromeos/network/device_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config_mojom_traits.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace network_config {

namespace {

std::string ShillToONC(const std::string& shill_string,
                       const onc::StringTranslationEntry table[]) {
  std::string onc_string;
  if (!shill_string.empty())
    onc::TranslateStringToONC(table, shill_string, &onc_string);
  return onc_string;
}

mojom::NetworkType ShillTypeToMojo(const std::string& shill_type) {
  NetworkTypePattern type = NetworkTypePattern::Primitive(shill_type);
  if (type.Equals(NetworkTypePattern::Cellular()))
    return mojom::NetworkType::kCellular;
  if (type.MatchesPattern(NetworkTypePattern::EthernetOrEthernetEAP()))
    return mojom::NetworkType::kEthernet;
  if (type.Equals(NetworkTypePattern::Tether()))
    return mojom::NetworkType::kTether;
  if (type.Equals(NetworkTypePattern::VPN()))
    return mojom::NetworkType::kVPN;
  if (type.Equals(NetworkTypePattern::WiFi()))
    return mojom::NetworkType::kWiFi;
  if (type.Equals(NetworkTypePattern::Wimax()))
    return mojom::NetworkType::kWiMAX;
  NOTREACHED() << "Unsupported shill network type: " << shill_type;
  return mojom::NetworkType::kAll;  // Unsupported
}

NetworkTypePattern MojoTypeToPattern(mojom::NetworkType type) {
  switch (type) {
    case mojom::NetworkType::kAll:
      return NetworkTypePattern::Default();
    case mojom::NetworkType::kCellular:
      return NetworkTypePattern::Cellular();
    case mojom::NetworkType::kEthernet:
      return NetworkTypePattern::Ethernet();
    case mojom::NetworkType::kTether:
      return NetworkTypePattern::Tether();
    case mojom::NetworkType::kVPN:
      return NetworkTypePattern::VPN();
    case mojom::NetworkType::kWireless:
      return NetworkTypePattern::Wireless();
    case mojom::NetworkType::kWiFi:
      return NetworkTypePattern::WiFi();
    case mojom::NetworkType::kWiMAX:
      return NetworkTypePattern::Wimax();
      NOTREACHED();
  }
  return NetworkTypePattern::Default();
}

mojom::ConnectionStateType GetMojoConnectionStateType(
    const NetworkState* network) {
  if (network->IsConnectedState()) {
    if (network->IsCaptivePortal())
      return mojom::ConnectionStateType::kPortal;
    if (network->IsOnline())
      return mojom::ConnectionStateType::kOnline;
    return mojom::ConnectionStateType::kConnected;
  }
  if (network->IsConnectingState())
    return mojom::ConnectionStateType::kConnecting;
  return mojom::ConnectionStateType::kNotConnected;
}

mojom::VPNType ShillVpnTypeToMojo(const std::string& shill_vpn_type) {
  if (shill_vpn_type == shill::kProviderL2tpIpsec)
    return mojom::VPNType::kL2TPIPsec;
  if (shill_vpn_type == shill::kProviderOpenVpn)
    return mojom::VPNType::kOpenVPN;
  if (shill_vpn_type == shill::kProviderThirdPartyVpn)
    return mojom::VPNType::kThirdPartyVPN;
  if (shill_vpn_type == shill::kProviderArcVpn)
    return mojom::VPNType::kArcVPN;
  NOTREACHED() << "Unsupported shill VPN type: " << shill_vpn_type;
  return mojom::VPNType::kOpenVPN;
}

base::Optional<mojom::DeviceStateType> GetMojoDeviceStateType(
    NetworkStateHandler::TechnologyState technology_state) {
  switch (technology_state) {
    case NetworkStateHandler::TECHNOLOGY_UNINITIALIZED:
      return mojom::DeviceStateType::kUninitialized;
    case NetworkStateHandler::TECHNOLOGY_AVAILABLE:
      return mojom::DeviceStateType::kDisabled;
    case NetworkStateHandler::TECHNOLOGY_ENABLING:
      return mojom::DeviceStateType::kEnabling;
    case NetworkStateHandler::TECHNOLOGY_ENABLED:
      return mojom::DeviceStateType::kEnabled;
    case NetworkStateHandler::TECHNOLOGY_PROHIBITED:
      return mojom::DeviceStateType::kProhibited;
    case NetworkStateHandler::TECHNOLOGY_UNAVAILABLE:
      return base::nullopt;
  }
  NOTREACHED();
  return base::nullopt;
}

mojom::NetworkStatePropertiesPtr NetworkStateToMojo(
    NetworkStateHandler* network_state_handler,
    const NetworkState* network) {
  mojom::NetworkType type = ShillTypeToMojo(network->type());
  if (type == mojom::NetworkType::kAll) {
    NET_LOG(ERROR) << "Unexpected network type: " << network->type()
                   << " GUID: " << network->guid();
    return nullptr;
  }

  auto result = mojom::NetworkStateProperties::New();
  result->type = type;
  result->connectable = network->connectable();
  result->connect_requested = network->connect_requested();
  result->connection_state = GetMojoConnectionStateType(network);
  if (!network->last_error().empty())
    result->error_state = network->last_error();
  result->guid = network->guid();
  result->name = network->name();
  result->priority = network->priority();
  result->source = mojom::ONCSource(network->onc_source());

  switch (type) {
    case mojom::NetworkType::kCellular: {
      auto cellular = mojom::CellularStateProperties::New();
      cellular->activation_state = network->GetMojoActivationState();
      cellular->network_technology = ShillToONC(network->network_technology(),
                                                onc::kNetworkTechnologyTable);
      cellular->roaming = network->IndicateRoaming();
      cellular->signal_strength = network->signal_strength();
      const DeviceState* device =
          network_state_handler->GetDeviceState(network->device_path());
      if (device) {
        cellular->scanning = device->scanning();
        cellular->sim_absent = device->IsSimAbsent();
      }
      result->cellular = std::move(cellular);
      break;
    }
    case mojom::NetworkType::kEthernet: {
      auto ethernet = mojom::EthernetStateProperties::New();
      ethernet->authentication = network->type() == shill::kTypeEthernetEap
                                     ? mojom::AuthenticationType::k8021x
                                     : mojom::AuthenticationType::kNone;
      result->ethernet = std::move(ethernet);
      break;
    }
    case mojom::NetworkType::kTether: {
      auto tether = mojom::TetherStateProperties::New();
      tether->battery_percentage = network->battery_percentage();
      tether->carrier = network->tether_carrier();
      tether->has_connected_to_host = network->tether_has_connected_to_host();
      tether->signal_strength = network->signal_strength();
      result->tether = std::move(tether);
      break;
    }
    case mojom::NetworkType::kVPN: {
      auto vpn = mojom::VPNStateProperties::New();
      const NetworkState::VpnProviderInfo* vpn_provider =
          network->vpn_provider();
      if (vpn_provider) {
        vpn->type = ShillVpnTypeToMojo(vpn_provider->type);
        if (vpn->type == mojom::VPNType::kThirdPartyVPN) {
          auto third_party_vpn = mojom::ThirdPartyVPNProperties::New();
          third_party_vpn->extension_id = vpn_provider->id;
          // TODO(stevenjb): Set the provider name in network state.
          // third_party_vpn->provider_name = vpn_provider->name;
          vpn->third_party_vpn = std::move(third_party_vpn);
        }
        result->vpn = std::move(vpn);
      }
      break;
    }
    case mojom::NetworkType::kWiFi: {
      auto wifi = mojom::WiFiStateProperties::New();
      wifi->bssid = network->bssid();
      wifi->frequency = network->frequency();
      wifi->hex_ssid = network->GetHexSsid();
      wifi->security = network->GetMojoSecurity();
      wifi->signal_strength = network->signal_strength();
      wifi->ssid = network->name();
      result->wifi = std::move(wifi);
      break;
    }
    case mojom::NetworkType::kWiMAX: {
      auto wimax = mojom::WiMAXStateProperties::New();
      wimax->signal_strength = network->signal_strength();
      result->wimax = std::move(wimax);
      break;
    }
    case mojom::NetworkType::kAll:
    case mojom::NetworkType::kWireless:
      NOTREACHED() << "NetworkStateProperties can not be of type: " << type;
      break;
  }
  return result;
}

mojom::DeviceStatePropertiesPtr DeviceStateToMojo(
    NetworkStateHandler* network_state_handler,
    const DeviceState* device) {
  mojom::NetworkType type = ShillTypeToMojo(device->type());
  if (type == mojom::NetworkType::kAll) {
    NET_LOG(ERROR) << "Unexpected device type: " << device->type()
                   << " path: " << device->path();
    return nullptr;
  }

  base::Optional<mojom::DeviceStateType> state =
      GetMojoDeviceStateType(network_state_handler->GetTechnologyState(
          NetworkTypePattern::Primitive(device->type())));
  if (!state) {
    NET_LOG(ERROR) << "Device state unavailable";
    return nullptr;
  }
  auto result = mojom::DeviceStateProperties::New();
  result->type = type;
  result->scanning = device->scanning();
  result->state = *state;
  result->managed_network_available =
      !device->available_managed_network_path().empty();

  if (device->sim_present()) {
    result->sim_present = true;
    auto sim_lock_status = mojom::SIMLockStatus::New();
    sim_lock_status->lock_type = device->sim_lock_type();
    sim_lock_status->lock_enabled = device->sim_lock_enabled();
    sim_lock_status->retries_left = device->sim_retries_left();
    result->sim_lock_status = std::move(sim_lock_status);
  }
  return result;
}

}  // namespace

CrosNetworkConfig::CrosNetworkConfig(NetworkStateHandler* network_state_handler)
    : network_state_handler_(network_state_handler) {
  CHECK(network_state_handler);
}

CrosNetworkConfig::~CrosNetworkConfig() {
  if (network_state_handler_->HasObserver(this))
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CrosNetworkConfig::BindRequest(mojom::CrosNetworkConfigRequest request) {
  NET_LOG(EVENT) << "CrosNetworkConfig::BindRequest()";
  bindings_.AddBinding(this, std::move(request));
}

void CrosNetworkConfig::AddObserver(
    mojom::CrosNetworkConfigObserverPtr observer) {
  if (!network_state_handler_->HasObserver(this))
    network_state_handler_->AddObserver(this, FROM_HERE);
  observers_.AddPtr(std::move(observer));
}

void CrosNetworkConfig::GetNetworkState(const std::string& guid,
                                        GetNetworkStateCallback callback) {
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (!network) {
    NET_LOG(ERROR) << "Network not found: " << guid;
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(NetworkStateToMojo(network_state_handler_, network));
}

void CrosNetworkConfig::GetNetworkStateList(
    mojom::NetworkFilterPtr filter,
    GetNetworkStateListCallback callback) {
  NetworkStateHandler::NetworkStateList networks;
  NetworkTypePattern pattern = MojoTypeToPattern(filter->network_type);
  switch (filter->filter) {
    case mojom::FilterType::kActive:
      network_state_handler_->GetActiveNetworkListByType(pattern, &networks);
      if (filter->limit > 0 &&
          static_cast<int>(networks.size()) > filter->limit)
        networks.resize(filter->limit);
      break;
    case mojom::FilterType::kVisible:
      network_state_handler_->GetNetworkListByType(
          pattern, /*configured_only=*/false, /*visible_only=*/true,
          filter->limit, &networks);
      break;
    case mojom::FilterType::kConfigured:
      network_state_handler_->GetNetworkListByType(
          pattern, /*configured_only=*/true, /*visible_only=*/false,
          filter->limit, &networks);
      break;
    case mojom::FilterType::kAll:
      network_state_handler_->GetNetworkListByType(
          pattern, /*configured_only=*/false, /*visible_only=*/false,
          filter->limit, &networks);
      break;
  }
  std::vector<mojom::NetworkStatePropertiesPtr> result;
  for (const NetworkState* network : networks) {
    mojom::NetworkStatePropertiesPtr mojo_network =
        NetworkStateToMojo(network_state_handler_, network);
    if (mojo_network)
      result.emplace_back(std::move(mojo_network));
  }
  std::move(callback).Run(std::move(result));
}

void CrosNetworkConfig::GetDeviceStateList(
    GetDeviceStateListCallback callback) {
  NetworkStateHandler::DeviceStateList devices;
  network_state_handler_->GetDeviceList(&devices);
  std::vector<mojom::DeviceStatePropertiesPtr> result;
  for (const DeviceState* device : devices) {
    mojom::DeviceStatePropertiesPtr mojo_device =
        DeviceStateToMojo(network_state_handler_, device);
    if (mojo_device)
      result.emplace_back(std::move(mojo_device));
  }
  std::move(callback).Run(std::move(result));
}

// NetworkStateHandlerObserver
void CrosNetworkConfig::NetworkListChanged() {
  observers_.ForAllPtrs([](mojom::CrosNetworkConfigObserver* observer) {
    observer->OnNetworkStateListChanged();
  });
}

void CrosNetworkConfig::DeviceListChanged() {
  observers_.ForAllPtrs([](mojom::CrosNetworkConfigObserver* observer) {
    observer->OnDeviceStateListChanged();
  });
}

void CrosNetworkConfig::ActiveNetworksChanged(
    const std::vector<const NetworkState*>& active_networks) {
  std::vector<mojom::NetworkStatePropertiesPtr> result;
  for (const NetworkState* network : active_networks) {
    mojom::NetworkStatePropertiesPtr mojo_network =
        NetworkStateToMojo(network_state_handler_, network);
    if (mojo_network)
      result.emplace_back(std::move(mojo_network));
  }
  observers_.ForAllPtrs([&result](mojom::CrosNetworkConfigObserver* observer) {
    std::vector<mojom::NetworkStatePropertiesPtr> result_copy;
    result_copy.reserve(result.size());
    for (const auto& network : result)
      result_copy.push_back(network.Clone());
    observer->OnActiveNetworksChanged(std::move(result_copy));
  });
}

void CrosNetworkConfig::OnShuttingDown() {
  if (network_state_handler_->HasObserver(this))
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  network_state_handler_ = nullptr;
}

}  // namespace network_config
}  // namespace chromeos
