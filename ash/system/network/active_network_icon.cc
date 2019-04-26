// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/active_network_icon.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/tray/tray_constants.h"
#include "base/stl_util.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;

namespace ash {

using network_icon::NetworkIconState;

namespace {

bool IsTrayIcon(network_icon::IconType icon_type) {
  return icon_type == network_icon::ICON_TYPE_TRAY_REGULAR ||
         icon_type == network_icon::ICON_TYPE_TRAY_OOBE;
}

SkColor GetDefaultColorForIconType(network_icon::IconType icon_type) {
  if (icon_type == network_icon::ICON_TYPE_TRAY_REGULAR)
    return kTrayIconColor;
  if (icon_type == network_icon::ICON_TYPE_TRAY_OOBE)
    return kOobeTrayIconColor;
  return kUnifiedMenuIconColor;
}

base::Optional<NetworkIconState> GetConnectingOrConnected(
    const NetworkStateProperties* connecting_network,
    const NetworkStateProperties* connected_network) {
  if (connecting_network &&
      (!connected_network || connecting_network->connect_requested)) {
    // If connecting to a network, and there is either no connected network or
    // the connection was user requested, use the connecting network.
    return base::make_optional<NetworkIconState>(connecting_network);
  }
  if (connected_network)
    return base::make_optional<NetworkIconState>(connected_network);
  return base::nullopt;
}

}  // namespace

ActiveNetworkIcon::ActiveNetworkIcon(service_manager::Connector* connector) {
  if (connector)  // May be null in tests.
    BindCrosNetworkConfig(connector);
}

ActiveNetworkIcon::~ActiveNetworkIcon() = default;

void ActiveNetworkIcon::BindCrosNetworkConfig(
    service_manager::Connector* connector) {
  // Ensure bindings are reset in case this is called after a failure.
  cros_network_config_observer_binding_.Close();
  cros_network_config_ptr_.reset();

  connector->BindInterface(chromeos::network_config::mojom::kServiceName,
                           &cros_network_config_ptr_);
  chromeos::network_config::mojom::CrosNetworkConfigObserverPtr observer_ptr;
  cros_network_config_observer_binding_.Bind(mojo::MakeRequest(&observer_ptr));
  cros_network_config_ptr_->AddObserver(std::move(observer_ptr));
  UpdateActiveNetworks();

  // If the connection is lost (e.g. due to a crash), attempt to rebind it.
  cros_network_config_ptr_.set_connection_error_handler(
      base::BindOnce(&ActiveNetworkIcon::BindCrosNetworkConfig,
                     base::Unretained(this), connector));
}

base::string16 ActiveNetworkIcon::GetDefaultLabel(
    network_icon::IconType icon_type) {
  if (!default_network_) {
    if (cellular_uninitialized_msg_ != 0)
      return l10n_util::GetStringUTF16(cellular_uninitialized_msg_);
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED);
  }
  return network_icon::GetLabelForNetwork(*default_network_, icon_type);
}

