// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/chrome_extension_bluetooth_chooser.h"

#include "chrome/browser/extensions/chrome_extension_chooser_dialog.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_controller.h"
#include "content/public/browser/web_contents.h"

ChromeExtensionBluetoothChooser::ChromeExtensionBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  std::unique_ptr<BluetoothChooserController> bluetooth_chooser_controller(
      new BluetoothChooserController(frame, event_handler));
  // Since ChromeExtensionBluetoothChooser object is destroyed before the
  // view object which owns |bluetooth_chooser_controller_| when the chooser
  // bubble/dialog closes, it is safe to store and use the raw pointer here.
  bluetooth_chooser_controller_ = bluetooth_chooser_controller.get();
  chooser_dialog_.reset(new ChromeExtensionChooserDialog(
      content::WebContents::FromRenderFrameHost(frame)));
  chooser_dialog_->ShowDialog(std::move(bluetooth_chooser_controller));
}

ChromeExtensionBluetoothChooser::~ChromeExtensionBluetoothChooser() {}

void ChromeExtensionBluetoothChooser::SetAdapterPresence(
    AdapterPresence presence) {}

void ChromeExtensionBluetoothChooser::ShowDiscoveryState(DiscoveryState state) {
}

void ChromeExtensionBluetoothChooser::AddDevice(
    const std::string& device_id,
    const base::string16& device_name) {
  bluetooth_chooser_controller_->AddDevice(device_id, device_name);
}

void ChromeExtensionBluetoothChooser::RemoveDevice(
    const std::string& device_id) {
  bluetooth_chooser_controller_->RemoveDevice(device_id);
}
