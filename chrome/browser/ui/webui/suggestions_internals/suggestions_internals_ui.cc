// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/suggestions_internals/suggestions_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/suggestions_internals/suggestions_internals_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

SuggestionsInternalsUI::SuggestionsInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://suggestions-internals/ source.
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUISuggestionsInternalsHost);
  html_source->AddResourcePath("suggestions_internals.css",
                               IDR_SUGGESTIONS_INTERNALS_CSS);
  html_source->AddResourcePath("suggestions_internals.js",
                               IDR_SUGGESTIONS_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_SUGGESTIONS_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
  content::URLDataSource::Add(
      profile, new FaviconSource(profile, FaviconSource::FAVICON));

  // AddMessageHandler takes ownership of SuggestionsInternalsUIHandler
  web_ui->AddMessageHandler(new SuggestionsInternalsUIHandler(profile));
}

SuggestionsInternalsUI::~SuggestionsInternalsUI() { }
