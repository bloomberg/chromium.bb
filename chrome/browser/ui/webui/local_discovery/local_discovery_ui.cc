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
#include "grit/generated_resources.h"

namespace {

content::WebUIDataSource* CreateLocalDiscoveryHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDevicesHost);

  source->SetDefaultResource(IDR_LOCAL_DISCOVERY_HTML);
  source->AddResourcePath("local_discovery.css", IDR_LOCAL_DISCOVERY_CSS);
  source->AddResourcePath("local_discovery.js", IDR_LOCAL_DISCOVERY_JS);

  source->SetUseJsonJSFormatV2();
  source->AddLocalizedString("serviceName",
                             IDS_LOCAL_DISCOVERY_SERVICE_NAME);
  source->AddLocalizedString("serviceDomain",
                             IDS_LOCAL_DISCOVERY_SERVICE_DOMAIN);
  source->AddLocalizedString("servicePort",
                             IDS_LOCAL_DISCOVERY_SERVICE_PORT);
  source->AddLocalizedString("serviceIp",
                             IDS_LOCAL_DISCOVERY_SERVICE_IP);
  source->AddLocalizedString("serviceLastseen",
                             IDS_LOCAL_DISCOVERY_SERVICE_LASTSEEN);
  source->AddLocalizedString("serviceRegister",
                             IDS_LOCAL_DISCOVERY_SERVICE_REGISTER);
  source->AddLocalizedString("registeringService",
                             IDS_LOCAL_DISCOVERY_REGISTERING_SERVICE);
  source->AddLocalizedString("registrationFailed",
                             IDS_LOCAL_DISCOVERY_REGISTRATION_FAILED);
  source->AddLocalizedString("registrationSucceeded",
                             IDS_LOCAL_DISCOVERY_REGISTRATION_SUCCEEDED);
  source->AddLocalizedString("registered",
                             IDS_LOCAL_DISCOVERY_REGISTERED);
  source->AddLocalizedString("infoStarted", IDS_LOCAL_DISCOVERY_INFO_STARTED);
  source->AddLocalizedString("infoFailed", IDS_LOCAL_DISCOVERY_INFO_FAILED);
  source->AddLocalizedString("serviceInfo",
                             IDS_LOCAL_DISCOVERY_SERVICE_INFO);

  source->SetJsonPath("strings.js");

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
  web_ui->AddMessageHandler(local_discovery::LocalDiscoveryUIHandler::Create());
}
