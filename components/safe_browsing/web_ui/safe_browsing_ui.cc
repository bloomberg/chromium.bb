// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/web_ui/safe_browsing_ui.h"

#include <algorithm>
#include <utility>
#include "base/macros.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
namespace safe_browsing {
SafeBrowsingUI::SafeBrowsingUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://safe-browsing source.

  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      safe_browsing::kChromeUISafeBrowsingHost);

  // Register callback handler.
  // Handles messages from JavaScript to C++ via chrome.send().
  web_ui->AddMessageHandler(base::MakeUnique<SafeBrowsingUIHandler>());

  // Add localized string resources.
  html_source->AddLocalizedString("sbUnderConstruction",
                                  IDS_SB_UNDER_CONSTRUCTION);

  // Add required resources.
  html_source->AddResourcePath("safe_browsing.css", IDR_SAFE_BROWSING_CSS);
  html_source->AddResourcePath("safe_browsing.js", IDR_SAFE_BROWSING_JS);
  html_source->SetDefaultResource(IDR_SAFE_BROWSING_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);
}

SafeBrowsingUI::~SafeBrowsingUI() {}

SafeBrowsingUIHandler::SafeBrowsingUIHandler(){};

void SafeBrowsingUIHandler::ExpParamList(const base::ListValue* unused) {
  AllowJavascript();
  CallJavascriptFunction("safe_browsing.addExperiment", GetFeatureStatusList());
}

void SafeBrowsingUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "expParamList",
      base::Bind(&SafeBrowsingUIHandler::ExpParamList, base::Unretained(this)));
}

SafeBrowsingUIHandler::~SafeBrowsingUIHandler() {}
}  // namespace safe_browsing
