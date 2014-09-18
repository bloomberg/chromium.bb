// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_data_fetcher.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "chrome/browser/safe_json_parser.h"
#include "extensions/common/extension_urls.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

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

void WebstoreDataFetcher::Start() {
  GURL webstore_data_url(extension_urls::GetWebstoreItemJsonDataURL(id_));

  webstore_data_url_fetcher_.reset(net::URLFetcher::Create(
      webstore_data_url, net::URLFetcher::GET, this));
  webstore_data_url_fetcher_->SetRequestContext(request_context_);
  webstore_data_url_fetcher_->SetReferrer(referrer_url_.spec());
  webstore_data_url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                                           net::LOAD_DISABLE_CACHE);
  if (max_auto_retries_ > 0) {
    webstore_data_url_fetcher_->SetMaxRetriesOn5xx(max_auto_retries_);
    webstore_data_url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(
        max_auto_retries_);
  }
  webstore_data_url_fetcher_->Start();
}

void WebstoreDataFetcher::OnJsonParseSuccess(
    scoped_ptr<base::Value> parsed_json) {
  if (!parsed_json->IsType(base::Value::TYPE_DICTIONARY)) {
    OnJsonParseFailure(kInvalidWebstoreResponseError);
    return;
  }

  delegate_->OnWebstoreResponseParseSuccess(scoped_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(parsed_json.release())));
}

void WebstoreDataFetcher::OnJsonParseFailure(
    const std::string& error) {
  delegate_->OnWebstoreResponseParseFailure(error);
}

void WebstoreDataFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK_EQ(webstore_data_url_fetcher_.get(), source);

  scoped_ptr<net::URLFetcher> fetcher(webstore_data_url_fetcher_.Pass());

  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() != 200) {
    delegate_->OnWebstoreRequestFailure();
    return;
  }

  std::string webstore_json_data;
  fetcher->GetResponseAsString(&webstore_json_data);

  scoped_refptr<SafeJsonParser> parser =
      new SafeJsonParser(webstore_json_data,
                         base::Bind(&WebstoreDataFetcher::OnJsonParseSuccess,
                                    AsWeakPtr()),
                         base::Bind(&WebstoreDataFetcher::OnJsonParseFailure,
                                    AsWeakPtr()));
  // The parser will call us back via one of the callbacks.
  parser->Start();
}

}  // namespace extensions
