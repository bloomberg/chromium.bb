// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/popular_sites_internals_ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ntp_tiles/chrome_popular_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/grit/components_resources.h"
#include "components/ntp_tiles/popular_sites.h"
#include "components/ntp_tiles/webui/popular_sites_internals_message_handler.h"
#include "components/ntp_tiles/webui/popular_sites_internals_message_handler_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

// The implementation for the chrome://popular-sites-internals page.
class ChromePopularSitesInternalsMessageHandlerBridge
    : public content::WebUIMessageHandler,
      public ntp_tiles::PopularSitesInternalsMessageHandlerClient {
 public:
  ChromePopularSitesInternalsMessageHandlerBridge() : handler_(this) {}

 private:
  // content::WebUIMessageHandler:
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

  DISALLOW_COPY_AND_ASSIGN(ChromePopularSitesInternalsMessageHandlerBridge);
};

void ChromePopularSitesInternalsMessageHandlerBridge::RegisterMessages() {
  handler_.RegisterMessages();
}

std::unique_ptr<ntp_tiles::PopularSites>
ChromePopularSitesInternalsMessageHandlerBridge::MakePopularSites() {
  return ChromePopularSitesFactory::NewForProfile(Profile::FromWebUI(web_ui()));
}

PrefService* ChromePopularSitesInternalsMessageHandlerBridge::GetPrefs() {
  return Profile::FromWebUI(web_ui())->GetPrefs();
}

void ChromePopularSitesInternalsMessageHandlerBridge::RegisterMessageCallback(
    const std::string& message,
    const base::Callback<void(const base::ListValue*)>& callback) {
  web_ui()->RegisterMessageCallback(message, callback);
}

void ChromePopularSitesInternalsMessageHandlerBridge::
    CallJavascriptFunctionVector(
        const std::string& name,
        const std::vector<const base::Value*>& values) {
  web_ui()->CallJavascriptFunctionUnsafe(name, values);
}

}  // namespace

content::WebUIDataSource* CreatePopularSitesInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIPopularSitesInternalsHost);

  source->AddResourcePath("popular_sites_internals.js",
                          IDR_POPULAR_SITES_INTERNALS_JS);
  source->AddResourcePath("popular_sites_internals.css",
                          IDR_POPULAR_SITES_INTERNALS_CSS);
  source->SetDefaultResource(IDR_POPULAR_SITES_INTERNALS_HTML);
  return source;
}

PopularSitesInternalsUI::PopularSitesInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreatePopularSitesInternalsHTMLSource());
  web_ui->AddMessageHandler(
      base::MakeUnique<ChromePopularSitesInternalsMessageHandlerBridge>());
}

PopularSitesInternalsUI::~PopularSitesInternalsUI() {}
