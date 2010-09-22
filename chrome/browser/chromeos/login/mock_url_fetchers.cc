// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_url_fetchers.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

ExpectCanceledFetcher::ExpectCanceledFetcher(
    bool success,
    const GURL& url,
    const std::string& results,
    URLFetcher::RequestType request_type,
    URLFetcher::Delegate* d)
    : URLFetcher(url, request_type, d) {
}

ExpectCanceledFetcher::~ExpectCanceledFetcher() {
  task_->Cancel();
}

void ExpectCanceledFetcher::Start() {
  LOG(INFO) << "Delaying fetch completion in mock";
  task_ = NewRunnableFunction(&ExpectCanceledFetcher::CompleteFetch);
  ChromeThread::PostDelayedTask(ChromeThread::UI,
                                FROM_HERE,
                                task_,
                                100);
}

// static
void ExpectCanceledFetcher::CompleteFetch() {
  ADD_FAILURE() << "Fetch completed in ExpectCanceledFetcher!";
  MessageLoop::current()->Quit();  // Allow exiting even if we mess up.
}

GotCanceledFetcher::GotCanceledFetcher(bool success,
                                       const GURL& url,
                                       const std::string& results,
                                       URLFetcher::RequestType request_type,
                                       URLFetcher::Delegate* d)
    : URLFetcher(url, request_type, d),
      url_(url) {
  LOG(INFO) << url_.spec();
}

GotCanceledFetcher::~GotCanceledFetcher() {}

void GotCanceledFetcher::Start() {
  URLRequestStatus status;
  status.set_status(URLRequestStatus::CANCELED);
  delegate()->OnURLFetchComplete(this,
                                 url_,
                                 status,
                                 403,
                                 ResponseCookies(),
                                 std::string());
}

}  // namespace chromeos
