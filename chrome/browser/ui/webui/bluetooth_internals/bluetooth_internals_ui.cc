// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

BluetoothInternalsUI::BluetoothInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://bluetooth-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIBluetoothInternalsHost);

  // Add required resources.
  html_source->AddResourcePath("adapter_broker.js",
                               IDR_BLUETOOTH_INTERNALS_ADAPTER_BROKER_JS);
  html_source->AddResourcePath("bluetooth_internals.css",
                               IDR_BLUETOOTH_INTERNALS_CSS);
  html_source->AddResourcePath("bluetooth_internals.js",
                               IDR_BLUETOOTH_INTERNALS_JS);
  html_source->AddResourcePath("device_collection.js",
                               IDR_BLUETOOTH_INTERNALS_DEVICE_COLLECTION_JS);
  html_source->AddResourcePath("device_table.js",
                               IDR_BLUETOOTH_INTERNALS_DEVICE_TABLE_JS);
  html_source->AddResourcePath("interfaces.js",
                               IDR_BLUETOOTH_INTERNALS_INTERFACES_JS);

  html_source->AddResourcePath(
      "device/bluetooth/public/interfaces/adapter.mojom",
      IDR_BLUETOOTH_ADAPTER_MOJO_JS);
  html_source->AddResourcePath(
      "device/bluetooth/public/interfaces/device.mojom",
      IDR_BLUETOOTH_DEVICE_MOJO_JS);
  html_source->SetDefaultResource(IDR_BLUETOOTH_INTERNALS_HTML);
  html_source->DisableI18nAndUseGzipForAllPaths();

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

BluetoothInternalsUI::~BluetoothInternalsUI() {}
