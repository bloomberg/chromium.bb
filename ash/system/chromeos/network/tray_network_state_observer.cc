// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_network_state_observer.h"

#include "ash/system/chromeos/network/network_detailed_view.h"
#include "ash/system/chromeos/network/tray_network.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace internal {

TrayNetworkStateObserver::TrayNetworkStateObserver(TrayNetwork* tray)
    : tray_(tray),
      wifi_state_(WIFI_UNKNOWN) {
  chromeos::NetworkStateHandler::Get()->AddObserver(this);
}

TrayNetworkStateObserver::~TrayNetworkStateObserver() {
  chromeos::NetworkStateHandler::Get()->RemoveObserver(this);
}

void TrayNetworkStateObserver::NetworkManagerChanged() {
  tray::NetworkDetailedView* detailed = tray_->detailed();
  bool wifi_enabled = chromeos::NetworkStateHandler::Get()->
      TechnologyEnabled(flimflam::kTypeWifi);
  WifiState wifi_state = wifi_enabled ? WIFI_ENABLED : WIFI_DISABLED;
  if ((wifi_state_ != WIFI_UNKNOWN && wifi_state_ != wifi_state) &&
      (!detailed ||
       detailed->GetViewType() == tray::NetworkDetailedView::WIFI_VIEW)) {
    tray_->set_request_wifi_view(true);
    tray_->PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  }
  wifi_state_ = wifi_state;
  if (detailed)
    detailed->ManagerChanged();
}

void TrayNetworkStateObserver::NetworkListChanged() {
  tray::NetworkDetailedView* detailed = tray_->detailed();
  if (detailed)
    detailed->NetworkListChanged();
  tray_->TrayNetworkUpdated();
}

void TrayNetworkStateObserver::DeviceListChanged() {
  tray::NetworkDetailedView* detailed = tray_->detailed();
  if (detailed)
    detailed->ManagerChanged();
  tray_->TrayNetworkUpdated();
}

void TrayNetworkStateObserver::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  tray_->TrayNetworkUpdated();
}

void TrayNetworkStateObserver::NetworkPropertiesUpdated(
    const chromeos::NetworkState* network) {
  tray_->NetworkServiceChanged(network);
  tray::NetworkDetailedView* detailed = tray_->detailed();
  if (detailed)
    detailed->NetworkServiceChanged(network);
}

}  // namespace ash
}  // namespace internal
