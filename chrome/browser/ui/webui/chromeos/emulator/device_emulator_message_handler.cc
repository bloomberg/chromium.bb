// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_message_handler.h"

#include <stdlib.h>

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "content/public/browser/web_ui.h"

namespace {

// Define the name of the callback functions that will be used by JavaScript.
const char kBatteryPercentFunction[] = "requestBatteryInfo";
const char kPowerSourceOptionsFunction[] = "requestExternalPowerOptions";
const char kUpdateBatteryPercentFunction[] = "updateBatteryPercent";
const char kUpdatePowerModeFunction[] = "updateExternalPower";
const char kBatteryPercentFunctionJSCallback[] =
    "device_emulator.batterySettings.setBatteryPercent";
const char kSetPowerSourceOptionsJSCallback[] =
    "device_emulator.batterySettings.setExternalPowerOptions";

const char kExternalPowerNameAc[] = "AC Power";
const char kExternalPowerNameUsb[] = "USB Power (Low energy)";
const char kExternalPowerNameDisconnected[] = "Disconnected";

}  // namespace

DeviceEmulatorMessageHandler::DeviceEmulatorMessageHandler() {
}

DeviceEmulatorMessageHandler::~DeviceEmulatorMessageHandler() {
  chromeos::FakePowerManagerClient* client =
      static_cast<chromeos::FakePowerManagerClient*>(
          chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
  client->RemoveObserver(this);
}

void DeviceEmulatorMessageHandler::Init() {
  chromeos::FakePowerManagerClient* client =
      static_cast<chromeos::FakePowerManagerClient*>(
          chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
  client->AddObserver(this);
}

void DeviceEmulatorMessageHandler::HandleRequestBatteryInfo(
    const base::ListValue* args) {
  CallBatteryPercentCallback(rand() % 101);
}

void DeviceEmulatorMessageHandler::HandleRequestExternalPowerOptions(
    const base::ListValue* args) {
  base::ListValue options;
  scoped_ptr<base::DictionaryValue> option1(new base::DictionaryValue());
  option1->SetString("text", kExternalPowerNameAc);
  option1->SetInteger("value",
                      power_manager::PowerSupplyProperties_ExternalPower_AC);

  options.Append(option1.Pass());

  scoped_ptr<base::DictionaryValue> option2(new base::DictionaryValue());
  option2->SetString("text", kExternalPowerNameUsb);
  option2->SetInteger("value",
                      power_manager::PowerSupplyProperties_ExternalPower_USB);

  options.Append(option2.Pass());

  scoped_ptr<base::DictionaryValue> option3(new base::DictionaryValue());
  option3->SetString("text", kExternalPowerNameDisconnected);
  option3->SetInteger(
      "value", power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);

  options.Append(option3.Pass());

  web_ui()->CallJavascriptFunction(kSetPowerSourceOptionsJSCallback, options);
}

void DeviceEmulatorMessageHandler::HandleUpdateBatteryPercent(
    const base::ListValue* args) {
  int new_percent = -1;
  args->GetInteger(0, &new_percent);

  // TODO(mozartalouis): Add call to FakePowerManagerClient
  // to update the battery percentage.
}

void DeviceEmulatorMessageHandler::HandleUpdateExternalPower(
    const base::ListValue* args) {
  int power_source;
  args->GetInteger(0, &power_source);
}

void DeviceEmulatorMessageHandler::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  CallBatteryPercentCallback(static_cast<int>(proto.battery_percent()));
}

void DeviceEmulatorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kBatteryPercentFunction,
      base::Bind(&DeviceEmulatorMessageHandler::HandleRequestBatteryInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kPowerSourceOptionsFunction,
      base::Bind(
          &DeviceEmulatorMessageHandler::HandleRequestExternalPowerOptions,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateBatteryPercentFunction,
      base::Bind(&DeviceEmulatorMessageHandler::HandleUpdateBatteryPercent,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdatePowerModeFunction,
      base::Bind(&DeviceEmulatorMessageHandler::HandleUpdateExternalPower,
                 base::Unretained(this)));
}

void DeviceEmulatorMessageHandler::CallBatteryPercentCallback(int percent) {
  web_ui()->CallJavascriptFunction(kBatteryPercentFunctionJSCallback,
                                   base::FundamentalValue(percent));
}
