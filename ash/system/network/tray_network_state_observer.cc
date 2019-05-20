// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network_state_observer.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::NetworkType;

namespace {

const int kUpdateFrequencyMs = 1000;

}  // namespace

namespace ash {

TrayNetworkStateObserver::TrayNetworkStateObserver(
    service_manager::Connector* connector)
    : update_frequency_(kUpdateFrequencyMs) {
  if (ui::ScopedAnimationDurationScaleMode::duration_scale_mode() !=
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION) {
    update_frequency_ = 0;  // Send updates immediately for tests.
  }
  if (connector)  // May be null in tests.
    BindCrosNetworkConfig(connector);
}

TrayNetworkStateObserver::~TrayNetworkStateObserver() = default;

void TrayNetworkStateObserver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TrayNetworkStateObserver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TrayNetworkStateObserver::OnActiveNetworksChanged(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>) {
  SendActiveNetworkStateChanged();
}

void TrayNetworkStateObserver::OnNetworkStateListChanged() {
  NotifyNetworkListChanged();
}

void TrayNetworkStateObserver::OnDeviceStateListChanged() {
  // This observer is triggered when any device state changes. Depending on the
  // actual state changes, an immediate or delated update may be triggered.
  // See OnGetDeviceStateList for details.
  UpdateDeviceEnabledStates();
}

void TrayNetworkStateObserver::BindCrosNetworkConfig(
    service_manager::Connector* connector) {
  // Ensure bindings are reset in case this is called after a failure.
  cros_network_config_observer_binding_.Close();
  cros_network_config_ptr_.reset();

  connector->BindInterface(chromeos::network_config::mojom::kServiceName,
                           &cros_network_config_ptr_);
  chromeos::network_config::mojom::CrosNetworkConfigObserverPtr observer_ptr;
  cros_network_config_observer_binding_.Bind(mojo::MakeRequest(&observer_ptr));
  cros_network_config_ptr_->AddObserver(std::move(observer_ptr));
  UpdateDeviceEnabledStates();

  // If the connection is lost (e.g. due to a crash), attempt to rebind it.
  cros_network_config_ptr_.set_connection_error_handler(
      base::BindOnce(&TrayNetworkStateObserver::BindCrosNetworkConfig,
                     base::Unretained(this), connector));
}

void TrayNetworkStateObserver::UpdateDeviceEnabledStates() {
  cros_network_config_ptr_->GetDeviceStateList(base::BindOnce(
      &TrayNetworkStateObserver::OnGetDeviceStateList, base::Unretained(this)));
}

void TrayNetworkStateObserver::OnGetDeviceStateList(
    std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
        devices) {
  bool wifi_was_enabled = wifi_enabled_;
  bool mobile_was_enabled = mobile_enabled_;
  wifi_enabled_ = false;
  mobile_enabled_ = false;
  for (auto& device : devices) {
    NetworkType type = device->type;
    if (type == NetworkType::kWiFi &&
        device->state == DeviceStateType::kEnabled) {
      wifi_enabled_ = true;
    }
    if ((type == NetworkType::kCellular || type == NetworkType::kTether) &&
        device->state == DeviceStateType::kEnabled) {
      mobile_enabled_ = true;
    }
  }
  if (wifi_was_enabled != wifi_enabled_ ||
      mobile_was_enabled != mobile_enabled_) {
    SendActiveNetworkStateChanged();
  }
}

void TrayNetworkStateObserver::NotifyNetworkListChanged() {
  if (timer_.IsRunning())
    return;
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(update_frequency_),
      base::BindRepeating(&TrayNetworkStateObserver::SendNetworkListChanged,
                          base::Unretained(this)));
}

void TrayNetworkStateObserver::SendActiveNetworkStateChanged() {
  for (auto& observer : observer_list_)
    observer.ActiveNetworkStateChanged();
}

void TrayNetworkStateObserver::SendNetworkListChanged() {
  for (auto& observer : observer_list_)
    observer.NetworkListChanged();
}

void TrayNetworkStateObserver::Observer::ActiveNetworkStateChanged() {}

void TrayNetworkStateObserver::Observer::NetworkListChanged() {}

}  // namespace ash
