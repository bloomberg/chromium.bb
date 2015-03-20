// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_settings_ui.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/settings_resources.h"
#include "grit/settings_resources_map.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#endif

MdSettingsUI::MdSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // TODO(jlklein): Remove handler logic once settingsPrivate is ready.
#if defined(OS_CHROMEOS)
  core_handler_ = new chromeos::options::CoreChromeOSOptionsHandler();
#else
  core_handler_ = new options::CoreOptionsHandler();
#endif

  core_handler_->set_handlers_host(this);
  scoped_ptr<options::OptionsPageUIHandler> handler(core_handler_);
  if (handler->IsEnabled())
    web_ui->AddMessageHandler(handler.release());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIMdSettingsHost);

  // Add all settings resources.
  for (size_t i = 0; i < kSettingsResourcesSize; ++i) {
    html_source->AddResourcePath(kSettingsResources[i].name,
                                 kSettingsResources[i].value);
  }

  html_source->AddResourcePath("md_settings.css", IDR_MD_SETTINGS_UI_CSS);
  html_source->SetDefaultResource(IDR_MD_SETTINGS_UI_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
}

MdSettingsUI::~MdSettingsUI() {
}

void MdSettingsUI::InitializeHandlers() {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(!profile->IsOffTheRecord() || profile->IsGuestSession());

  core_handler_->InitializeHandler();
  core_handler_->InitializePage();
}
