// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/active_network_icon.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkState;
using chromeos::NetworkTypePattern;

namespace ash {

namespace {

bool IsTrayIcon(network_icon::IconType icon_type) {
  return icon_type == network_icon::ICON_TYPE_TRAY_REGULAR ||
         icon_type == network_icon::ICON_TYPE_TRAY_OOBE;
}

}  // namespace

ActiveNetworkIcon::ActiveNetworkIcon() {
  // NetworkHandler may not be initialized in tests.
  if (!chromeos::NetworkHandler::IsInitialized())
    return;
  network_state_handler_ =
      chromeos::NetworkHandler::Get()->network_state_handler();
  DCHECK(network_state_handler_);
  network_state_handler_->AddObserver(this, FROM_HERE);
  UpdateActiveNetworks();
}

ActiveNetworkIcon::~ActiveNetworkIcon() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
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

gfx::ImageSkia ActiveNetworkIcon::GetDefaultImage(
    network_icon::IconType icon_type,
    bool* animating) {
  if (!default_network_)
    return GetDefaultImageForNoNetwork(icon_type, animating);

  // Don't show connected Ethernet in the tray unless a VPN is present.
  if (default_network_->Matches(NetworkTypePattern::Ethernet()) &&
      IsTrayIcon(icon_type) && !active_vpn_) {
    if (animating)
      *animating = false;
    return gfx::ImageSkia();
  }

  // Connected network with a connecting VPN.
  if (default_network_->IsConnectedState() && active_vpn_ &&
      active_vpn_->IsConnectingState()) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectedNetworkWithConnectingVpnImage(
        default_network_, icon_type);
  }

  // Default behavior: connected or connecting network, possibly with VPN badge.
  bool show_vpn_badge = !!active_vpn_;
  return network_icon::GetImageForNonVirtualNetwork(default_network_, icon_type,
                                                    show_vpn_badge, animating);
}

gfx::ImageSkia ActiveNetworkIcon::GetDefaultImageForNoNetwork(
    network_icon::IconType icon_type,
    bool* animating) {
  // If cellular is initializing or scanning, show a Cellular connecting image.
  if (cellular_uninitialized_msg_ != 0) {
    if (animating)
      *animating = true;
    return network_icon::GetConnectingImageForNetworkType(shill::kTypeCellular,
                                                          icon_type);
  }
  // Otherwise show a WiFi icon.
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

void ActiveNetworkIcon::DeviceListChanged() {
  UpdateActiveNetworks();
}

void ActiveNetworkIcon::ActiveNetworksChanged(
    const std::vector<const NetworkState*>& active_networks) {
  active_vpn_ = nullptr;

  const NetworkState* connected_network = nullptr;
  const NetworkState* connecting_network = nullptr;
  for (const NetworkState* network : active_networks) {
    if (network->Matches(NetworkTypePattern::VPN())) {
      if (!active_vpn_)
        active_vpn_ = network;
      continue;
    }
    if (network->IsConnectedState()) {
      if (!connected_network)
        connected_network = network;
      continue;
    }
    if (network->Matches(NetworkTypePattern::Wireless()) &&
        network->IsActive()) {
      if (!connecting_network)
        connecting_network = network;
    }
  }

  if (connecting_network &&
      (!connected_network || connecting_network->IsReconnecting() ||
       connecting_network->connect_requested())) {
    // If we are connecting to a network, and there is either no connected
    // network, or the connection was user requested, or shill triggered a
    // reconnection, use the connecting network.
    default_network_ = connecting_network;
  } else if (connected_network) {
    default_network_ = connected_network;
  } else {
    default_network_ = nullptr;
  }

  if (!default_network_) {
    // If there is no active network and cellular is initializing, we want to
    // provide a connecting cellular icon and appropriate message.
    cellular_uninitialized_msg_ = network_icon::GetCellularUninitializedMsg();
  } else {
    cellular_uninitialized_msg_ = 0;
  }
}

void ActiveNetworkIcon::OnShuttingDown() {
  network_state_handler_ = nullptr;
}

}  // namespace ash
