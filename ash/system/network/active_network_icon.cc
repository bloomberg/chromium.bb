// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/active_network_icon.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {

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

const NetworkState* GetConnectingOrConnected(
    const NetworkState* connecting_network,
    const NetworkState* connected_network) {
  if (connecting_network &&
      (!connected_network || connecting_network->connect_requested())) {
    // If connecting to a network, and there is either no connected network or
    // the connection was user requested, use the connecting network.
    return connecting_network;
  }
  return connected_network;
}

}  // namespace

ActiveNetworkIcon::ActiveNetworkIcon() {
  // NetworkHandler may not be initialized in tests.
  if (chromeos::NetworkHandler::IsInitialized()) {
    InitializeNetworkStateHandler(
        chromeos::NetworkHandler::Get()->network_state_handler());
  }
}

ActiveNetworkIcon::~ActiveNetworkIcon() {
  ShutdownNetworkStateHandler();
}

base::string16 ActiveNetworkIcon::GetDefaultLabel(
    network_icon::IconType icon_type) {
  if (!default_network_) {
    if (cellular_uninitialized_msg_ != 0)
      return l10n_util::GetStringUTF16(cellular_uninitialized_msg_);
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED);
  }
  return network_icon::GetLabelForNetwork(default_network_, icon_type);
}

