// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Handler class for the Device Emulator page operations.
class DeviceEmulatorMessageHandler : public content::WebUIMessageHandler {
 public:
  DeviceEmulatorMessageHandler();
  ~DeviceEmulatorMessageHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Callback for the "requestBatteryInfo" message. This asynchronously
  // requests the emulator's battery percentage.
  void HandleRequestBatteryInfo(const base::ListValue* args);

  // Callback for the "updateBatteryInfo" message. This asynchronously
  // updates the emulator's battery percentage to a given percentage
  // contained in args.
  void HandleUpdateBatteryInfo(const base::ListValue* args);

  // Callback for the "updatePowerSource" message. This asynchronously
  // updates the emulator's power source based on the given parameter
  // in args.
  void HandleUpdatePowerSource(const base::ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceEmulatorMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
