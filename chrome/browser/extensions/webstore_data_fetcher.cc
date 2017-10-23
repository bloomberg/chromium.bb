// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_data_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/extension_urls.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"

namespace {

const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";

}  // namespace

namespace extensions {

WebstoreDataFetcher::WebstoreDataFetcher(
    WebstoreDataFetcherDelegate* delegate,
    net::URLRequestContextGetter* request_context,
    const GURL& referrer_url,
    const std::string webstore_item_id)
    : delegate_(delegate),
      request_context_(request_context),
      referrer_url_(referrer_url),
      id_(webstore_item_id),
      max_auto_retries_(0) {
}

WebstoreDataFetcher::~WebstoreDataFetcher() {}

void WebstoreDataFetcher::SetJsonPostData(const std::string& json) {
  json_post_data_ = json;
}

void WebstoreDataFetcher::Start() {
  GURL webstore_data_url(extension_urls::GetWebstoreItemJsonDataURL(id_));
  net::URLFetcher::RequestType request_type =
      json_post_data_.empty() ? net::URLFetcher::GET : net::URLFetcher::POST;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("webstore_data_fetcher", R"(
        semantics {
          sender: "Webstore Data Fetcher"
          description:
            "Fetches metadata about an extension from the Chrome Web Store."
          trigger:
            "The user or another program triggers some action where Chrome "
            "will show metadata about an extension. This includes extension "
            "installation flows, triggering an install for a disabled "
            "extension, and an extension being added to Chrome through "
            "third-party sideloading. It also happens when a kiosk app account "
            "whose metadata (app icon, name, required platform version) is not "
            "cached locally is detected in device local accounts list. The "
            "account can be set either by device policy or through extensions "
            "web UI, by the device owner (user that was initially added to the "
            "device; implies non managed device). The latter case is "
            "deprecated and not supported on newer Chrome OS boards."
          data:
            "The extension id and referrer url. The referrer chain is also "
            "included if the user has not opted out of SafeBrowsing."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings. It will only be "
            "triggered if the user uses extensions."
          policy_exception_justification: "Not implemented."
        })");
  webstore_data_url_fetcher_ = net::URLFetcher::Create(
      webstore_data_url, request_type, this, traffic_annotation);
  webstore_data_url_fetcher_->SetRequestContext(request_context_);
  webstore_data_url_fetcher_->SetReferrer(referrer_url_.spec());
  webstore_data_url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                                           net::LOAD_DISABLE_CACHE);

  if (!json_post_data_.empty()) {
    webstore_data_url_fetcher_->SetUploadData("application/json",
                                              json_post_data_);
  }

  if (max_auto_retries_ > 0) {
    webstore_data_url_fetcher_->SetMaxRetriesOn5xx(max_auto_retries_);
    webstore_data_url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(
        max_auto_retries_);
  }
  webstore_data_url_fetcher_->Start();
}

void WebstoreDataFetcher::OnJsonParseSuccess(
    std::unique_ptr<base::Value> parsed_json) {
  if (!parsed_json->is_dict()) {
    OnJsonParseFailure(kInvalidWebstoreResponseError);
    return;
  }

  delegate_->OnWebstoreResponseParseSuccess(
      std::unique_ptr<base::DictionaryValue>(
          static_cast<base::DictionaryValue*>(parsed_json.release())));
}

void WebstoreDataFetcher::OnJsonParseFailure(
    const std::string& error) {
  delegate_->OnWebstoreResponseParseFailure(error);
}

void WebstoreDataFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK_EQ(webstore_data_url_fetcher_.get(), source);

  std::unique_ptr<net::URLFetcher> fetcher(
      std::move(webstore_data_url_fetcher_));

  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() != 200) {
    delegate_->OnWebstoreRequestFailure();
    return;
  }

  std::string webstore_json_data;
  fetcher->GetResponseAsString(&webstore_json_data);

  // The parser will call us back via one of the callbacks.
  data_decoder::SafeJsonParser::Parse(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      webstore_json_data,
      base::Bind(&WebstoreDataFetcher::OnJsonParseSuccess, AsWeakPtr()),
      base::Bind(&WebstoreDataFetcher::OnJsonParseFailure, AsWeakPtr()));
}

}  // namespace extensions
