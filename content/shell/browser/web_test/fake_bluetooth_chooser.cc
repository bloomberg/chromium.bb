// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/fake_bluetooth_chooser.h"

#include <string>
#include <utility>

#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/test/web_test_support.h"
#include "content/shell/common/web_test/fake_bluetooth_chooser.mojom.h"

namespace content {

FakeBluetoothChooser::~FakeBluetoothChooser() {
  SetTestBluetoothScanDuration(
      BluetoothTestScanDurationSetting::kImmediateTimeout);
  // TODO(https://crbug.com/719826): Send a
  // mojom::ChooserEventType::CHOOSER_CLOSED event to the client.
}

// static
std::unique_ptr<FakeBluetoothChooser> FakeBluetoothChooser::Create(
    mojom::FakeBluetoothChooserRequest request) {
  SetTestBluetoothScanDuration(BluetoothTestScanDurationSetting::kNeverTimeout);
  return std::unique_ptr<FakeBluetoothChooser>(
      new FakeBluetoothChooser(std::move(request)));
}

void FakeBluetoothChooser::SetEventHandler(const EventHandler& event_handler) {
  event_handler_ = event_handler;
}

// mojom::FakeBluetoothChooser overrides

void FakeBluetoothChooser::WaitForEvents(uint32_t num_of_events,
                                         WaitForEventsCallback callback) {
  // TODO(https://crbug.com/719826): Implement this function according to the
  // Web Bluetooth Test Scanning design document.
  // https://docs.google.com/document/d/1XFl_4ZAgO8ddM6U53A9AfUuZeWgJnlYD5wtbXqEpzeg
  NOTREACHED();
}

void FakeBluetoothChooser::SelectPeripheral(
    const std::string& peripheral_address) {
  event_handler_.Run(BluetoothChooser::Event::SELECTED, peripheral_address);
}

void FakeBluetoothChooser::Cancel() {
  // TODO(https://crbug.com/719826): Send a BluetoothChooser::CANCELLED event to
  // |event_handler_| and a mojom::ChooserEventType::CHOOSER_CLOSED to the
  // client.
  NOTREACHED();
}

void FakeBluetoothChooser::Rescan() {
  // TODO(https://crbug.com/719826): Send a BluetoothChooser::RESCAN event to
  // |event_handler_| and the appropriate mojom::ChooserEventType to describe
  // the discovery session.
  NOTREACHED();
}

// BluetoothChooser overrides

void FakeBluetoothChooser::SetAdapterPresence(AdapterPresence presence) {
  // TODO(https://crbug.com/719826): Send the appropriate
  // mojom::ChooserEventType to the client.
  NOTREACHED();
}

void FakeBluetoothChooser::ShowDiscoveryState(DiscoveryState state) {
  // TODO(https://crbug.com/719826): Send the appropriate
  // mojom::ChooserEventType to the client.
  NOTREACHED();
}

void FakeBluetoothChooser::AddOrUpdateDevice(const std::string& device_id,
                                             bool should_update_name,
                                             const base::string16& device_name,
                                             bool is_gatt_connected,
                                             bool is_paired,
                                             int signal_strength_level) {
  // TODO(https://crbug.com/719826): Send a
  // mojom::ChooserEventType::ADD_OR_UPDATE_DEVICE to the client.
  NOTREACHED();
}

// private

FakeBluetoothChooser::FakeBluetoothChooser(
    mojom::FakeBluetoothChooserRequest request)
    : binding_(this, std::move(request)) {}

}  // namespace content
