// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_ui.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

// Create data source for chrome://device-emulator/.
content::WebUIDataSource* CreateDeviceEmulatorUIDataSource() {
  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUIDeviceEmulatorHost);

  // Add variables for the JS to use.
  html->AddString("acPower", base::IntToString(
      power_manager::PowerSupplyProperties_ExternalPower_AC));
  html->AddString("usbPower", base::IntToString(
      power_manager::PowerSupplyProperties_ExternalPower_USB));
  html->AddString("disconnected", base::IntToString(
      power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED));
  html->SetJsonPath("strings.js");

  // Add resources.
  html->AddResourcePath("device_emulator.css", IDR_DEVICE_EMULATOR_CSS);
  html->AddResourcePath("device_emulator.js", IDR_DEVICE_EMULATOR_JS);
  html->SetDefaultResource(IDR_DEVICE_EMULATOR_HTML);

  return html;
}

}  // namespace

DeviceEmulatorUI::DeviceEmulatorUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new DeviceEmulatorMessageHandler());

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateDeviceEmulatorUIDataSource());
}

DeviceEmulatorUI::~DeviceEmulatorUI() {
}
