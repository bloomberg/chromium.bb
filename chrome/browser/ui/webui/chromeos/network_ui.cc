// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace chromeos {

namespace {

const char kStringsJsFile[] = "strings.js";
const char kRequestNetworkInfoCallback[] = "requestNetworkInfo";
const char kNetworkEventLogTag[] = "networkEventLog";
const char kNetworkStateTag[] = "networkState";
const char kOnNetworkInfoReceivedFunction[] = "NetworkUI.onNetworkInfoReceived";

class NetworkMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkMessageHandler();
  virtual ~NetworkMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void CollectNetworkInfo(const base::ListValue* value) const;
  std::string GetNetworkEventLog() const;
  void GetNetworkState(base::DictionaryValue* output) const;
  void RespondToPage(const base::DictionaryValue& value) const;

  DISALLOW_COPY_AND_ASSIGN(NetworkMessageHandler);
};

NetworkMessageHandler::NetworkMessageHandler() {
}

NetworkMessageHandler::~NetworkMessageHandler() {
}

void NetworkMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kRequestNetworkInfoCallback,
      base::Bind(&NetworkMessageHandler::CollectNetworkInfo,
                 base::Unretained(this)));
}

void NetworkMessageHandler::CollectNetworkInfo(
    const base::ListValue* value) const {
  base::DictionaryValue data;
  data.SetString(kNetworkEventLogTag, GetNetworkEventLog());
  base::DictionaryValue* networkState = new base::DictionaryValue;
  GetNetworkState(networkState);
  data.Set(kNetworkStateTag, networkState);
  RespondToPage(data);
}

void NetworkMessageHandler::RespondToPage(
    const base::DictionaryValue& value) const {
  web_ui()->CallJavascriptFunction(kOnNetworkInfoReceivedFunction, value);
}

std::string NetworkMessageHandler::GetNetworkEventLog() const {
  std::string format = "json";
  return chromeos::network_event_log::GetAsString(
      chromeos::network_event_log::NEWEST_FIRST,
      format,
      chromeos::network_event_log::LOG_LEVEL_DEBUG,
      0);
}

void NetworkMessageHandler::GetNetworkState(
    base::DictionaryValue* output) const {
  chromeos::NetworkStateHandler* handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  chromeos::NetworkStateHandler::NetworkStateList network_list;
  handler->GetNetworkList(&network_list);

  for (chromeos::NetworkStateHandler::NetworkStateList::const_iterator it =
           network_list.begin();
       it != network_list.end();
       ++it) {
    base::DictionaryValue* properties = new base::DictionaryValue;
    (*it)->GetProperties(properties);
    output->Set((*it)->path(), properties);
  }
}

}  // namespace

NetworkUI::NetworkUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(new NetworkMessageHandler());

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
  html->AddLocalizedString("logEntryFormat", IDS_NETWORK_LOG_ENTRY);
  html->SetJsonPath(kStringsJsFile);

  html->AddResourcePath("network.css", IDR_NETWORK_CSS);
  html->AddResourcePath("network.js", IDR_NETWORK_JS);
  html->SetDefaultResource(IDR_NETWORK_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html);
}

NetworkUI::~NetworkUI() {
}

}  // namespace chromeos
