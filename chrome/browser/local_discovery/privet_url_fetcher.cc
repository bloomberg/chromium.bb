// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_url_fetcher.h"

#include <algorithm>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

namespace local_discovery {

namespace {

typedef std::map<std::string, std::string> TokenMap;

struct TokenMapHolder {
 public:
  static TokenMapHolder* GetInstance() {
    return Singleton<TokenMapHolder>::get();
  }

  TokenMap map;
};

const char kXPrivetTokenHeaderPrefix[] = "X-Privet-Token: ";
const char kRangeHeaderFormat[] = "Range: bytes=%d-%d";
const char kXPrivetEmptyToken[] = "\"\"";
const int kPrivetMaxRetries = 20;
const int kPrivetTimeoutOnError = 5;
const int kHTTPErrorCodeInvalidXPrivetToken = 418;

std::string MakeRangeHeader(int start, int end) {
  DCHECK_GE(start, 0);
  DCHECK_GT(end, 0);
  DCHECK_GT(end, start);
  return base::StringPrintf(kRangeHeaderFormat, start, end);
}

}  // namespace

void PrivetURLFetcher::Delegate::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const TokenCallback& callback) {
  OnError(fetcher, TOKEN_ERROR);
}

bool PrivetURLFetcher::Delegate::OnRawData(PrivetURLFetcher* fetcher,
                                           bool response_is_file,
                                           const std::string& data_string,
                                           const base::FilePath& data_file) {
  return false;
}

PrivetURLFetcher::PrivetURLFetcher(
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    net::URLRequestContextGetter* request_context,
    PrivetURLFetcher::Delegate* delegate)
    : url_(url),
      request_type_(request_type),
      request_context_(request_context),
      delegate_(delegate),
      do_not_retry_on_transient_error_(false),
      send_empty_privet_token_(false),
      has_byte_range_(false),
      make_response_file_(false),
      byte_range_start_(0),
      byte_range_end_(0),
      tries_(0),
      weak_factory_(this) {}

PrivetURLFetcher::~PrivetURLFetcher() {
}

// static
void PrivetURLFetcher::SetTokenForHost(const std::string& host,
                                       const std::string& token) {
  TokenMapHolder::GetInstance()->map[host] = token;
}

// static
void PrivetURLFetcher::ResetTokenMapForTests() {
  TokenMapHolder::GetInstance()->map.clear();
}

void PrivetURLFetcher::DoNotRetryOnTransientError() {
  DCHECK(tries_ == 0);
  do_not_retry_on_transient_error_ = true;
}

void PrivetURLFetcher::SendEmptyPrivetToken() {
  DCHECK(tries_ == 0);
  send_empty_privet_token_ = true;
}

std::string PrivetURLFetcher::GetPrivetAccessToken() {
  if (send_empty_privet_token_) {
    return std::string();
  }

  TokenMapHolder* token_map_holder = TokenMapHolder::GetInstance();
  TokenMap::iterator found = token_map_holder->map.find(GetHostString());
  return found != token_map_holder->map.end() ? found->second : std::string();
}

std::string PrivetURLFetcher::GetHostString() {
  return url_.GetOrigin().spec();
}

void PrivetURLFetcher::SaveResponseToFile() {
  DCHECK(tries_ == 0);
  make_response_file_ = true;
}

void PrivetURLFetcher::SetByteRange(int start, int end) {
  DCHECK(tries_ == 0);
  byte_range_start_ = start;
  byte_range_end_ = end;
  has_byte_range_ = true;
}

void PrivetURLFetcher::Try() {
  tries_++;
  if (tries_ < kPrivetMaxRetries) {
    std::string token = GetPrivetAccessToken();

    if (token.empty())
      token = kXPrivetEmptyToken;

    url_fetcher_.reset(net::URLFetcher::Create(url_, request_type_, this));
    url_fetcher_->SetRequestContext(request_context_);
    url_fetcher_->AddExtraRequestHeader(std::string(kXPrivetTokenHeaderPrefix) +
                                        token);
    if (has_byte_range_) {
      url_fetcher_->AddExtraRequestHeader(
          MakeRangeHeader(byte_range_start_, byte_range_end_));
    }

    if (make_response_file_) {
      url_fetcher_->SaveResponseToTemporaryFile(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::FILE));
    }

    // URLFetcher requires us to set upload data for POST requests.
    if (request_type_ == net::URLFetcher::POST) {
      if (!upload_file_path_.empty()) {
        url_fetcher_->SetUploadFilePath(
            upload_content_type_,
            upload_file_path_,
            0 /*offset*/,
            kuint64max /*length*/,
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::FILE));
      } else {
        url_fetcher_->SetUploadData(upload_content_type_, upload_data_);
      }
    }

    url_fetcher_->Start();
  } else {
    delegate_->OnError(this, RETRY_ERROR);
  }
}

