// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_button.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {

namespace {

const NetworkState* GetCurrentConnectedNetwork() {
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();
  const NetworkState* connected_network =
      state_handler->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());

  if (!connected_network)
    return nullptr;

  // It is possible that a device type has been disabled but a network
  // corresponding to that device has not yet been updated. If this is the case,
  // that network should not be considered connected in this UI surface.
  if (!state_handler->IsTechnologyEnabled(
          NetworkTypePattern::Primitive(connected_network->type()))) {
    return nullptr;
  }

  return connected_network;
}

bool ShouldToggleBeOn() {
  // The toggle should always be on if Wi-Fi is enabled.
  if (NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
          NetworkTypePattern::WiFi())) {
    return true;
  }

  // Otherwise, the toggle should be on if there is a connected network.
  return GetCurrentConnectedNetwork() != nullptr;
}

const NetworkState* GetCurrentNetwork() {
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();
  const NetworkState* connected_network = GetCurrentConnectedNetwork();
  const NetworkState* connecting_network =
      state_handler->ConnectingNetworkByType(NetworkTypePattern::Wireless());

  // If connecting to a network, and there is either no connected network or
  // the connection was user requested, use the connecting network.
  if (connecting_network &&
      (!connected_network || connecting_network->connect_requested())) {
    return connecting_network;
  }

  if (connected_network)
    return connected_network;

  // If no connecting network, check if we are activating a network.
  const NetworkState* mobile_network =
      state_handler->FirstNetworkByType(NetworkTypePattern::Mobile());
  if (mobile_network && (mobile_network->activation_state() ==
                         shill::kActivationStateActivating)) {
    return mobile_network;
  }

  return nullptr;
}

base::string16 GetSubLabelForConnectedNetwork(const NetworkState* network) {
  DCHECK(network && network->IsConnectedState());

  if (NetworkTypePattern::Cellular().MatchesType(network->type())) {
    if (network->network_technology() == shill::kNetworkTechnology1Xrtt) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_ONE_X);
    }
    if (network->network_technology() == shill::kNetworkTechnologyGsm) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_GSM);
    }
    if (network->network_technology() == shill::kNetworkTechnologyGprs) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_GPRS);
    }
    if (network->network_technology() == shill::kNetworkTechnologyEdge) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_EDGE);
    }
    if (network->network_technology() == shill::kNetworkTechnologyEvdo ||
        network->network_technology() == shill::kNetworkTechnologyUmts) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_THREE_G);
    }
    if (network->network_technology() == shill::kNetworkTechnologyHspa) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_HSPA);
    }
    if (network->network_technology() == shill::kNetworkTechnologyHspaPlus) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_HSPA_PLUS);
    }
    if (network->network_technology() == shill::kNetworkTechnologyLte) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_LTE);
    }
    if (network->network_technology() == shill::kNetworkTechnologyLteAdvanced) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_CELLULAR_TYPE_LTE_PLUS);
    }

    // All connectivity types exposed by Shill should be covered above. However,
    // as a fail-safe, return the default "Connected" string here to protect
    // against Shill providing an unexpected value.
    NOTREACHED();
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
  }

  switch (network_icon::GetSignalStrengthForNetwork(network)) {
    case network_icon::SignalStrength::WEAK:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_WEAK_SUBLABEL);
    case network_icon::SignalStrength::MEDIUM:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_MEDIUM_SUBLABEL);
    case network_icon::SignalStrength::STRONG:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG_SUBLABEL);
    default:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
  }
}

}  // namespace

NetworkFeaturePodButton::NetworkFeaturePodButton(
    FeaturePodControllerBase* controller)
    : FeaturePodButton(controller) {
  // NetworkHandler can be uninitialized in unit tests.
  if (!NetworkHandler::IsInitialized())
    return;
  network_state_observer_ = std::make_unique<TrayNetworkStateObserver>(this);
  ShowDetailedViewArrow();
  Update();
}

NetworkFeaturePodButton::~NetworkFeaturePodButton() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkFeaturePodButton::NetworkIconChanged() {
  Update();
}

void NetworkFeaturePodButton::NetworkStateChanged(bool notify_a11y) {
  Update();
}

void NetworkFeaturePodButton::Update() {
  bool animating = false;
  gfx::ImageSkia image =
      Shell::Get()->system_tray_model()->active_network_icon()->GetSingleImage(
          network_icon::ICON_TYPE_DEFAULT_VIEW, &animating);
  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);

  SetToggled(ShouldToggleBeOn());
  icon_button()->SetImage(views::Button::STATE_NORMAL, image);

  const NetworkState* network = GetCurrentNetwork();
  if (!network) {
    SetLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_LABEL));
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL));
    SetTooltipState(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_TOOLTIP));
    return;
  }

  base::string16 network_name =
      network->Matches(NetworkTypePattern::Ethernet())
          ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET)
          : base::UTF8ToUTF16(network->name());

  SetLabel(network_name);

  if (network->IsConnectingState()) {
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING_SUBLABEL));
    SetTooltipState(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING_TOOLTIP, network_name));
    return;
  }

  if (network->IsConnectedState()) {
    SetSubLabel(GetSubLabelForConnectedNetwork(network));
    SetTooltipState(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED_TOOLTIP, network_name));
    return;
  }

  if (network->activation_state() == shill::kActivationStateActivating) {
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING_SUBLABEL));
    SetTooltipState(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING_TOOLTIP, network_name));
    return;
  }
}

void NetworkFeaturePodButton::SetTooltipState(
    const base::string16& tooltip_state) {
  SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_TOGGLE_TOOLTIP, tooltip_state));
  SetLabelTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS_TOOLTIP, tooltip_state));
}

}  // namespace ash
