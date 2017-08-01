// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_manifest_downloader.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/link_header_util/link_header_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/url_constants.h"

namespace payments {
namespace {

GURL ParseResponseHeader(const net::URLFetcher* source) {
  if (source->GetResponseCode() != net::HTTP_OK &&
      source->GetResponseCode() != net::HTTP_NO_CONTENT) {
    return GURL();
  }

  net::HttpResponseHeaders* headers = source->GetResponseHeaders();
  if (!headers)
    return GURL();

  std::string link_header;
  headers->GetNormalizedHeader("link", &link_header);
  if (link_header.empty())
    return GURL();

  std::string payment_method_manifest_url;
  std::unordered_map<std::string, base::Optional<std::string>> params;
  for (const auto& value : link_header_util::SplitLinkHeader(link_header)) {
    if (!link_header_util::ParseLinkHeaderValue(
            value.first, value.second, &payment_method_manifest_url, &params)) {
      continue;
    }

    auto rel = params.find("rel");
    if (rel == params.end())
      continue;

    std::vector<std::string> rel_parts =
        base::SplitString(rel->second.value_or(""), HTTP_LWS,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (base::ContainsValue(rel_parts, "payment-method-manifest"))
      return source->GetOriginalURL().Resolve(payment_method_manifest_url);
  }

  return GURL();
}

std::string ParseResponseContent(const net::URLFetcher* source) {
  std::string content;
  if (source->GetResponseCode() != net::HTTP_OK)
    return content;

  bool success = source->GetResponseAsString(&content);
  DCHECK(success);  // Whether the fetcher was set to store result as string.

  return content;
}

}  // namespace

PaymentManifestDownloader::PaymentManifestDownloader(
    const scoped_refptr<net::URLRequestContextGetter>& context)
    : context_(context), allow_http_for_test_(false) {}

PaymentManifestDownloader::~PaymentManifestDownloader() {}

void PaymentManifestDownloader::DownloadPaymentMethodManifest(
    const GURL& url,
    DownloadCallback callback) {
  DCHECK(IsValidManifestUrl(url));
  InitiateDownload(url, net::URLFetcher::HEAD, std::move(callback));
}

void PaymentManifestDownloader::DownloadWebAppManifest(
    const GURL& url,
    DownloadCallback callback) {
  DCHECK(IsValidManifestUrl(url));
  InitiateDownload(url, net::URLFetcher::GET, std::move(callback));
}

void PaymentManifestDownloader::AllowHttpForTest() {
  allow_http_for_test_ = true;
}

PaymentManifestDownloader::Download::Download() {}

PaymentManifestDownloader::Download::~Download() {}

void PaymentManifestDownloader::OnURLFetchComplete(
    const net::URLFetcher* source) {
  auto download_it = downloads_.find(source);
  DCHECK(download_it != downloads_.end());

  std::unique_ptr<Download> download = std::move(download_it->second);
  downloads_.erase(download_it);

  if (download->request_type == net::URLFetcher::HEAD) {
    GURL url = ParseResponseHeader(source);
    if (IsValidManifestUrl(url)) {
      InitiateDownload(url, net::URLFetcher::GET,
                       std::move(download->callback));
    } else {
      std::move(download->callback).Run(std::string());
    }
  } else {
    std::move(download->callback).Run(ParseResponseContent(source));
  }
}

void PaymentManifestDownloader::InitiateDownload(
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    DownloadCallback callback) {
  DCHECK(IsValidManifestUrl(url));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("payment_manifest_downloader", R"(
        semantics {
          sender: "Web Payments"
          description:
            "Chromium downloads manifest files for web payments API to help "
            "users make secure and convenient payments on the web."
          trigger:
            "A user that has a payment app visits a website that uses the web "
            "payments API."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings. Users can uninstall/"
            "disable all payment apps to stop this feature."
          policy_exception_justification: "Not implemented."
        })");
  std::unique_ptr<net::URLFetcher> fetcher = net::URLFetcher::Create(
      0 /* id */, url, request_type, this, traffic_annotation);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher.get(), data_use_measurement::DataUseUserData::PAYMENTS);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->SetStopOnRedirect(true);
  fetcher->SetRequestContext(context_.get());
  fetcher->Start();

  auto download = base::MakeUnique<Download>();
  download->request_type = request_type;
  download->fetcher = std::move(fetcher);
  download->callback = std::move(callback);

  const net::URLFetcher* identifier = download->fetcher.get();
  auto insert_result =
      downloads_.insert(std::make_pair(identifier, std::move(download)));
  DCHECK(insert_result.second);  // Whether the insert has succeeded.
}

bool PaymentManifestDownloader::IsValidManifestUrl(const GURL& url) {
  return url.is_valid() &&
         (url.SchemeIs(url::kHttpsScheme) ||
          (allow_http_for_test_ && url.SchemeIs(url::kHttpScheme) &&
           url.host() == "127.0.0.1"));
}

}  // namespace payments
