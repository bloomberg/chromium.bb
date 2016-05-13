// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_BUBBLE_CONTROLLER_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_controller.h"
#include "components/bubble/bubble_reference.h"

class BluetoothChooserDesktop;

// BluetoothChooserBubbleController is a chooser that presents a list of
// Bluetooth device names, which come from |bluetooth_chooser_desktop_|.
// It can be used by WebBluetooth API to get the user's permission to
// access a Bluetooth device.
class BluetoothChooserBubbleController : public ChooserBubbleController {
 public:
  explicit BluetoothChooserBubbleController(content::RenderFrameHost* owner);
  ~BluetoothChooserBubbleController() override;

  // ChooserBubbleController:
  size_t NumOptions() const override;
  const base::string16& GetOption(size_t index) const override;
  void Select(size_t index) override;
  void Cancel() override;
  void Close() override;
  GURL GetHelpCenterUrl() const override;

  // Shows a new device in the chooser.
  void AddDevice(const std::string& device_id,
                 const base::string16& device_name);

  // Tells the chooser that a device is no longer available.
  void RemoveDevice(const std::string& device_id);

  void set_bluetooth_chooser(BluetoothChooserDesktop* bluetooth_chooser) {
    bluetooth_chooser_ = bluetooth_chooser;
  }

  void set_bubble_reference(BubbleReference bubble_reference) {
    bubble_reference_ = bubble_reference;
  }

 private:
  // Each pair is a (device name, device id).
  std::vector<std::pair<base::string16, std::string>> device_names_and_ids_;
  BluetoothChooserDesktop* bluetooth_chooser_;
  BubbleReference bubble_reference_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothChooserBubbleController);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_BUBBLE_CONTROLLER_H_
