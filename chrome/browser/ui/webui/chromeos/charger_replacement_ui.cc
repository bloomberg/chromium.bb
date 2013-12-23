// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/charger_replacement_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/charger_replacement_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace chromeos {

ChargerReplacementUI::ChargerReplacementUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  base::DictionaryValue localized_strings;
  ChargerReplacementHandler::GetLocalizedValues(&localized_strings);
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIChargerReplacementHost);
  source->SetUseJsonJSFormatV2();
  source->AddLocalizedStrings(localized_strings);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_CHARGER_REPLACEMENT_HTML);
  source->DisableContentSecurityPolicy();

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

ChargerReplacementUI::~ChargerReplacementUI() {
}

}  // namespace chromeos
