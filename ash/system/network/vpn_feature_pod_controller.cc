// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_feature_pod_controller.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {

namespace {

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

}  // namespace

VPNFeaturePodController::VPNFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);
}

VPNFeaturePodController::~VPNFeaturePodController() {
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
}

FeaturePodButton* VPNFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuVpnIcon);
  button_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_SHORT));
  button_->SetIconAndLabelTooltips(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_TOOLTIP));
  button_->ShowDetailedViewArrow();
  button_->DisableLabelButtonFocus();
  Update();
  return button_;
}

void VPNFeaturePodController::OnIconPressed() {
  tray_controller_->ShowVPNDetailedView();
}

SystemTrayItemUmaType VPNFeaturePodController::GetUmaType() const {
  return SystemTrayItemUmaType::UMA_VPN;
}

void VPNFeaturePodController::ActiveNetworkStateChanged() {
  Update();
}

void VPNFeaturePodController::Update() {
  // NetworkHandler can be uninitialized in unit tests.
  if (!chromeos::NetworkHandler::IsInitialized())
    return;

  button_->SetVisible(IsVPNVisibleInSystemTray());
  if (!button_->GetVisible())
    return;

  button_->SetSubLabel(l10n_util::GetStringUTF16(
      IsVPNConnected() ? IDS_ASH_STATUS_TRAY_VPN_CONNECTED_SHORT
                       : IDS_ASH_STATUS_TRAY_VPN_DISCONNECTED_SHORT));
  button_->SetToggled(IsVPNEnabled() && IsVPNConnected());
}

}  // namespace ash
