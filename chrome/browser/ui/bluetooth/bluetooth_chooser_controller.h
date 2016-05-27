// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_CONTROLLER_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "components/chooser_controller/chooser_controller.h"
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
  size_t NumOptions() const override;
  const base::string16& GetOption(size_t index) const override;
  void Select(size_t index) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  // Shows a new device in the chooser.
  void AddDevice(const std::string& device_id,
                 const base::string16& device_name);

  // Tells the chooser that a device is no longer available.
  void RemoveDevice(const std::string& device_id);

 private:
  // Each pair is a (device name, device id).
  std::vector<std::pair<base::string16, std::string>> device_names_and_ids_;
  content::BluetoothChooser::EventHandler event_handler_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothChooserController);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_CONTROLLER_H_
