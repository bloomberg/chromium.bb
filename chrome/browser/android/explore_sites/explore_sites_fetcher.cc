// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_fetcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/explore_sites/catalog.pb.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "chrome/browser/android/explore_sites/url_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_accept_language_settings.h"
#include "chrome/common/channel_info.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace explore_sites {

namespace {

std::string GetCountry() {
  std::string manually_set_variation_country =
      base::GetFieldTrialParamValueByFeature(chrome::android::kExploreSites,
                                             "country_override");
  if (!manually_set_variation_country.empty())
    return manually_set_variation_country;

  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    std::string country = variations_service->GetStoredPermanentCountry();
    if (!country.empty())
      return country;
    country = variations_service->GetLatestCountry();
    if (!country.empty())
      return country;
  }

  return "DEFAULT";
}

// Content type needed in order to communicate with the server in binary
// proto format.
const char kRequestContentType[] = "application/x-protobuf";
const char kRequestMethod[] = "GET";

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("explore_sites", R"(
          semantics {
            sender: "Explore Sites"
            description:
              "Downloads a catalog of categories and sites for the purposes of "
              "exploring the Web."
            trigger:
              "Periodically scheduled for update in the background and also "
              "triggered by New Tab Page UI."
            data:
              "Proto data comprising interesting site and category information."
              " No user information is sent."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
          })");

}  // namespace

std::unique_ptr<ExploreSitesFetcher> ExploreSitesFetcher::CreateForGetCatalog(
    Callback callback,
    const std::string catalog_version,
    const std::string accept_languages,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  GURL url = GetCatalogURL();
  return base::WrapUnique(
      new ExploreSitesFetcher(std::move(callback), url, catalog_version,
                              accept_languages, loader_factory));
}

std::unique_ptr<ExploreSitesFetcher>
ExploreSitesFetcher::CreateForGetCategories(
    Callback callback,
    const std::string catalog_version,
    const std::string accept_languages,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  GURL url = GetCategoriesURL();
  return base::WrapUnique(
      new ExploreSitesFetcher(std::move(callback), url, catalog_version,
                              accept_languages, loader_factory));
}

ExploreSitesFetcher::ExploreSitesFetcher(
    Callback callback,
    const GURL& url,
    const std::string catalog_version,
    const std::string accept_languages,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
    : callback_(std::move(callback)),
      url_loader_factory_(loader_factory),
      weak_factory_(this) {
  base::Version version = version_info::GetVersion();
  std::string channel_name = chrome::GetChannelName();
  std::string client_version =
      base::StringPrintf("%d.%d.%d.%s.chrome",
                         version.components()[0],  // Major
                         version.components()[2],  // Build
                         version.components()[3],  // Patch
                         channel_name.c_str());
  GURL final_url =
      net::AppendOrReplaceQueryParameter(url, "country_code", GetCountry());
  final_url = net::AppendOrReplaceQueryParameter(final_url, "version_token",
                                                 catalog_version);
  DVLOG(1) << "Final URL: " << final_url.spec();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = final_url;
  resource_request->method = kRequestMethod;
  bool is_stable_channel =
      chrome::GetChannel() == version_info::Channel::STABLE;
  std::string api_key = is_stable_channel ? google_apis::GetAPIKey()
                                          : google_apis::GetNonStableAPIKey();
  resource_request->headers.SetHeader("x-goog-api-key", api_key);
  resource_request->headers.SetHeader("X-Client-Version", client_version);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      kRequestContentType);
  if (!accept_languages.empty()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAcceptLanguage, accept_languages);
  }

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ExploreSitesFetcher::OnSimpleLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

ExploreSitesFetcher::~ExploreSitesFetcher() {}

void ExploreSitesFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  ExploreSitesRequestStatus status = HandleResponseCode();

  if (response_body && response_body->empty()) {
    DVLOG(1) << "Failed to get response or empty response";
    status = ExploreSitesRequestStatus::kShouldRetry;
  }

  std::move(callback_).Run(status, std::move(response_body));
}

ExploreSitesRequestStatus ExploreSitesFetcher::HandleResponseCode() {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  if (response_code == -1) {
    int net_error = url_loader_->NetError();
    DVLOG(1) << "Net error: " << net_error;
    return (net_error == net::ERR_BLOCKED_BY_ADMINISTRATOR)
               ? ExploreSitesRequestStatus::kShouldSuspendBlockedByAdministrator
               : ExploreSitesRequestStatus::kShouldRetry;
  } else if (response_code < 200 || response_code > 299) {
    DVLOG(1) << "HTTP status: " << response_code;
    switch (response_code) {
      case net::HTTP_BAD_REQUEST:
        return ExploreSitesRequestStatus::kShouldSuspendBadRequest;
      default:
        return ExploreSitesRequestStatus::kShouldRetry;
    }
  }

  return ExploreSitesRequestStatus::kSuccess;
}

}  // namespace explore_sites
