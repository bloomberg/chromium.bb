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
#include "grit/webui_resources.h"

namespace proximity_auth {

ProximityAuthUI::ProximityAuthUI(content::WebUI* web_ui,
                                 ProximityAuthClient* delegate)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIProximityAuthHost);
  source->SetDefaultResource(IDR_PROXIMITY_AUTH_UI_HTML);
  source->AddResourcePath("proximity_auth.css", IDR_PROXIMITY_AUTH_UI_CSS);
  source->AddResourcePath("content-panel.html",
                          IDR_PROXIMITY_AUTH_CONTENT_PANEL_HTML);
  source->AddResourcePath("content-panel.js",
                          IDR_PROXIMITY_AUTH_CONTENT_PANEL_JS);
  source->AddResourcePath("log-panel.html", IDR_PROXIMITY_AUTH_LOG_PANEL_HTML);
  source->AddResourcePath("log-panel.js", IDR_PROXIMITY_AUTH_LOG_PANEL_JS);
  source->AddResourcePath("local-state.html",
                          IDR_PROXIMITY_AUTH_LOCAL_STATE_HTML);
  source->AddResourcePath("local-state.js", IDR_PROXIMITY_AUTH_LOCAL_STATE_JS);
  source->AddResourcePath("device-list.html",
                          IDR_PROXIMITY_AUTH_DEVICE_LIST_HTML);
  source->AddResourcePath("device-list.js", IDR_PROXIMITY_AUTH_DEVICE_LIST_JS);
  source->AddResourcePath("log-buffer.html",
                          IDR_PROXIMITY_AUTH_LOG_BUFFER_HTML);
  source->AddResourcePath("log-buffer.js", IDR_PROXIMITY_AUTH_LOG_BUFFER_JS);
  source->AddResourcePath("eligible-devices.html",
                          IDR_PROXIMITY_AUTH_ELIGIBLE_DEVICES_HTML);
  source->AddResourcePath("eligible-devices.js",
                          IDR_PROXIMITY_AUTH_ELIGIBLE_DEVICES_JS);
  source->AddResourcePath("reachable-devices.html",
                          IDR_PROXIMITY_AUTH_REACHABLE_DEVICES_HTML);
  source->AddResourcePath("reachable-devices.js",
                          IDR_PROXIMITY_AUTH_REACHABLE_DEVICES_JS);
  source->AddResourcePath("cryptauth_interface.js",
                          IDR_PROXIMITY_AUTH_CRYPTAUTH_INTERFACE_JS);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source);
  web_ui->AddMessageHandler(new ProximityAuthWebUIHandler(delegate));
}

ProximityAuthUI::~ProximityAuthUI() {
}

}  // namespace proximity_auth
