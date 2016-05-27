// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"

#include "base/logging.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_controller.h"

BluetoothChooserDesktop::BluetoothChooserDesktop(
    BluetoothChooserController* bluetooth_chooser_controller)
    : bluetooth_chooser_controller_(bluetooth_chooser_controller) {
  DCHECK(bluetooth_chooser_controller_);
}

BluetoothChooserDesktop::~BluetoothChooserDesktop() {}

void BluetoothChooserDesktop::SetAdapterPresence(AdapterPresence presence) {}

void BluetoothChooserDesktop::ShowDiscoveryState(DiscoveryState state) {}

void BluetoothChooserDesktop::AddDevice(const std::string& device_id,
                                        const base::string16& device_name) {
  bluetooth_chooser_controller_->AddDevice(device_id, device_name);
}

void BluetoothChooserDesktop::RemoveDevice(const std::string& device_id) {
  bluetooth_chooser_controller_->RemoveDevice(device_id);
}