gfx::ImageSkia ActiveNetworkIcon::GetSingleImage(
    network_icon::IconType icon_type,
    bool* animating) {
  // If no network, check for cellular initializing.
  if (!default_network_ && cellular_uninitialized_msg_ != 0) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectingImageForNetworkType(
        NetworkType::kCellular, icon_type);
  }
  return GetDefaultImageImpl(default_network_, icon_type, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDualImagePrimary(
    network_icon::IconType icon_type,
    bool* animating) {
  if (default_network_ && default_network_->type == NetworkType::kCellular) {
    if (network_icon::IsConnected(*default_network_)) {
      // TODO: Show proper technology badges.
      if (animating)
        *animating = false;
      return gfx::CreateVectorIcon(kNetworkBadgeTechnologyLteIcon,
                                   GetDefaultColorForIconType(icon_type));
    }
    // If Cellular is connecting, use the active non cellular network.
    return GetDefaultImageImpl(active_non_cellular_, icon_type, animating);
  }
  return GetDefaultImageImpl(default_network_, icon_type, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDualImageCellular(
    network_icon::IconType icon_type,
    bool* animating) {
  if (!base::ContainsKey(devices_, NetworkType::kCellular)) {
    if (animating)
      *animating = false;
    return gfx::ImageSkia();
  }

  if (cellular_uninitialized_msg_ != 0) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectingImageForNetworkType(
        NetworkType::kCellular, icon_type);
  }

  if (!active_cellular_) {
    if (animating)
      *animating = false;
    return network_icon::GetDisconnectedImageForNetworkType(
        NetworkType::kCellular);
  }

  return network_icon::GetImageForNonVirtualNetwork(
      *active_cellular_, icon_type, false /* show_vpn_badge */, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDefaultImageImpl(
    const base::Optional<NetworkIconState>& default_network,
    network_icon::IconType icon_type,
    bool* animating) {
  if (!default_network) {
    VLOG(1) << __func__ << ": No network";
    return GetDefaultImageForNoNetwork(icon_type, animating);
  }
  // Don't show connected Ethernet in the tray unless a VPN is present.
  if (default_network->type == NetworkType::kEthernet &&
      IsTrayIcon(icon_type) && !active_vpn_) {
    if (animating)
      *animating = false;
    VLOG(1) << __func__ << ": Ethernet: No icon";
    return gfx::ImageSkia();
  }

  // Connected network with a connecting VPN.
  if (network_icon::IsConnected(*default_network) && active_vpn_ &&
      network_icon::IsConnecting(*active_vpn_)) {
    if (animating)
      *animating = true;
    VLOG(1) << __func__ << ": Connected with connecting VPN";
    return network_icon::GetConnectedNetworkWithConnectingVpnImage(
        *default_network, icon_type);
  }

  // Default behavior: connected or connecting network, possibly with VPN badge.
  bool show_vpn_badge = !!active_vpn_;
  VLOG(1) << __func__ << ": Network: " << default_network->name
          << " Strength: " << default_network->signal_strength;
  return network_icon::GetImageForNonVirtualNetwork(*default_network, icon_type,
                                                    show_vpn_badge, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDefaultImageForNoNetwork(
    network_icon::IconType icon_type,
    bool* animating) {
  if (animating)
    *animating = false;
  DeviceStateProperties* wifi = GetDevice(NetworkType::kWiFi);
  if (wifi && wifi->state == DeviceStateType::kEnabled) {
    // WiFi is enabled but disconnected, show an empty wedge.
    return network_icon::GetBasicImage(icon_type, NetworkType::kWiFi,
                                       false /* connected */);
  }
  // WiFi is disabled, show a full icon with a strikethrough.
  return network_icon::GetImageForWiFiEnabledState(false /* not enabled*/,
                                                   icon_type);
}

void ActiveNetworkIcon::UpdateActiveNetworks() {
  DCHECK(cros_network_config_ptr_);
  cros_network_config_ptr_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                         /*limit=*/0),
      base::BindOnce(&ActiveNetworkIcon::OnActiveNetworksChanged,
                     base::Unretained(this)));
}

void ActiveNetworkIcon::SetCellularUninitializedMsg() {
  DeviceStateProperties* cellular = GetDevice(NetworkType::kCellular);
  if (cellular && cellular->state == DeviceStateType::kUninitialized) {
    cellular_uninitialized_msg_ = IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    uninitialized_state_time_ = base::Time::Now();
    return;
  }

  if (cellular && cellular->scanning) {
    cellular_uninitialized_msg_ = IDS_ASH_STATUS_TRAY_MOBILE_SCANNING;
    uninitialized_state_time_ = base::Time::Now();
    return;
  }

  // There can be a delay between leaving the Initializing state and when
  // a Cellular device shows up, so keep showing the initializing
  // animation for a bit to avoid flashing the disconnect icon.
  const int kInitializingDelaySeconds = 1;
  base::TimeDelta dtime = base::Time::Now() - uninitialized_state_time_;
  if (dtime.InSeconds() >= kInitializingDelaySeconds)
    cellular_uninitialized_msg_ = 0;
}

// CrosNetworkConfigObserver

void ActiveNetworkIcon::OnActiveNetworksChanged(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  active_cellular_.reset();
  active_vpn_.reset();

  const NetworkStateProperties* connected_network = nullptr;
  const NetworkStateProperties* connected_non_cellular = nullptr;
  const NetworkStateProperties* connecting_network = nullptr;
  const NetworkStateProperties* connecting_non_cellular = nullptr;
  for (const auto& network_ptr : networks) {
    const NetworkStateProperties* network = network_ptr.get();
    if (network->type == NetworkType::kVPN) {
      if (!active_vpn_)
        active_vpn_ = base::make_optional<NetworkIconState>(network);
      continue;
    }
    if (network->type == NetworkType::kCellular) {
      if (!active_cellular_)
        active_cellular_ = base::make_optional<NetworkIconState>(network);
    }
    if (chromeos::network_config::StateIsConnected(network->connection_state)) {
      if (!connected_network)
        connected_network = network;
      if (!connected_non_cellular && network->type != NetworkType::kCellular) {
        connected_non_cellular = network;
      }
      continue;
    }
    // Active non connected networks are connecting.
    if (chromeos::network_config::NetworkStateMatchesType(
            network, NetworkType::kWireless)) {
      if (!connecting_network)
        connecting_network = network;
      if (!connecting_non_cellular && network->type != NetworkType::kCellular) {
        connecting_non_cellular = network;
      }
    }
  }

  VLOG_IF(2, connected_network)
      << __func__ << ": Connected network: " << connected_network->name
      << " State: " << connected_network->connection_state;
  VLOG_IF(2, connecting_network)
      << __func__ << ": Connecting network: " << connecting_network->name
      << " State: " << connecting_network->connection_state;

  default_network_ =
      GetConnectingOrConnected(connecting_network, connected_network);
  VLOG_IF(2, default_network_)
      << __func__ << ": Default network: " << default_network_->name
      << " Strength: " << default_network_->signal_strength;

  active_non_cellular_ =
      GetConnectingOrConnected(connecting_non_cellular, connected_non_cellular);

  SetCellularUninitializedMsg();
}

void ActiveNetworkIcon::OnNetworkStateListChanged() {}

void ActiveNetworkIcon::OnDeviceStateListChanged() {
  cros_network_config_ptr_->GetDeviceStateList(base::BindOnce(
      &ActiveNetworkIcon::OnGetDeviceStateList, base::Unretained(this)));
}

void ActiveNetworkIcon::OnGetDeviceStateList(
    std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
        devices) {
  devices_.clear();
  for (auto& device : devices) {
    NetworkType type = device->type;
    if (base::ContainsKey(devices_, type))
      continue;  // Ignore multiple entries with the same type.
    devices_.emplace(std::make_pair(type, std::move(device)));
  }
  UpdateActiveNetworks();
  SetCellularUninitializedMsg();
}

DeviceStateProperties* ActiveNetworkIcon::GetDevice(NetworkType type) {
  auto iter = devices_.find(type);
  if (iter == devices_.end())
    return nullptr;
  return iter->second.get();
}

}  // namespace ash
