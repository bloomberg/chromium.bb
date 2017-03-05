// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/tray_network_state_observer.h"

#include <set>
#include <string>

#include "ash/common/system/chromeos/network/network_icon.h"
#include "ash/common/system/tray/system_tray.h"
#include "base/location.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

using chromeos::NetworkHandler;

namespace {

const int kUpdateFrequencyMs = 1000;

}  // namespace

namespace ash {

TrayNetworkStateObserver::TrayNetworkStateObserver(Delegate* delegate)
    : delegate_(delegate),
      purge_icons_(false),
      update_frequency_(kUpdateFrequencyMs) {
  if (ui::ScopedAnimationDurationScaleMode::duration_scale_mode() !=
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION) {
    update_frequency_ = 0;  // Send updates immediately for tests.
  }
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }
}

TrayNetworkStateObserver::~TrayNetworkStateObserver() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void TrayNetworkStateObserver::NetworkListChanged() {
  purge_icons_ = true;
  SignalUpdate();
}

void TrayNetworkStateObserver::DeviceListChanged() {
  SignalUpdate();
}

// Any change to the Default (primary connected) network, including Strength
// changes, should trigger a NetworkStateChanged update.
void TrayNetworkStateObserver::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  SignalUpdate();
}

// Any change to the Connection State should trigger a NetworkStateChanged
// update. This is important when both a VPN and a physical network are
// connected.
void TrayNetworkStateObserver::NetworkConnectionStateChanged(
    const chromeos::NetworkState* network) {
  SignalUpdate();
}

// This tracks Strength and other property changes for all networks. It will
// be called in addition to NetworkConnectionStateChanged for connection state
// changes.
void TrayNetworkStateObserver::NetworkPropertiesUpdated(
    const chromeos::NetworkState* network) {
  SignalUpdate();
}

void TrayNetworkStateObserver::SignalUpdate() {
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(update_frequency_),
               this, &TrayNetworkStateObserver::SendNetworkStateChanged);
}

void TrayNetworkStateObserver::SendNetworkStateChanged() {
  delegate_->NetworkStateChanged();
  if (purge_icons_) {
    network_icon::PurgeNetworkIconCache();
    purge_icons_ = false;
  }
}

}  // namespace ash
