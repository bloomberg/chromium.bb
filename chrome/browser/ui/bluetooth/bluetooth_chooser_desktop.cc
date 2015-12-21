// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_bubble_delegate.h"

BluetoothChooserDesktop::BluetoothChooserDesktop(
    const content::BluetoothChooser::EventHandler& event_handler)
    : event_handler_(event_handler),
      bluetooth_chooser_bubble_delegate_(nullptr) {}

BluetoothChooserDesktop::~BluetoothChooserDesktop() {
  if (bluetooth_chooser_bubble_delegate_)
    bluetooth_chooser_bubble_delegate_->set_bluetooth_chooser(nullptr);
}

void BluetoothChooserDesktop::SetAdapterPresence(AdapterPresence presence) {}

void BluetoothChooserDesktop::ShowDiscoveryState(DiscoveryState state) {}

void BluetoothChooserDesktop::AddDevice(const std::string& device_id,
                                        const base::string16& device_name) {
  if (bluetooth_chooser_bubble_delegate_)
    bluetooth_chooser_bubble_delegate_->AddDevice(device_id, device_name);
}

void BluetoothChooserDesktop::RemoveDevice(const std::string& device_id) {
  if (bluetooth_chooser_bubble_delegate_)
    bluetooth_chooser_bubble_delegate_->RemoveDevice(device_id);
}

void BluetoothChooserDesktop::CallEventHandler(
    content::BluetoothChooser::Event event,
    const std::string& device_id) {
  event_handler_.Run(event, device_id);
}