gfx::ImageSkia ActiveNetworkIcon::GetSingleImage(
    network_icon::IconType icon_type,
    bool* animating) {
  // If no network, check for cellular initializing.
  if (!default_network_ && cellular_uninitialized_msg_ != 0) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectingImageForNetworkType(shill::kTypeCellular,
                                                          icon_type);
  }
  return GetDefaultImageImpl(default_network_, icon_type, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDualImagePrimary(
    network_icon::IconType icon_type,
    bool* animating) {
  const NetworkState* default_network = default_network_;
  if (default_network &&
      default_network->Matches(NetworkTypePattern::Cellular())) {
    if (default_network->IsConnectedState()) {
      // TODO: Show proper technology badges.
      if (animating)
        *animating = false;
      return gfx::CreateVectorIcon(kNetworkBadgeTechnologyLteIcon,
                                   GetDefaultColorForIconType(icon_type));
    }
    // If Cellular is connecting, use the active non cellular network.
    default_network = active_non_cellular_;
  }
  return GetDefaultImageImpl(default_network, icon_type, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDualImageCellular(
    network_icon::IconType icon_type,
    bool* animating) {
  if (!network_state_handler_->IsTechnologyAvailable(
          NetworkTypePattern::Cellular())) {
    if (animating)
      *animating = false;
    return gfx::ImageSkia();
  }

  if (cellular_uninitialized_msg_ != 0) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectingImageForNetworkType(shill::kTypeCellular,
                                                          icon_type);
  }

  if (!active_cellular_) {
    if (animating)
      *animating = false;
    return network_icon::GetDisconnectedImageForNetworkType(
        shill::kTypeCellular);
  }

  return network_icon::GetImageForNonVirtualNetwork(
      active_cellular_, icon_type, false /* show_vpn_badge */, animating);
}

void ActiveNetworkIcon::InitForTesting(
    chromeos::NetworkStateHandler* network_state_handler) {
  ShutdownNetworkStateHandler();
  InitializeNetworkStateHandler(network_state_handler);
}

void ActiveNetworkIcon::InitializeNetworkStateHandler(
    chromeos::NetworkStateHandler* handler) {
  DCHECK(!network_state_handler_);
  DCHECK(handler);
  network_state_handler_ = handler;
  network_state_handler_->AddObserver(this, FROM_HERE);
  UpdateActiveNetworks();
}

void ActiveNetworkIcon::ShutdownNetworkStateHandler() {
  if (!network_state_handler_)
    return;
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

gfx::ImageSkia ActiveNetworkIcon::GetDefaultImageImpl(
    const NetworkState* default_network,
    network_icon::IconType icon_type,
    bool* animating) {
  if (!default_network_)
    return GetDefaultImageForNoNetwork(icon_type, animating);
  // Don't show connected Ethernet in the tray unless a VPN is present.
  if (default_network->Matches(NetworkTypePattern::Ethernet()) &&
      IsTrayIcon(icon_type) && !active_vpn_) {
    if (animating)
      *animating = false;
    return gfx::ImageSkia();
  }

  // Connected network with a connecting VPN.
  if (default_network->IsConnectedState() && active_vpn_ &&
      active_vpn_->IsConnectingState()) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectedNetworkWithConnectingVpnImage(
        default_network, icon_type);
  }

  // Default behavior: connected or connecting network, possibly with VPN badge.
  bool show_vpn_badge = !!active_vpn_;
  return network_icon::GetImageForNonVirtualNetwork(default_network, icon_type,
                                                    show_vpn_badge, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDefaultImageForNoNetwork(
    network_icon::IconType icon_type,
    bool* animating) {
  if (animating)
    *animating = false;
  if (network_state_handler_ &&
      network_state_handler_->IsTechnologyEnabled(NetworkTypePattern::WiFi())) {
    // WiFi is enabled but disconnected, show an empty wedge.
    return network_icon::GetBasicImage(icon_type, shill::kTypeWifi,
                                       false /* connected */);
  }
  // WiFi is disabled, show a full icon with a strikethrough.
  return network_icon::GetImageForWiFiEnabledState(false /* not enabled*/,
                                                   icon_type);
}

void ActiveNetworkIcon::UpdateActiveNetworks() {
  std::vector<const NetworkState*> active_networks;
  network_state_handler_->GetActiveNetworkListByType(
      NetworkTypePattern::Default(), &active_networks);
  ActiveNetworksChanged(active_networks);
}

void ActiveNetworkIcon::SetCellularUninitializedMsg() {
  if (network_state_handler_->GetTechnologyState(
          NetworkTypePattern::Cellular()) ==
      NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
    cellular_uninitialized_msg_ = IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    uninitialized_state_time_ = base::Time::Now();
    return;
  }

  if (network_state_handler_->GetScanningByType(
          NetworkTypePattern::Cellular())) {
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

void ActiveNetworkIcon::DeviceListChanged() {
  UpdateActiveNetworks();
}

void ActiveNetworkIcon::DevicePropertiesUpdated(
    const chromeos::DeviceState* device) {
  SetCellularUninitializedMsg();
}

void ActiveNetworkIcon::ActiveNetworksChanged(
    const std::vector<const NetworkState*>& active_networks) {
  active_cellular_ = nullptr;
  active_vpn_ = nullptr;

  const NetworkState* connected_network = nullptr;
  const NetworkState* connected_non_cellular = nullptr;
  const NetworkState* connecting_network = nullptr;
  const NetworkState* connecting_non_cellular = nullptr;
  for (const NetworkState* network : active_networks) {
    if (network->Matches(NetworkTypePattern::VPN())) {
      if (!active_vpn_)
        active_vpn_ = network;
      continue;
    }
    if (network->Matches(NetworkTypePattern::Cellular())) {
      if (!active_cellular_)
        active_cellular_ = network;
    }
    if (network->IsConnectedState()) {
      if (!connected_network)
        connected_network = network;
      if (!connected_non_cellular &&
          !network->Matches(NetworkTypePattern::Cellular())) {
        connected_non_cellular = network;
      }
      continue;
    }
    // Active non connected networks are connecting.
    if (network->Matches(NetworkTypePattern::Wireless())) {
      if (!connecting_network)
        connecting_network = network;
      if (!connecting_non_cellular &&
          !network->Matches(NetworkTypePattern::Cellular())) {
        connecting_non_cellular = network;
      }
    }
  }

  default_network_ =
      GetConnectingOrConnected(connecting_network, connected_network);
  active_non_cellular_ =
      GetConnectingOrConnected(connecting_non_cellular, connected_non_cellular);

  SetCellularUninitializedMsg();
}

void ActiveNetworkIcon::OnShuttingDown() {
  network_state_handler_ = nullptr;
}

}  // namespace ash
