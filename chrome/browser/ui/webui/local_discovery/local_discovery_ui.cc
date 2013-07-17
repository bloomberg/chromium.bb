// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace {

content::WebUIDataSource* CreateLocalDiscoveryHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDevicesHost);

  source->SetDefaultResource(IDR_LOCAL_DISCOVERY_HTML);
  source->AddResourcePath("local_discovery.css", IDR_LOCAL_DISCOVERY_CSS);
  source->AddResourcePath("local_discovery.js", IDR_LOCAL_DISCOVERY_JS);
  return source;
}

}  // namespace

LocalDiscoveryUI::LocalDiscoveryUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://devices/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateLocalDiscoveryHTMLSource());

  // TODO(gene): Use LocalDiscoveryUIHandler to send updated to the devices
  // page. For example
  // web_ui->AddMessageHandler(new LocalDiscoveryUIHandler());
}
