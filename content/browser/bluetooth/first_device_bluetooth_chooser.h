// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_FIRST_DEVICE_BLUETOOTH_CHOOSER_H_
#define CONTENT_BROWSER_BLUETOOTH_FIRST_DEVICE_BLUETOOTH_CHOOSER_H_

#include "base/macros.h"
#include "content/public/browser/bluetooth_chooser.h"

namespace content {

// Implements a Bluetooth chooser that, instead of showing a dialog, selects the
// first added device, or cancels if no device is added before discovery stops.
// This is used as a default chooser implementation for platforms without a full
// UI.
class FirstDeviceBluetoothChooser : public BluetoothChooser {
 public:
  // See the BluetoothChooser::EventHandler comments for how |event_handler| is
  // used.
  explicit FirstDeviceBluetoothChooser(const EventHandler& event_handler);
  ~FirstDeviceBluetoothChooser() override;

  // BluetoothChooser:
  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddDevice(const std::string& device_id,
                 const base::string16& device_name) override;
  void RemoveDevice(const std::string& device_id) override {}

 private:
  EventHandler event_handler_;

  DISALLOW_COPY_AND_ASSIGN(FirstDeviceBluetoothChooser);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_FIRST_DEVICE_BLUETOOTH_CHOOSER_H_