void PrivetURLFetcher::Start() {
  DCHECK_EQ(tries_, 0);  // We haven't called |Start()| yet.

  if (!send_empty_privet_token_) {
    std::string privet_access_token;
    privet_access_token = GetPrivetAccessToken();
    if (privet_access_token.empty()) {
      RequestTokenRefresh();
      return;
    }
  }

  Try();
}

void PrivetURLFetcher::SetUploadData(const std::string& upload_content_type,
                                     const std::string& upload_data) {
  DCHECK(upload_file_path_.empty());
  upload_content_type_ = upload_content_type;
  upload_data_ = upload_data;
}

void PrivetURLFetcher::SetUploadFilePath(
    const std::string& upload_content_type,
    const base::FilePath& upload_file_path) {
  DCHECK(upload_data_.empty());
  upload_content_type_ = upload_content_type;
  upload_file_path_ = upload_file_path;
}

void PrivetURLFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source->GetResponseCode() == net::HTTP_SERVICE_UNAVAILABLE) {
    ScheduleRetry(kPrivetTimeoutOnError);
    return;
  }

  if (!OnURLFetchCompleteDoNotParseData(source)) {
    // Byte ranges should only be used when we're not parsing the data
    // as JSON.
    DCHECK(!has_byte_range_);

    // We should only be saving raw data to a file.
    DCHECK(!make_response_file_);

    OnURLFetchCompleteParseData(source);
  }
}

// Note that this function returns "true" in error cases to indicate
// that it has fully handled the responses.
bool PrivetURLFetcher::OnURLFetchCompleteDoNotParseData(
    const net::URLFetcher* source) {
  if (source->GetResponseCode() == kHTTPErrorCodeInvalidXPrivetToken) {
    RequestTokenRefresh();
    return true;
  }

  if (source->GetResponseCode() != net::HTTP_OK &&
      source->GetResponseCode() != net::HTTP_PARTIAL_CONTENT) {
    delegate_->OnError(this, RESPONSE_CODE_ERROR);
    return true;
  }

  if (make_response_file_) {
    base::FilePath response_file_path;

    if (!source->GetResponseAsFilePath(true, &response_file_path)) {
      delegate_->OnError(this, URL_FETCH_ERROR);
      return true;
    }

    return delegate_->OnRawData(this, true, std::string(), response_file_path);
  } else {
    std::string response_str;

    if (!source->GetResponseAsString(&response_str)) {
      delegate_->OnError(this, URL_FETCH_ERROR);
      return true;
    }

    return delegate_->OnRawData(this, false, response_str, base::FilePath());
  }
}

void PrivetURLFetcher::OnURLFetchCompleteParseData(
    const net::URLFetcher* source) {
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

  const base::DictionaryValue* dictionary_value = NULL;

  if (!value->GetAsDictionary(&dictionary_value)) {
    delegate_->OnError(this, JSON_PARSE_ERROR);
    return;
  }

  std::string error;
  if (dictionary_value->GetString(kPrivetKeyError, &error)) {
    if (error == kPrivetErrorInvalidXPrivetToken) {
      RequestTokenRefresh();
      return;
    } else if (PrivetErrorTransient(error)) {
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

  delegate_->OnParsedJson(
      this, *dictionary_value, dictionary_value->HasKey(kPrivetKeyError));
}

void PrivetURLFetcher::ScheduleRetry(int timeout_seconds) {
  double random_scaling_factor =
      1 + base::RandDouble() * kPrivetMaximumTimeRandomAddition;

  int timeout_seconds_randomized =
      static_cast<int>(timeout_seconds * random_scaling_factor);

  timeout_seconds_randomized =
      std::max(timeout_seconds_randomized, kPrivetMinimumTimeout);

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
    SetTokenForHost(GetHostString(), token);
    Try();
  }
}

bool PrivetURLFetcher::PrivetErrorTransient(const std::string& error) {
  return (error == kPrivetErrorDeviceBusy) ||
         (error == kPrivetV3ErrorDeviceBusy) ||
         (error == kPrivetErrorPendingUserAction) ||
         (error == kPrivetErrorPrinterBusy);
}

}  // namespace local_discovery
