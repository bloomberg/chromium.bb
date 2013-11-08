// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_url_fetcher.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

namespace local_discovery {

namespace {
const char kXPrivetTokenHeaderPrefix[] = "X-Privet-Token: ";
const char kXPrivetEmptyToken[] = "\"\"";
const int kPrivetMaxRetries = 20;
const int kPrivetTimeoutOnError = 5;
}

void PrivetURLFetcher::Delegate::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const TokenCallback& callback) {
  OnError(fetcher, TOKEN_ERROR);
}

PrivetURLFetcher::PrivetURLFetcher(
    const std::string& token,
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    net::URLRequestContextGetter* request_context,
    PrivetURLFetcher::Delegate* delegate)
    : privet_access_token_(token), url_(url), request_type_(request_type),
      request_context_(request_context), delegate_(delegate),
      do_not_retry_on_transient_error_(false), allow_empty_privet_token_(false),
      tries_(0), weak_factory_(this) {
}

PrivetURLFetcher::~PrivetURLFetcher() {
}

void PrivetURLFetcher::DoNotRetryOnTransientError() {
  do_not_retry_on_transient_error_ = true;
}

void PrivetURLFetcher::AllowEmptyPrivetToken() {
  allow_empty_privet_token_ = true;
}

void PrivetURLFetcher::Try() {
  tries_++;
  if (tries_ < kPrivetMaxRetries) {
    std::string token = privet_access_token_;

    if (token.empty())
      token = kXPrivetEmptyToken;

    url_fetcher_.reset(net::URLFetcher::Create(url_, request_type_, this));
    url_fetcher_->SetRequestContext(request_context_);
    url_fetcher_->AddExtraRequestHeader(std::string(kXPrivetTokenHeaderPrefix) +
                                        token);

    // URLFetcher requires us to set upload data for POST requests.
    if (request_type_ == net::URLFetcher::POST)
      url_fetcher_->SetUploadData(upload_content_type_, upload_data_);

    url_fetcher_->Start();
  } else {
    delegate_->OnError(this, RETRY_ERROR);
  }
}

void PrivetURLFetcher::Start() {
  DCHECK_EQ(tries_, 0);  // We haven't called |Start()| yet.

  if (privet_access_token_.empty() && !allow_empty_privet_token_) {
    RequestTokenRefresh();
  } else {
    Try();
  }
}

void PrivetURLFetcher::SetUploadData(const std::string& upload_content_type,
                                     const std::string& upload_data) {
  upload_content_type_ = upload_content_type;
  upload_data_ = upload_data;
}

void PrivetURLFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      source->GetResponseCode() == net::HTTP_SERVICE_UNAVAILABLE) {
    ScheduleRetry(kPrivetTimeoutOnError);
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

  std::string error;
  if (dictionary_value->GetString(kPrivetKeyError, &error)) {
    if (error == kPrivetErrorInvalidXPrivetToken) {
      RequestTokenRefresh();
      return;
    } else if (PrivetErrorTransient(error) ||
        dictionary_value->HasKey(kPrivetKeyTimeout)) {
      if (!do_not_retry_on_transient_error_) {
        int timeout_seconds;
        if (!dictionary_value->GetInteger(kPrivetKeyTimeout,
                                          &timeout_seconds)) {
          timeout_seconds = kPrivetDefaultTimeout;
        }

        ScheduleRetry(timeout_seconds);
        return;
      }
    }
  }

  delegate_->OnParsedJson(this, dictionary_value,
                          dictionary_value->HasKey(kPrivetKeyError));
}

void PrivetURLFetcher::ScheduleRetry(int timeout_seconds) {
  double random_scaling_factor =
      1 + base::RandDouble() * kPrivetMaximumTimeRandomAddition;

  int timeout_seconds_randomized =
      static_cast<int>(timeout_seconds * random_scaling_factor);

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PrivetURLFetcher::Try, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(timeout_seconds_randomized));
}

void PrivetURLFetcher::RequestTokenRefresh() {
  delegate_->OnNeedPrivetToken(
      this,
      base::Bind(&PrivetURLFetcher::RefreshToken, weak_factory_.GetWeakPtr()));
}

void PrivetURLFetcher::RefreshToken(const std::string& token) {
  if (token.empty()) {
    delegate_->OnError(this, TOKEN_ERROR);
  } else {
    privet_access_token_ = token;
    Try();
  }
}

bool PrivetURLFetcher::PrivetErrorTransient(const std::string& error) {
  return (error == kPrivetErrorDeviceBusy) ||
      (error == kPrivetErrorPendingUserAction);
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
      new PrivetURLFetcher(token_, url, request_type, request_context_.get(),
                           delegate));
}

}  // namespace local_discovery
