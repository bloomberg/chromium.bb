// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_tiles_internals_ui.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/grit/components_resources.h"
#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/image_fetcher_impl.h"
#include "components/ntp_tiles/field_trial.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/ntp/popular_sites.h"
#endif

namespace {

// The implementation for the chrome://ntp-tiles-internals page.
class ChromeNTPTilesInternalsMessageHandlerClient
    : public content::WebUIMessageHandler,
      public ntp_tiles::NTPTilesInternalsMessageHandlerClient {
 public:
  ChromeNTPTilesInternalsMessageHandlerClient() {}

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // ntp_tiles::NTPTilesInternalsMessageHandlerClient
  bool DoesSourceExist(ntp_tiles::NTPTileSource source) override;
  std::unique_ptr<ntp_tiles::MostVisitedSites> MakeMostVisitedSites() override;
  std::unique_ptr<ntp_tiles::PopularSites> MakePopularSites() override;
  PrefService* GetPrefs() override;
  void RegisterMessageCallback(
      const std::string& message,
      const base::Callback<void(const base::ListValue*)>& callback) override;
  void CallJavascriptFunctionVector(
      const std::string& name,
      const std::vector<const base::Value*>& values) override;

  ntp_tiles::NTPTilesInternalsMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNTPTilesInternalsMessageHandlerClient);
};

void ChromeNTPTilesInternalsMessageHandlerClient::RegisterMessages() {
  handler_.RegisterMessages(this);
}

bool ChromeNTPTilesInternalsMessageHandlerClient::DoesSourceExist(
    ntp_tiles::NTPTileSource source) {
  switch (source) {
    case ntp_tiles::NTPTileSource::TOP_SITES:
    case ntp_tiles::NTPTileSource::SUGGESTIONS_SERVICE:
      return true;
#if defined(OS_ANDROID)
    case ntp_tiles::NTPTileSource::POPULAR:
    case ntp_tiles::NTPTileSource::WHITELIST:
      return true;
#else
    case ntp_tiles::NTPTileSource::POPULAR:
      return false;
    case ntp_tiles::NTPTileSource::WHITELIST:
      // TODO(sfiera): support WHITELIST.
      return false;
#endif
  }
  NOTREACHED();
  return false;
}

std::unique_ptr<ntp_tiles::MostVisitedSites>
ChromeNTPTilesInternalsMessageHandlerClient::MakeMostVisitedSites() {
  // TODO(sfiera): share with Android and Instant in a factory.
  auto* profile = Profile::FromWebUI(web_ui());
  return base::MakeUnique<ntp_tiles::MostVisitedSites>(
      GetPrefs(), TopSitesFactory::GetForProfile(profile),
      suggestions::SuggestionsServiceFactory::GetForProfile(profile),
      MakePopularSites(),
      base::MakeUnique<ntp_tiles::IconCacher>(
          FaviconServiceFactory::GetForProfile(
              profile, ServiceAccessType::IMPLICIT_ACCESS),
          base::MakeUnique<image_fetcher::ImageFetcherImpl>(
              base::MakeUnique<suggestions::ImageDecoderImpl>(),
              profile->GetRequestContext())),
      /*supervisor=*/nullptr);
}

std::unique_ptr<ntp_tiles::PopularSites>
ChromeNTPTilesInternalsMessageHandlerClient::MakePopularSites() {
#if defined(OS_ANDROID)
  return ChromePopularSites::NewForProfile(Profile::FromWebUI(web_ui()));
#else
  return nullptr;
#endif
}

PrefService* ChromeNTPTilesInternalsMessageHandlerClient::GetPrefs() {
  return Profile::FromWebUI(web_ui())->GetPrefs();
}

void ChromeNTPTilesInternalsMessageHandlerClient::RegisterMessageCallback(
    const std::string& message,
    const base::Callback<void(const base::ListValue*)>& callback) {
  web_ui()->RegisterMessageCallback(message, callback);
}

void ChromeNTPTilesInternalsMessageHandlerClient::CallJavascriptFunctionVector(
    const std::string& name,
    const std::vector<const base::Value*>& values) {
  web_ui()->CallJavascriptFunctionUnsafe(name, values);
}

content::WebUIDataSource* CreateNTPTilesInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINTPTilesInternalsHost);

  source->AddResourcePath("ntp_tiles_internals.js", IDR_NTP_TILES_INTERNALS_JS);
  source->AddResourcePath("ntp_tiles_internals.css",
                          IDR_NTP_TILES_INTERNALS_CSS);
  source->SetDefaultResource(IDR_NTP_TILES_INTERNALS_HTML);
  return source;
}

}  // namespace

NTPTilesInternalsUI::NTPTilesInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreateNTPTilesInternalsHTMLSource());
  web_ui->AddMessageHandler(new ChromeNTPTilesInternalsMessageHandlerClient);
}

NTPTilesInternalsUI::~NTPTilesInternalsUI() {}
