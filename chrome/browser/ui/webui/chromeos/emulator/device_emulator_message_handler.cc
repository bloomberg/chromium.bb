// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_message_handler.h"

#include <stdlib.h>

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "content/public/browser/web_ui.h"

namespace {

// Define request function that will request the values that are set in
// power manager properties. Used to initialize the web UI values.
const char kRequestPowerInfo[] = "requestPowerInfo";

// Define update functions that will update the power properties to the
// variables defined in the web UI.
const char kUpdateBatteryPercent[] = "updateBatteryPercent";
const char kUpdateExternalPower[] = "updateExternalPower";
const char kUpdateTimeToEmpty[] = "updateTimeToEmpty";
const char kUpdateTimeToFull[] = "updateTimeToFull";

// Define callback functions that will update the JavaScript variable
// and the web UI.
const char kUpdatePowerPropertiesJSCallback[] =
    "device_emulator.batterySettings.updatePowerProperties";

}  // namespace

DeviceEmulatorMessageHandler::DeviceEmulatorMessageHandler()
    : fake_power_manager_client_(static_cast<chromeos::FakePowerManagerClient*>(
          chromeos::DBusThreadManager::Get()
              ->GetPowerManagerClient())) {}

DeviceEmulatorMessageHandler::~DeviceEmulatorMessageHandler() {
  fake_power_manager_client_->RemoveObserver(this);
}

void DeviceEmulatorMessageHandler::Init() {
  fake_power_manager_client_->AddObserver(this);
}

void DeviceEmulatorMessageHandler::RequestPowerInfo(
    const base::ListValue* args) {
  fake_power_manager_client_->RequestStatusUpdate();
}

void DeviceEmulatorMessageHandler::UpdateBatteryPercent(
    const base::ListValue* args) {
  power_manager::PowerSupplyProperties props =
      fake_power_manager_client_->props();

  int new_percent;
  if (args->GetInteger(0, &new_percent))
    props.set_battery_percent(new_percent);

  fake_power_manager_client_->UpdatePowerProperties(props);
}

void DeviceEmulatorMessageHandler::UpdateExternalPower(
    const base::ListValue* args) {
  int power_source;
  args->GetInteger(0, &power_source);

  power_manager::PowerSupplyProperties props =
      fake_power_manager_client_->props();
  props.set_external_power(
      static_cast<power_manager::PowerSupplyProperties_ExternalPower>(
          power_source));
  fake_power_manager_client_->UpdatePowerProperties(props);
}

void DeviceEmulatorMessageHandler::UpdateTimeToEmpty(
    const base::ListValue* args) {
  power_manager::PowerSupplyProperties props =
      fake_power_manager_client_->props();

  int new_time;
  if (args->GetInteger(0, &new_time))
    props.set_battery_time_to_empty_sec(new_time);

  fake_power_manager_client_->UpdatePowerProperties(props);
}

void DeviceEmulatorMessageHandler::UpdateTimeToFull(
    const base::ListValue* args) {
  power_manager::PowerSupplyProperties props =
      fake_power_manager_client_->props();
  int new_time;
  if (args->GetInteger(0, &new_time))
    props.set_battery_time_to_full_sec(new_time);
  fake_power_manager_client_->UpdatePowerProperties(props);
}

void DeviceEmulatorMessageHandler::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  web_ui()->CallJavascriptFunction(
      kUpdatePowerPropertiesJSCallback,
      base::FundamentalValue(proto.battery_percent()),
      base::FundamentalValue(proto.external_power()),
      base::FundamentalValue(
          static_cast<int>(proto.battery_time_to_empty_sec())),
      base::FundamentalValue(
          static_cast<int>(proto.battery_time_to_full_sec())));
}

void DeviceEmulatorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kRequestPowerInfo,
      base::Bind(&DeviceEmulatorMessageHandler::RequestPowerInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateBatteryPercent,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateBatteryPercent,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateExternalPower,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateExternalPower,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateTimeToEmpty,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateTimeToEmpty,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateTimeToFull,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateTimeToFull,
                 base::Unretained(this)));
}
