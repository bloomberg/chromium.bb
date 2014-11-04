// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_url_fetcher.h"

#include "components/translate/core/browser/translate_download_manager.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace translate {

namespace {

// Retry parameter for fetching.
const int kMaxRetry = 16;

}  // namespace

TranslateURLFetcher::TranslateURLFetcher(int id) : id_(id),
                                                   state_(IDLE),
                                                   retry_count_(0) {
}

TranslateURLFetcher::~TranslateURLFetcher() {
}

bool TranslateURLFetcher::Request(
    const GURL& url,
    const TranslateURLFetcher::Callback& callback) {
  // This function is not supposed to be called before previous operaion is not
  // finished.
  if (state_ == REQUESTING) {
    NOTREACHED();
    return false;
  }

  if (retry_count_ >= kMaxRetry)
    return false;
  retry_count_++;

  state_ = REQUESTING;
  url_ = url;
  callback_ = callback;

  fetcher_.reset(net::URLFetcher::Create(
      id_,
      url_,
      net::URLFetcher::GET,
      this));
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetRequestContext(
      TranslateDownloadManager::GetInstance()->request_context());
  // Set retry parameter for HTTP status code 5xx. This doesn't work against
  // 106 (net::ERR_INTERNET_DISCONNECTED) and so on.
  // TranslateLanguageList handles network status, and implements retry.
  fetcher_->SetMaxRetriesOn5xx(max_retry_on_5xx_);
  if (!extra_request_header_.empty())
    fetcher_->SetExtraRequestHeaders(extra_request_header_);

  fetcher_->Start();

  return true;
}

void TranslateURLFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(fetcher_.get() == source);

  std::string data;
  if (source->GetStatus().status() == net::URLRequestStatus::SUCCESS &&
      source->GetResponseCode() == net::HTTP_OK) {
    state_ = COMPLETED;
    source->GetResponseAsString(&data);
  } else {
    state_ = FAILED;
  }

  // Transfer URLFetcher's ownership before invoking a callback.
  scoped_ptr<const net::URLFetcher> delete_ptr(fetcher_.release());
  callback_.Run(id_, state_ == COMPLETED, data);
}

}  // namespace translate
