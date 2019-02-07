// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_tray_icon_strategy.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon.h"
#include "base/logging.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/gfx/image/image_skia.h"

using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {

namespace {

const NetworkState* GetConnectingOrConnectedNetwork(
    NetworkTypePattern pattern) {
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();
  const NetworkState* connecting_network =
      state_handler->ConnectingNetworkByType(pattern);
  const NetworkState* connected_network =
      state_handler->ConnectedNetworkByType(pattern);
  // If we are connecting to a network, and there is either no connected
  // network, or the connection was user requested, or shill triggered a
  // reconnection, use the connecting network.
  if (connecting_network &&
      (!connected_network || connecting_network->IsReconnecting() ||
       connecting_network->connect_requested())) {
    return connecting_network;
  }
  return connected_network;
}

bool NetworkTypeEnabled(NetworkTypePattern pattern) {
  return NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
      pattern);
}

// OOBE has a white background that makes regular tray icons not visible.
network_icon::IconType GetIconType() {
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::OOBE) {
    return network_icon::ICON_TYPE_TRAY_OOBE;
  }
  return network_icon::ICON_TYPE_TRAY_REGULAR;
}

}  // namespace

gfx::ImageSkia DefaultNetworkTrayIconStrategy::GetNetworkIcon(bool* animating) {
  if (!NetworkTypeEnabled(NetworkTypePattern::WiFi()))
    return gfx::ImageSkia();

  auto icon_type = GetIconType();
  const NetworkState* network =
      GetConnectingOrConnectedNetwork(NetworkTypePattern::WiFi());
  if (network) {
    bool show_vpn_badge =
        network->IsConnectedState() &&
        NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
            NetworkTypePattern::VPN());
    return network_icon::GetImageForNonVirtualNetwork(
        network, icon_type, show_vpn_badge, animating);
  }
  *animating = false;
  return network_icon::GetDisconnectedImageForNetworkType(shill::kTypeWifi);
}

gfx::ImageSkia MobileNetworkTrayIconStrategy::GetNetworkIcon(bool* animating) {
  if (!NetworkTypeEnabled(NetworkTypePattern::Mobile()))
    return gfx::ImageSkia();

  auto icon_type = GetIconType();
  // Check if we are initializing a mobile network.
  if (network_icon::GetCellularUninitializedMsg()) {
    *animating = true;
    return network_icon::GetConnectingImageForNetworkType(shill::kTypeCellular,
                                                          icon_type);
  }

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
          NetworkTypePattern::Mobile());

  if (network && network->IsConnectingOrConnected()) {
    return network_icon::GetImageForNonVirtualNetwork(
        network, icon_type, false /* show_vpn_badge */, animating);
  }

  *animating = false;
  return network_icon::GetDisconnectedImageForNetworkType(shill::kTypeCellular);
}

gfx::ImageSkia SingleNetworkTrayIconStrategy::GetNetworkIcon(bool* animating) {
  return Shell::Get()
      ->system_tray_model()
      ->active_network_icon()
      ->GetDefaultImage(GetIconType(), animating);
}

}  // namespace tray
}  // namespace ash
