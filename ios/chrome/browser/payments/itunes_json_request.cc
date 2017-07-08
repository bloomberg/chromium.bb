// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/itunes_json_request.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace payment_request_util {

const char kBadResponse[] = "Bad iTunes Store search response";
const char kITunesStoreLookupPrefix[] = "https://itunes.apple.com/lookup?";
const char kITunesStoreSearchPrefix[] = "https://itunes.apple.com/search?";

ITunesJsonRequest::ITunesJsonRequest(
    const Callback& callback,
    net::URLRequestContextGetter* context_getter)
    : callback_(callback),
      context_getter_(context_getter),
      weak_factory_(this) {
  DCHECK(!callback_.is_null());
}

ITunesJsonRequest::~ITunesJsonRequest() {}

void ITunesJsonRequest::Start(ITunesStoreRequestType request_type,
                              const std::string& request_query) {
  Stop();

  std::string complete_query;
  switch (request_type) {
    case LOOKUP:
      complete_query = kITunesStoreLookupPrefix + request_query;
    case SEARCH:
      complete_query = kITunesStoreSearchPrefix + request_query;
  }

  fetcher_ =
      net::URLFetcher::Create(GURL(complete_query), net::URLFetcher::GET, this);
  fetcher_->SetRequestContext(context_getter_);
  fetcher_->SetLoadFlags(net::LOAD_MAYBE_USER_GESTURE |
                         net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_VERIFY_EV_CERT);
  fetcher_->Start();
}

void ITunesJsonRequest::Stop() {
  fetcher_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void ITunesJsonRequest::ParseJson(
    const std::string& json,
    const payment_request_util::ITunesJsonRequest::SuccessCallback&
        success_callback,
    const payment_request_util::ITunesJsonRequest::ErrorCallback&
        error_callback) {
  DCHECK(!success_callback.is_null());
  DCHECK(!error_callback.is_null());

  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    success_callback.Run(std::move(value));
  } else {
    error_callback.Run(json_reader.GetErrorMessage());
  }
}

void ITunesJsonRequest::OnJsonParseSuccess(
    std::unique_ptr<base::Value> parsed_json) {
  if (!parsed_json->IsType(base::Value::Type::DICTIONARY)) {
    OnJsonParseError(kBadResponse);
    return;
  }

  callback_.Run(base::WrapUnique(
      static_cast<base::DictionaryValue*>(parsed_json.release())));
}

void ITunesJsonRequest::OnJsonParseError(const std::string& error) {
  callback_.Run(std::unique_ptr<base::DictionaryValue>());
}

void ITunesJsonRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK_EQ(fetcher_.get(), source);

  std::unique_ptr<net::URLFetcher> fetcher(std::move(fetcher_));

  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() != net::HTTP_OK) {
    OnJsonParseError(kBadResponse);
    return;
  }

  std::string json_data;
  fetcher->GetResponseAsString(&json_data);

  // The parser will call us back via one of the callbacks.
  ParseJson(json_data,
            base::Bind(&ITunesJsonRequest::OnJsonParseSuccess,
                       weak_factory_.GetWeakPtr()),
            base::Bind(&ITunesJsonRequest::OnJsonParseError,
                       weak_factory_.GetWeakPtr()));
}

}  // namespace payment_request_util
