// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_manifest_downloader.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
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

bool IsValidManifestUrl(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme);
}

}  // namespace

PaymentManifestDownloader::PaymentManifestDownloader(
    const scoped_refptr<net::URLRequestContextGetter>& context,
    const GURL& url,
    Delegate* delegate)
    : context_(context),
      url_(url),
      delegate_(delegate),
      is_downloading_http_link_header_(true) {
  DCHECK(IsValidManifestUrl(url_));
}

PaymentManifestDownloader::~PaymentManifestDownloader() {}

void PaymentManifestDownloader::DownloadPaymentMethodManifest() {
  DCHECK(!fetcher_);
  is_downloading_http_link_header_ = true;
  InitiateDownload(url_, net::URLFetcher::HEAD);
}

void PaymentManifestDownloader::DownloadWebAppManifest() {
  DCHECK(!fetcher_);
  is_downloading_http_link_header_ = false;
  InitiateDownload(url_, net::URLFetcher::GET);
}

void PaymentManifestDownloader::InitiateDownload(
    const GURL& url,
    net::URLFetcher::RequestType request_type) {
  if (!IsValidManifestUrl(url)) {
    delegate_->OnManifestDownloadFailure();
    return;
  }

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
          cookies_allowed: false
          setting:
            "This feature cannot be disabled in settings. Users can uninstall/"
            "disable all payment apps to stop this feature."
          policy_exception_justification: "Not implemented."
        })");
  fetcher_ = net::URLFetcher::Create(0 /* id */, url, request_type, this,
                                     traffic_annotation);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher_.get(), data_use_measurement::DataUseUserData::PAYMENTS);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetStopOnRedirect(true);
  fetcher_->SetRequestContext(context_.get());
  fetcher_->Start();
}

void PaymentManifestDownloader::OnURLFetchComplete(
    const net::URLFetcher* source) {
  if (source->GetResponseCode() != net::HTTP_OK) {
    delegate_->OnManifestDownloadFailure();
    return;
  }

  if (is_downloading_http_link_header_) {
    is_downloading_http_link_header_ = false;

    net::HttpResponseHeaders* headers = source->GetResponseHeaders();
    if (!headers) {
      delegate_->OnManifestDownloadFailure();
      return;
    }

    std::string link_header;
    headers->GetNormalizedHeader("link", &link_header);
    if (!link_header.empty()) {
      std::string payment_method_manifest_url;
      std::unordered_map<std::string, base::Optional<std::string>> params;
      for (const auto& value : link_header_util::SplitLinkHeader(link_header)) {
        if (!link_header_util::ParseLinkHeaderValue(
                value.first, value.second, &payment_method_manifest_url,
                &params)) {
          continue;
        }

        auto rel = params.find("rel");
        if (rel == params.end())
          continue;

        std::vector<std::string> rel_parts =
            base::SplitString(rel->second.value_or(""), HTTP_LWS,
                              base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        if (std::find(rel_parts.begin(), rel_parts.end(),
                      "payment-method-manifest") != rel_parts.end()) {
          InitiateDownload(url_.Resolve(payment_method_manifest_url),
                           net::URLFetcher::GET);
          return;
        }
      }
    }
  } else {
    std::string content;
    if (source->GetResponseAsString(&content) && !content.empty()) {
      delegate_->OnManifestDownloadSuccess(content);
      return;
    }
  }

  delegate_->OnManifestDownloadFailure();
}

}  // namespace payments
