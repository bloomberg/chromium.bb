// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_ui.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/chromeos/bluetooth_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

BluetoothPairingUI::BluetoothPairingUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  base::DictionaryValue localized_strings;

  auto core_handler = base::MakeUnique<options::CoreChromeOSOptionsHandler>();
  core_handler_ = core_handler.get();
  web_ui->AddMessageHandler(std::move(core_handler));
  core_handler_->set_handlers_host(this);
  core_handler_->GetLocalizedValues(&localized_strings);

  auto bluetooth_handler = base::MakeUnique<options::BluetoothOptionsHandler>();
  bluetooth_handler_ = bluetooth_handler.get();
  web_ui->AddMessageHandler(std::move(bluetooth_handler));
  bluetooth_handler_->GetLocalizedValues(&localized_strings);

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBluetoothPairingHost);
  source->AddLocalizedStrings(localized_strings);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_BLUETOOTH_PAIR_DEVICE_HTML);
  source->DisableContentSecurityPolicy();

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

BluetoothPairingUI::~BluetoothPairingUI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them.
  core_handler_->Uninitialize();
  bluetooth_handler_->Uninitialize();
}

void BluetoothPairingUI::InitializeHandlers() {
  core_handler_->InitializeHandler();
  bluetooth_handler_->InitializeHandler();
  core_handler_->InitializePage();
  bluetooth_handler_->InitializePage();
}

}  // namespace chromeos
