// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_notification_presenter.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

FakeNotificationPresenter::FakeNotificationPresenter()
    : NotificationPresenter(),
      potential_hotspot_state_(
          PotentialHotspotNotificationState::NO_HOTSPOT_NOTIFICATION_SHOWN),
      is_setup_required_notification_shown_(false),
      is_connection_failed_notification_shown_(false) {}

FakeNotificationPresenter::~FakeNotificationPresenter() {}

cryptauth::RemoteDevice&
FakeNotificationPresenter::GetPotentialHotspotRemoteDevice() {
  EXPECT_EQ(PotentialHotspotNotificationState::SINGLE_HOTSPOT_NEARBY_SHOWN,
            potential_hotspot_state_);
  return potential_hotspot_remote_device_;
}

void FakeNotificationPresenter::NotifyPotentialHotspotNearby(
    const cryptauth::RemoteDevice& remote_device,
    int signal_strength) {
  potential_hotspot_state_ =
      PotentialHotspotNotificationState::SINGLE_HOTSPOT_NEARBY_SHOWN;
  potential_hotspot_remote_device_ = remote_device;
}

void FakeNotificationPresenter::NotifyMultiplePotentialHotspotsNearby() {
  potential_hotspot_state_ =
      PotentialHotspotNotificationState::MULTIPLE_HOTSPOTS_NEARBY_SHOWN;
}

void FakeNotificationPresenter::RemovePotentialHotspotNotification() {
  potential_hotspot_state_ =
      PotentialHotspotNotificationState::NO_HOTSPOT_NOTIFICATION_SHOWN;
}

void FakeNotificationPresenter::NotifySetupRequired(
    const std::string& device_name) {
  is_setup_required_notification_shown_ = true;
}

void FakeNotificationPresenter::RemoveSetupRequiredNotification() {
  is_setup_required_notification_shown_ = false;
}

void FakeNotificationPresenter::NotifyConnectionToHostFailed() {
  is_connection_failed_notification_shown_ = true;
}

void FakeNotificationPresenter::RemoveConnectionToHostFailedNotification() {
  is_connection_failed_notification_shown_ = false;
}

void FakeNotificationPresenter::NotifyEnableBluetooth() {
  is_enable_bluetooth_notification_shown_ = true;
}

void FakeNotificationPresenter::RemoveEnableBluetoothNotification() {
  is_enable_bluetooth_notification_shown_ = false;
}

}  // namespace tether

}  // namespace chromeos
