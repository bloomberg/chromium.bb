// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_downloader.h"

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
    const GURL& method_name,
    Delegate* delegate)
    : context_(context),
      method_name_(method_name),
      delegate_(delegate),
      is_downloading_http_link_header_(true) {
  DCHECK(IsValidManifestUrl(method_name_));
}

PaymentManifestDownloader::~PaymentManifestDownloader() {}

void PaymentManifestDownloader::Download() {
  InitiateDownload(method_name_, net::URLFetcher::HEAD);
}

void PaymentManifestDownloader::InitiateDownload(
    const GURL& url,
    net::URLFetcher::RequestType request_type) {
  if (!IsValidManifestUrl(url)) {
    delegate_->OnManifestDownloadFailure();
    return;
  }

  fetcher_ = net::URLFetcher::Create(0 /* id */, url, request_type, this);
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
      std::string manifest_url;
      std::unordered_map<std::string, base::Optional<std::string>> params;
      for (const auto& value : link_header_util::SplitLinkHeader(link_header)) {
        if (!link_header_util::ParseLinkHeaderValue(value.first, value.second,
                                                    &manifest_url, &params)) {
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
          InitiateDownload(method_name_.Resolve(manifest_url),
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
