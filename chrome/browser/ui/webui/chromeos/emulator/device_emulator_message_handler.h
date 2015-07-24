// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_

#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
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

  // Callbacks for JS request methods. All these methods work
  // asynchronously.
  void RequestPowerInfo(const base::ListValue* args);

  // Callback for the "requestBluetoothDiscover" message. This asynchronously
  // requests for the system to discover a certain device. The device's data
  // should be passed into |args| as a dictionary. If the device does not
  // already exist, then it will be created and attached to the main adapter.
  void HandleRequestBluetoothDiscover(const base::ListValue* args);

  // Callback for the "requestBluetoothPair" message. This asynchronously
  // requests for the system to pair a certain device. The device's data should
  // be passed into |args| as a dictionary. If the device does not already
  // exist, then it will be created and attached to the main adapter.
  void HandleRequestBluetoothPair(const base::ListValue* args);

  // Callbacks for JS update methods. All these methods work
  // asynchronously.
  void UpdateBatteryPercent(const base::ListValue* args);
  void UpdateExternalPower(const base::ListValue* args);
  void UpdateTimeToEmpty(const base::ListValue* args);
  void UpdateTimeToFull(const base::ListValue* args);

  // Adds |this| as an observer to all necessary objects.
  void Init();

  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  chromeos::FakePowerManagerClient* fake_power_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(DeviceEmulatorMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
