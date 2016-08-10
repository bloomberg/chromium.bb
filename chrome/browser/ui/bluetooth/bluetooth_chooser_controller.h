// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_CONTROLLER_H_

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "content/public/browser/bluetooth_chooser.h"

// BluetoothChooserController is a chooser that presents a list of
// Bluetooth device names, which come from |bluetooth_chooser_desktop_|.
// It can be used by WebBluetooth API to get the user's permission to
// access a Bluetooth device. It is owned by ChooserBubbleDelegate.
class BluetoothChooserController : public ChooserController {
 public:
  explicit BluetoothChooserController(
      content::RenderFrameHost* owner,
      const content::BluetoothChooser::EventHandler& event_handler);
  ~BluetoothChooserController() override;

  // ChooserController:
  base::string16 GetNoOptionsText() const override;
  base::string16 GetOkButtonLabel() const override;
  size_t NumOptions() const override;
  base::string16 GetOption(size_t index) const override;
  void RefreshOptions() override;
  base::string16 GetStatus() const override;
  void Select(size_t index) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  // Update the state of the Bluetooth adapter.
  void OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence presence);

  // Update the Bluetooth discovery state and let the user know whether
  // discovery is happening.
  void OnDiscoveryStateChanged(content::BluetoothChooser::DiscoveryState state);

  // Shows a new device in the chooser or updates its information.
  // TODO(ortuno): Update device's name if necessary.
  // https://crbug.com/634366
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         const int8_t* rssi);

  // Tells the chooser that a device is no longer available.
  void RemoveDevice(const std::string& device_id);

  // Called when |event_handler_| is no longer valid and should not be used
  // any more.
  void ResetEventHandler();

 private:
  // Clears |device_names_and_ids_| and |device_name_map_|. Called when
  // Bluetooth adapter is turned on or off, or when re-scan happens.
  void ClearAllDevices();

  std::vector<std::string> device_ids_;
  std::unordered_map<std::string, base::string16> device_id_to_name_map_;
  // Maps from device name to number of devices.
  std::unordered_map<base::string16, int> device_name_map_;

  content::BluetoothChooser::EventHandler event_handler_;
  base::string16 no_devices_text_;
  base::string16 status_text_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothChooserController);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_CONTROLLER_H_
