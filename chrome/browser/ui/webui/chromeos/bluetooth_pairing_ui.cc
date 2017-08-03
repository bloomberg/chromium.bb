// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_dialog_localized_strings_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace {

void AddBluetoothStrings(content::WebUIDataSource* html_source) {
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"ok", IDS_OK},
      {"bluetoothPairDevicePageTitle",
       IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE_TITLE},
      {"cancel", IDS_CANCEL},
      {"close", IDS_CLOSE},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
  chromeos::bluetooth_dialog::AddLocalizedStrings(html_source);
}

}  // namespace

BluetoothPairingUI::BluetoothPairingUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBluetoothPairingHost);

  AddBluetoothStrings(source);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_BLUETOOTH_PAIR_DEVICE_HTML);
  source->DisableContentSecurityPolicy();

  source->AddResourcePath("bluetooth_dialog_host.html",
                          IDR_BLUETOOTH_DIALOG_HOST_HTML);
  source->AddResourcePath("bluetooth_dialog_host.js",
                          IDR_BLUETOOTH_DIALOG_HOST_JS);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

BluetoothPairingUI::~BluetoothPairingUI() {}

}  // namespace chromeos
