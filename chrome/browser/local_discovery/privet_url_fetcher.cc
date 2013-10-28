// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_url_fetcher.h"

#include "base/json/json_reader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

namespace local_discovery {

namespace {
const char kXPrivetTokenHeaderPrefix[] = "X-Privet-Token: ";
const char kXPrivetEmptyToken[] = "\"\"";
}

PrivetURLFetcher::PrivetURLFetcher(
    const std::string& token,
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    net::URLRequestContextGetter* request_context,
    PrivetURLFetcher::Delegate* delegate)
    : delegate_(delegate) {
  std::string sent_token = token;
  if (sent_token.empty())
    sent_token = kXPrivetEmptyToken;

  url_fetcher_.reset(net::URLFetcher::Create(url, request_type, this));
  url_fetcher_->SetRequestContext(request_context);
  url_fetcher_->AddExtraRequestHeader(std::string(kXPrivetTokenHeaderPrefix) +
                                      sent_token);

  // URLFetcher requires us to set upload data for POST requests.
  if (request_type == net::URLFetcher::POST)
      url_fetcher_->SetUploadData(std::string(), std::string());
}

PrivetURLFetcher::~PrivetURLFetcher() {
}

void PrivetURLFetcher::Start() {
  url_fetcher_->Start();
}

void PrivetURLFetcher::SetUploadData(const std::string& upload_content_type,
                                     const std::string& upload_data) {
  url_fetcher_->SetUploadData(upload_content_type, upload_data);
}

void PrivetURLFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
    delegate_->OnError(this, URL_FETCH_ERROR);
    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    delegate_->OnError(this, RESPONSE_CODE_ERROR);
    return;
  }

  std::string response_str;

  if (!source->GetResponseAsString(&response_str)) {
    delegate_->OnError(this, URL_FETCH_ERROR);
    return;
  }

  base::JSONReader json_reader(base::JSON_ALLOW_TRAILING_COMMAS);
  scoped_ptr<base::Value> value;

  value.reset(json_reader.ReadToValue(response_str));

  if (!value) {
    delegate_->OnError(this, JSON_PARSE_ERROR);
    return;
  }

  const base::DictionaryValue* dictionary_value;

  if (!value->GetAsDictionary(&dictionary_value)) {
    delegate_->OnError(this, JSON_PARSE_ERROR);
    return;
  }

  delegate_->OnParsedJson(this, dictionary_value,
                          dictionary_value->HasKey(kPrivetKeyError));
}

PrivetURLFetcherFactory::PrivetURLFetcherFactory(
    net::URLRequestContextGetter* request_context)
    : request_context_(request_context) {
}

PrivetURLFetcherFactory::~PrivetURLFetcherFactory() {
}

scoped_ptr<PrivetURLFetcher> PrivetURLFetcherFactory::CreateURLFetcher(
    const GURL& url, net::URLFetcher::RequestType request_type,
    PrivetURLFetcher::Delegate* delegate) const {
  return scoped_ptr<PrivetURLFetcher>(
      new PrivetURLFetcher(token_, url, request_type,
                           request_context_.get(), delegate));
}

}  // namespace local_discovery
