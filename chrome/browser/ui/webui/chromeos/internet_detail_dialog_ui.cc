// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace {

void AddInternetStrings(content::WebUIDataSource* html_source) {
  // Add default strings first.
  chromeos::network_element::AddLocalizedStrings(html_source);
  chromeos::network_element::AddOncLocalizedStrings(html_source);
  chromeos::network_element::AddDetailsLocalizedStrings(html_source);
  // Add additional strings and overrides needed by the dialog.
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"networkButtonConnect", IDS_SETTINGS_INTERNET_BUTTON_CONNECT},
      {"networkButtonDisconnect", IDS_SETTINGS_INTERNET_BUTTON_DISCONNECT},
      {"networkIPAddress", IDS_SETTINGS_INTERNET_NETWORK_IP_ADDRESS},
      {"networkSectionNetwork", IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK},
      {"networkSectionProxy", IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY},
      {"networkIPConfigAuto", IDS_SETTINGS_INTERNET_NETWORK_IP_CONFIG_AUTO},
      {"save", IDS_SAVE},
      // Override for network_element::AddDetailsLocalizedStrings
      {"networkProxyConnectionType",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_CONNECTION_TYPE_DIALOG},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
}

}  // namespace

InternetDetailDialogUI::InternetDetailDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIInternetDetailDialogHost);

  AddInternetStrings(source);

  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_INTERNET_DETAIL_DIALOG_HTML);
  source->DisableContentSecurityPolicy();

  source->AddResourcePath("internet_detail_dialog.js",
                          IDR_INTERNET_DETAIL_DIALOG_JS);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

InternetDetailDialogUI::~InternetDetailDialogUI() {}

}  // namespace chromeos
