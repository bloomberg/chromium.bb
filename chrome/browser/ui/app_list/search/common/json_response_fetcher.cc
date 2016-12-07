// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/json_response_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/safe_json/safe_json_parser.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace app_list {

const char kBadResponse[] = "Bad Web Service search response";

JSONResponseFetcher::JSONResponseFetcher(
    const Callback& callback,
    net::URLRequestContextGetter* context_getter)
    : callback_(callback),
      context_getter_(context_getter),
      weak_factory_(this) {
  DCHECK(!callback_.is_null());
}

JSONResponseFetcher::~JSONResponseFetcher() {}

void JSONResponseFetcher::Start(const GURL& query_url) {
  Stop();

  fetcher_ = net::URLFetcher::Create(query_url, net::URLFetcher::GET, this);
  fetcher_->SetRequestContext(context_getter_);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_DISABLE_CACHE);
  fetcher_->Start();
}

void JSONResponseFetcher::Stop() {
  fetcher_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void JSONResponseFetcher::OnJsonParseSuccess(
    std::unique_ptr<base::Value> parsed_json) {
  if (!parsed_json->IsType(base::Value::Type::DICTIONARY)) {
    OnJsonParseError(kBadResponse);
    return;
  }

  callback_.Run(base::WrapUnique(
      static_cast<base::DictionaryValue*>(parsed_json.release())));
}

void JSONResponseFetcher::OnJsonParseError(const std::string& error) {
  callback_.Run(std::unique_ptr<base::DictionaryValue>());
}

void JSONResponseFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  CHECK_EQ(fetcher_.get(), source);

  std::unique_ptr<net::URLFetcher> fetcher(std::move(fetcher_));

  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() != 200) {
    OnJsonParseError(kBadResponse);
    return;
  }

  std::string json_data;
  fetcher->GetResponseAsString(&json_data);

  // The parser will call us back via one of the callbacks.
  safe_json::SafeJsonParser::Parse(
      json_data, base::Bind(&JSONResponseFetcher::OnJsonParseSuccess,
                            weak_factory_.GetWeakPtr()),
      base::Bind(&JSONResponseFetcher::OnJsonParseError,
                 weak_factory_.GetWeakPtr()));
}

}  // namespace app_list
