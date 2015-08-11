// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_ui.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

// Create data source for chrome://device-emulator/.
content::WebUIDataSource* CreateDeviceEmulatorUIDataSource() {
  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUIDeviceEmulatorHost);

  html->SetJsonPath("strings.js");

  // Add resources.
  html->AddResourcePath("device_emulator.css", IDR_DEVICE_EMULATOR_CSS);
  html->AddResourcePath("device_emulator.js", IDR_DEVICE_EMULATOR_JS);
  html->AddResourcePath("audio_settings.html", IDR_AUDIO_SETTINGS_HTML);
  html->AddResourcePath("audio_settings.css", IDR_AUDIO_SETTINGS_CSS);
  html->AddResourcePath("audio_settings.js", IDR_AUDIO_SETTINGS_JS);
  html->AddResourcePath("battery_settings.html", IDR_BATTERY_SETTINGS_HTML);
  html->AddResourcePath("battery_settings.css", IDR_BATTERY_SETTINGS_CSS);
  html->AddResourcePath("battery_settings.js", IDR_BATTERY_SETTINGS_JS);
  html->AddResourcePath("bluetooth_settings.html", IDR_BLUETOOTH_SETTINGS_HTML);
  html->AddResourcePath("bluetooth_settings.css", IDR_BLUETOOTH_SETTINGS_CSS);
  html->AddResourcePath("bluetooth_settings.js", IDR_BLUETOOTH_SETTINGS_JS);
  html->SetDefaultResource(IDR_DEVICE_EMULATOR_HTML);

  return html;
}

}  // namespace

DeviceEmulatorUI::DeviceEmulatorUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  chromeos::DeviceEmulatorMessageHandler* handler =
      new chromeos::DeviceEmulatorMessageHandler();
  handler->Init();
  web_ui->AddMessageHandler(handler);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                CreateDeviceEmulatorUIDataSource());
}

DeviceEmulatorUI::~DeviceEmulatorUI() {
}
