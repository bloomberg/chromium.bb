// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/network_config_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chromeos/network/network_event_log.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace chromeos {

namespace {

const int kMaxLogEvents = 1000;

class NetworkUIMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkUIMessageHandler() {}
  virtual ~NetworkUIMessageHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE {
    web_ui()->RegisterMessageCallback(
        "NetworkUI.getNetworkLog",
        base::Bind(&NetworkUIMessageHandler::GetNetworkLog,
                   base::Unretained(this)));
  }

 private:
  void GetNetworkLog(const base::ListValue* value) const {
    base::StringValue data(chromeos::network_event_log::GetAsString(
        chromeos::network_event_log::NEWEST_FIRST,
        "json",
        chromeos::network_event_log::LOG_LEVEL_DEBUG,
        kMaxLogEvents));
    web_ui()->CallJavascriptFunction("NetworkUI.getNetworkLogCallback", data);
  }

  DISALLOW_COPY_AND_ASSIGN(NetworkUIMessageHandler);
};

}  // namespace

NetworkUI::NetworkUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(new NetworkConfigMessageHandler());
  web_ui->AddMessageHandler(new NetworkUIMessageHandler());

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUINetworkHost);
  html->SetUseJsonJSFormatV2();

  html->AddLocalizedString("titleText", IDS_NETWORK_TITLE);
  html->AddLocalizedString("autoRefreshText", IDS_NETWORK_AUTO_REFRESH);
  html->AddLocalizedString("logRefreshText", IDS_NETWORK_LOG_REFRESH);
  html->AddLocalizedString("logLevelShowText", IDS_NETWORK_LOG_LEVEL_SHOW);
  html->AddLocalizedString("logLevelErrorText", IDS_NETWORK_LOG_LEVEL_ERROR);
  html->AddLocalizedString("logLevelUserText", IDS_NETWORK_LOG_LEVEL_USER);
  html->AddLocalizedString("logLevelEventText", IDS_NETWORK_LOG_LEVEL_EVENT);
  html->AddLocalizedString("logLevelDebugText", IDS_NETWORK_LOG_LEVEL_DEBUG);
  html->AddLocalizedString("logLevelFileinfoText",
                           IDS_NETWORK_LOG_LEVEL_FILEINFO);
  html->AddLocalizedString("logLevelTimeDetailText",
                           IDS_NETWORK_LOG_LEVEL_TIME_DETAIL);
  html->AddLocalizedString("logEntryFormat", IDS_NETWORK_LOG_ENTRY);
  html->SetJsonPath("strings.js");
  html->AddResourcePath("network_config.js", IDR_NETWORK_CONFIG_JS);
  html->AddResourcePath("network_ui.css", IDR_NETWORK_UI_CSS);
  html->AddResourcePath("network_ui.js", IDR_NETWORK_UI_JS);
  html->SetDefaultResource(IDR_NETWORK_UI_HTML);

  content::WebUIDataSource::Add(
      web_ui->GetWebContents()->GetBrowserContext(), html);
}

NetworkUI::~NetworkUI() {
}

}  // namespace chromeos
