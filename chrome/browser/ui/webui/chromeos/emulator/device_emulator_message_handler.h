// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Handler class for the Device Emulator page operations.
class DeviceEmulatorMessageHandler
    : public content::WebUIMessageHandler,
      public chromeos::PowerManagerClient::Observer {
 public:
  DeviceEmulatorMessageHandler();
  ~DeviceEmulatorMessageHandler() override;


  // Callback for the "requestBatteryInfo" message. This asynchronously
  // requests the emulator's battery percentage.
  void HandleRequestBatteryInfo(const base::ListValue* args);

  // Callback for the "requestExternalPowerOptions" message. This asynchronously
  // requests the options which can be selected for the emulator's power source.
  void HandleRequestExternalPowerOptions(const base::ListValue* args);

  // Callback for the "updateBatteryPercent" message. This asynchronously
  // updates the emulator's battery percentage to a given percentage
  // contained in args.
  void HandleUpdateBatteryPercent(const base::ListValue* args);

  // Callback for the "updateExternalPower" message. This asynchronously
  // updates the emulator's power source based on the given parameter
  // in args.
  void HandleUpdateExternalPower(const base::ListValue* args);

  // Adds |this| as an observer to all necessary objects.
  void Init();

  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void CallBatteryPercentCallback(int percent);

  DISALLOW_COPY_AND_ASSIGN(DeviceEmulatorMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
