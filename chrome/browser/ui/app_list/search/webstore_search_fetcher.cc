// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/webstore_search_fetcher.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/safe_json_parser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace app_list {

const char kBadResponse[] = "Bad Web Store search response";

WebstoreSearchFetcher::WebstoreSearchFetcher(
    const Callback& callback,
    net::URLRequestContextGetter* context_getter)
    : callback_(callback),
      context_getter_(context_getter),
      weak_factory_(this) {
  DCHECK(!callback_.is_null());
}

WebstoreSearchFetcher::~WebstoreSearchFetcher() {}

void WebstoreSearchFetcher::Start(const std::string& query,
                                  const std::string& hl) {
  Stop();

  fetcher_.reset(net::URLFetcher::Create(
      extension_urls::GetWebstoreJsonSearchUrl(query, hl),
      net::URLFetcher::GET,
      this));
  fetcher_->SetRequestContext(context_getter_);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                         net::LOAD_DISABLE_CACHE);
  fetcher_->Start();
}

void WebstoreSearchFetcher::Stop() {
  fetcher_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void WebstoreSearchFetcher::OnJsonParseSuccess(
    scoped_ptr<base::Value> parsed_json) {
  if (!parsed_json->IsType(base::Value::TYPE_DICTIONARY)) {
    OnJsonParseError(kBadResponse);
    return;
  }

  callback_.Run(make_scoped_ptr(
      static_cast<base::DictionaryValue*>(parsed_json.release())));
}

void WebstoreSearchFetcher::OnJsonParseError(const std::string& error) {
  callback_.Run(scoped_ptr<base::DictionaryValue>());
}

void WebstoreSearchFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  CHECK_EQ(fetcher_.get(), source);

  scoped_ptr<net::URLFetcher> fetcher(fetcher_.Pass());

  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() != 200) {
    OnJsonParseError(kBadResponse);
    return;
  }

  std::string webstore_json_data;
  fetcher->GetResponseAsString(&webstore_json_data);

  scoped_refptr<SafeJsonParser> parser =
      new SafeJsonParser(webstore_json_data,
                         base::Bind(&WebstoreSearchFetcher::OnJsonParseSuccess,
                                    weak_factory_.GetWeakPtr()),
                         base::Bind(&WebstoreSearchFetcher::OnJsonParseError,
                                    weak_factory_.GetWeakPtr()));
  // The parser will call us back via one of the callbacks.
  parser->Start();
}

}  // namespace app_list
