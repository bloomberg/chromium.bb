// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_DESKTOP_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_DESKTOP_H_

#include "base/macros.h"
#include "content/public/browser/bluetooth_chooser.h"

class BluetoothChooserBubbleDelegate;

// Represents a Bluetooth chooser to ask the user to select a Bluetooth
// device from a list of options. This implementation is for desktop.
// BluetoothChooserAndroid implements the mobile part.
class BluetoothChooserDesktop : public content::BluetoothChooser {
 public:
  explicit BluetoothChooserDesktop(
      const content::BluetoothChooser::EventHandler& event_handler);
  ~BluetoothChooserDesktop() override;

  // BluetoothChooser:
  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddDevice(const std::string& device_id,
                 const base::string16& device_name) override;
  void RemoveDevice(const std::string& device_id) override;

  void set_bluetooth_chooser_bubble_delegate(
      BluetoothChooserBubbleDelegate* bluetooth_chooser_bubble_delegate) {
    bluetooth_chooser_bubble_delegate_ = bluetooth_chooser_bubble_delegate;
  }

  // Use this function to call event_handler_.
  void CallEventHandler(content::BluetoothChooser::Event event,
                        const std::string& device_id);

 private:
  content::BluetoothChooser::EventHandler event_handler_;
  BluetoothChooserBubbleDelegate* bluetooth_chooser_bubble_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothChooserDesktop);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_CHOOSER_DESKTOP_H_
