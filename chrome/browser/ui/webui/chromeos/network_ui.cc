// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_ui.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/network_config_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace chromeos {

NetworkUI::NetworkUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(new NetworkConfigMessageHandler());

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUINetworkHost);

  html->AddLocalizedString("titleText", IDS_NETWORK_UI_TITLE);
  html->AddLocalizedString("autoRefreshText", IDS_NETWORK_UI_AUTO_REFRESH);
  html->AddLocalizedString("deviceLogLinkText", IDS_DEVICE_LOG_LINK_TEXT);
  html->AddLocalizedString("networkRefreshText", IDS_NETWORK_UI_REFRESH);
  html->AddLocalizedString("clickToExpandText", IDS_NETWORK_UI_EXPAND);
  html->AddLocalizedString("propertyFormatText",
                           IDS_NETWORK_UI_PROPERTY_FORMAT);

  html->AddLocalizedString("normalFormatOption", IDS_NETWORK_UI_FORMAT_NORMAL);
  html->AddLocalizedString("managedFormatOption",
                           IDS_NETWORK_UI_FORMAT_MANAGED);
  html->AddLocalizedString("shillFormatOption", IDS_NETWORK_UI_FORMAT_SHILL);

  html->AddLocalizedString("visibleNetworksLabel",
                           IDS_NETWORK_UI_VISIBLE_NETWORKS);
  html->AddLocalizedString("favoriteNetworksLabel",
                           IDS_NETWORK_UI_FAVORITE_NETWORKS);

  html->SetJsonPath("strings.js");
  html->AddResourcePath("network_config.js", IDR_NETWORK_CONFIG_JS);
  html->AddResourcePath("network_ui.css", IDR_NETWORK_UI_CSS);
  html->AddResourcePath("network_ui.js", IDR_NETWORK_UI_JS);
  html->SetDefaultResource(IDR_NETWORK_UI_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html);
}

NetworkUI::~NetworkUI() {
}

}  // namespace chromeos
