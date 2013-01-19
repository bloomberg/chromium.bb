// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "grit/browser_resources.h"

OmniboxUI::OmniboxUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://omnibox/ source.
  content::WebUIDataSource* html_source =
      ChromeWebUIDataSource::Create(chrome::kChromeUIOmniboxHost);
  html_source->AddResourcePath("omnibox.css", IDR_OMNIBOX_CSS);
  html_source->AddResourcePath("omnibox.js", IDR_OMNIBOX_JS);
  html_source->SetDefaultResource(IDR_OMNIBOX_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddWebUIDataSource(profile, html_source);

  // AddMessageHandler takes ownership of OmniboxUIHandler
  web_ui->AddMessageHandler(new OmniboxUIHandler(profile));
}

OmniboxUI::~OmniboxUI() { }
