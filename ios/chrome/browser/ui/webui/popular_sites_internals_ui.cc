// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/popular_sites_internals_ui.h"

#include "base/memory/ptr_util.h"
#include "components/grit/components_resources.h"
#include "components/ntp_tiles/popular_sites.h"
#include "components/ntp_tiles/webui/popular_sites_internals_message_handler.h"
#include "components/ntp_tiles/webui/popular_sites_internals_message_handler_client.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ntp_tiles/ios_popular_sites_factory.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

// The implementation for the chrome://popular-sites-internals page.
class IOSPopularSitesInternalsMessageHandlerBridge
    : public web::WebUIIOSMessageHandler,
      public ntp_tiles::PopularSitesInternalsMessageHandlerClient {
 public:
  IOSPopularSitesInternalsMessageHandlerBridge() : handler_(this) {}

 private:
  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

  // ntp_tiles::PopularSitesInternalsMessageHandlerClient
  std::unique_ptr<ntp_tiles::PopularSites> MakePopularSites() override;
  PrefService* GetPrefs() override;
  void RegisterMessageCallback(
      const std::string& message,
      const base::Callback<void(const base::ListValue*)>& callback) override;
  void CallJavascriptFunctionVector(
      const std::string& name,
      const std::vector<const base::Value*>& values) override;

  ntp_tiles::PopularSitesInternalsMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(IOSPopularSitesInternalsMessageHandlerBridge);
};

void IOSPopularSitesInternalsMessageHandlerBridge::RegisterMessages() {
  handler_.RegisterMessages();
}

std::unique_ptr<ntp_tiles::PopularSites>
IOSPopularSitesInternalsMessageHandlerBridge::MakePopularSites() {
  return IOSPopularSitesFactory::NewForBrowserState(
      ios::ChromeBrowserState::FromWebUIIOS(web_ui()));
}

PrefService* IOSPopularSitesInternalsMessageHandlerBridge::GetPrefs() {
  return ios::ChromeBrowserState::FromWebUIIOS(web_ui())->GetPrefs();
}

void IOSPopularSitesInternalsMessageHandlerBridge::RegisterMessageCallback(
    const std::string& message,
    const base::Callback<void(const base::ListValue*)>& callback) {
  web_ui()->RegisterMessageCallback(message, callback);
}

void IOSPopularSitesInternalsMessageHandlerBridge::CallJavascriptFunctionVector(
    const std::string& name,
    const std::vector<const base::Value*>& values) {
  web_ui()->CallJavascriptFunction(name, values);
}

}  // namespace

web::WebUIIOSDataSource* CreatePopularSitesInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIPopularSitesInternalsHost);

  source->AddResourcePath("popular_sites_internals.js",
                          IDR_POPULAR_SITES_INTERNALS_JS);
  source->AddResourcePath("popular_sites_internals.css",
                          IDR_POPULAR_SITES_INTERNALS_CSS);
  source->SetDefaultResource(IDR_POPULAR_SITES_INTERNALS_HTML);
  return source;
}

PopularSitesInternalsUI::PopularSitesInternalsUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreatePopularSitesInternalsHTMLSource());
  web_ui->AddMessageHandler(
      base::MakeUnique<IOSPopularSitesInternalsMessageHandlerBridge>());
}

PopularSitesInternalsUI::~PopularSitesInternalsUI() {}
