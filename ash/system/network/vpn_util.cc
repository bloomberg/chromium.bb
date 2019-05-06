// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_util.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/network/vpn_list.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace vpn_util {

bool IsVPNVisibleInSystemTray() {
  LoginStatus login_status = Shell::Get()->session_controller()->login_status();
  if (login_status == LoginStatus::NOT_LOGGED_IN)
    return false;

  // Show the VPN entry in the ash tray bubble if at least one third-party VPN
  // provider is installed.
  if (Shell::Get()->vpn_list()->HaveThirdPartyOrArcVPNProviders())
    return true;

  // Also show the VPN entry if at least one VPN network is configured.
  NetworkStateHandler* const handler =
      NetworkHandler::Get()->network_state_handler();
  if (handler->FirstNetworkByType(NetworkTypePattern::VPN()))
    return true;
  return false;
}

bool IsVPNEnabled() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  return handler->FirstNetworkByType(NetworkTypePattern::VPN());
}

bool IsVPNConnected() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* vpn =
      handler->FirstNetworkByType(NetworkTypePattern::VPN());
  return IsVPNEnabled() &&
         (vpn->IsConnectedState() || vpn->IsConnectingState());
}

}  // namespace vpn_util
}  // namespace ash
