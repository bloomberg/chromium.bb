// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_message_handler.h"

#include <stdlib.h>

#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace {

// Define the name of the callback functions that will be used by JavaScript.
const char kBatteryPercentFunction[] = "requestBatteryInfo";
const char kUpdateBatteryPercentFunction[] = "updateBatteryInfo";
const char kUpdatePowerModeFunction[] = "updatePowerSource";
const char kBatteryPercentFunctionJSCallback[] =
    "device_emulator.setBatteryInfo";

}  // namespace

DeviceEmulatorMessageHandler::DeviceEmulatorMessageHandler() {
}

DeviceEmulatorMessageHandler::~DeviceEmulatorMessageHandler() {
}

void DeviceEmulatorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kBatteryPercentFunction,
      base::Bind(&DeviceEmulatorMessageHandler::HandleRequestBatteryInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateBatteryPercentFunction,
      base::Bind(&DeviceEmulatorMessageHandler::HandleUpdateBatteryInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdatePowerModeFunction,
      base::Bind(&DeviceEmulatorMessageHandler::HandleUpdatePowerSource,
                 base::Unretained(this)));
}

void DeviceEmulatorMessageHandler::HandleRequestBatteryInfo(
    const base::ListValue* args) {
  web_ui()->CallJavascriptFunction(kBatteryPercentFunctionJSCallback,
                                   base::FundamentalValue(rand() % 101));
}

void DeviceEmulatorMessageHandler::HandleUpdateBatteryInfo(
    const base::ListValue* args) {
  int new_percent = -1;
  args->GetInteger(0, &new_percent);
}

void DeviceEmulatorMessageHandler::HandleUpdatePowerSource(
    const base::ListValue* args) {
}
