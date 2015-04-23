// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/webui/proximity_auth_ui.h"

#include "components/proximity_auth/webui/proximity_auth_webui_handler.h"
#include "components/proximity_auth/webui/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/components_resources.h"

namespace proximity_auth {

ProximityAuthUI::ProximityAuthUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIProximityAuthHost);
  source->SetDefaultResource(IDR_PROXIMITY_AUTH_HTML);
  source->AddResourcePath("proximity_auth.css", IDR_PROXIMITY_AUTH_CSS);
  source->AddResourcePath("proximity_auth.js", IDR_PROXIMITY_AUTH_JS);
  source->SetJsonPath("strings.js");

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source);
  web_ui->AddMessageHandler(new ProximityAuthWebUIHandler());
}

ProximityAuthUI::~ProximityAuthUI() {
}

}  // namespace proximity_auth
