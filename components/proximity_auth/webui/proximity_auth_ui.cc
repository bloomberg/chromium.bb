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
  source->SetDefaultResource(IDR_PROXIMITY_AUTH_POLLUX_DEBUG_HTML);
  source->AddResourcePath("pollux_debug.css",
                          IDR_PROXIMITY_AUTH_POLLUX_DEBUG_CSS);
  source->AddResourcePath("pollux_debug.js",
                          IDR_PROXIMITY_AUTH_POLLUX_DEBUG_JS);
  source->AddResourcePath("webui.js", IDR_PROXIMITY_AUTH_POLLUX_WEBUI_JS);
  source->AddResourcePath("logs_controller.js",
                          IDR_PROXIMITY_AUTH_POLLUX_LOGS_CONTROLLER_JS);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source);
  web_ui->AddMessageHandler(
      base::MakeUnique<ProximityAuthWebUIHandler>(delegate));
}

ProximityAuthUI::~ProximityAuthUI() {
}

}  // namespace proximity_auth
