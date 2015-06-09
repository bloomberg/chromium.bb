// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/appearance_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/settings_resources.h"
#include "grit/settings_resources_map.h"

namespace settings {

SettingsPageUIHandler::SettingsPageUIHandler() {
}

SettingsPageUIHandler::~SettingsPageUIHandler() {
}

MdSettingsUI::MdSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  AddSettingsPageUIHandler(new AppearanceHandler(web_ui));
  AddSettingsPageUIHandler(new DownloadsHandler());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIMdSettingsHost);

  // Add all settings resources.
  for (size_t i = 0; i < kSettingsResourcesSize; ++i) {
    html_source->AddResourcePath(kSettingsResources[i].name,
                                 kSettingsResources[i].value);
  }

  AddLocalizedStrings(html_source);
  html_source->AddResourcePath("md_settings.css", IDR_MD_SETTINGS_UI_CSS);
  html_source->SetDefaultResource(IDR_MD_SETTINGS_UI_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
}

MdSettingsUI::~MdSettingsUI() {
}

void MdSettingsUI::AddSettingsPageUIHandler(
    settings::SettingsPageUIHandler* handler_raw) {
  scoped_ptr<settings::SettingsPageUIHandler> handler(handler_raw);
  DCHECK(handler.get());

  web_ui()->AddMessageHandler(handler.release());
}

}  // namespace settings
