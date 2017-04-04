// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/webui/proximity_auth_ui.h"

#include "base/memory/ptr_util.h"
#include "components/grit/components_resources.h"
#include "components/proximity_auth/webui/proximity_auth_webui_handler.h"
#include "components/proximity_auth/webui/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_resources.h"

namespace proximity_auth {

ProximityAuthUI::ProximityAuthUI(content::WebUI* web_ui,
                                 ProximityAuthClient* delegate)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIProximityAuthHost);
  source->SetDefaultResource(IDR_PROXIMITY_AUTH_INDEX_HTML);
  source->AddResourcePath("common.css", IDR_PROXIMITY_AUTH_COMMON_CSS);
  source->AddResourcePath("webui.js", IDR_PROXIMITY_AUTH_WEBUI_JS);
  source->AddResourcePath("logs.js", IDR_PROXIMITY_AUTH_LOGS_JS);
  source->AddResourcePath("proximity_auth.html",
                          IDR_PROXIMITY_AUTH_PROXIMITY_AUTH_HTML);
  source->AddResourcePath("proximity_auth.css",
                          IDR_PROXIMITY_AUTH_PROXIMITY_AUTH_CSS);
  source->AddResourcePath("proximity_auth.js",
                          IDR_PROXIMITY_AUTH_PROXIMITY_AUTH_JS);
  source->AddResourcePath("pollux.html", IDR_PROXIMITY_AUTH_POLLUX_HTML);
  source->AddResourcePath("pollux.css", IDR_PROXIMITY_AUTH_POLLUX_CSS);
  source->AddResourcePath("pollux.js", IDR_PROXIMITY_AUTH_POLLUX_JS);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source);
  web_ui->AddMessageHandler(
      base::MakeUnique<ProximityAuthWebUIHandler>(delegate));
}

ProximityAuthUI::~ProximityAuthUI() {
}

}  // namespace proximity_auth
