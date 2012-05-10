// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_url_fetchers.h"

#include <errno.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

ExpectCanceledFetcher::ExpectCanceledFetcher(
    bool success,
    const GURL& url,
    const std::string& results,
    content::URLFetcher::RequestType request_type,
    content::URLFetcherDelegate* d)
    : TestURLFetcher(0, url, d),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

ExpectCanceledFetcher::~ExpectCanceledFetcher() {
}

void ExpectCanceledFetcher::Start() {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExpectCanceledFetcher::CompleteFetch,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(100));
}

void ExpectCanceledFetcher::CompleteFetch() {
  ADD_FAILURE() << "Fetch completed in ExpectCanceledFetcher!";
  MessageLoop::current()->Quit();  // Allow exiting even if we mess up.
}

GotCanceledFetcher::GotCanceledFetcher(
    bool success,
    const GURL& url,
    const std::string& results,
    content::URLFetcher::RequestType request_type,
    content::URLFetcherDelegate* d)
    : TestURLFetcher(0, url, d) {
  set_url(url);
  set_status(net::URLRequestStatus(net::URLRequestStatus::CANCELED, 0));
  set_response_code(net::HTTP_FORBIDDEN);
}

GotCanceledFetcher::~GotCanceledFetcher() {}

void GotCanceledFetcher::Start() {
  delegate()->OnURLFetchComplete(this);
}

SuccessFetcher::SuccessFetcher(bool success,
                               const GURL& url,
                               const std::string& results,
                               content::URLFetcher::RequestType request_type,
                               content::URLFetcherDelegate* d)
    : TestURLFetcher(0, url, d) {
  set_url(url);
  set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
  set_response_code(net::HTTP_OK);
}

SuccessFetcher::~SuccessFetcher() {}

void SuccessFetcher::Start() {
  delegate()->OnURLFetchComplete(this);
}

FailFetcher::FailFetcher(bool success,
                         const GURL& url,
                         const std::string& results,
                         content::URLFetcher::RequestType request_type,
                         content::URLFetcherDelegate* d)
    : TestURLFetcher(0, url, d) {
  set_url(url);
  set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED, ECONNRESET));
  set_response_code(net::HTTP_OK);
}

FailFetcher::~FailFetcher() {}

void FailFetcher::Start() {
  delegate()->OnURLFetchComplete(this);
}

// static
const char CaptchaFetcher::kCaptchaToken[] = "token";
// static
const char CaptchaFetcher::kCaptchaUrlBase[] = "http://accounts.google.com/";
// static
const char CaptchaFetcher::kCaptchaUrlFragment[] = "fragment";
// static
const char CaptchaFetcher::kUnlockUrl[] = "http://what.ever";


CaptchaFetcher::CaptchaFetcher(bool success,
                               const GURL& url,
                               const std::string& results,
                               content::URLFetcher::RequestType request_type,
                               content::URLFetcherDelegate* d)
    : TestURLFetcher(0, url, d) {
  set_url(url);
  set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
  set_response_code(net::HTTP_FORBIDDEN);
  SetResponseString(base::StringPrintf("Error=%s\n"
                                       "Url=%s\n"
                                       "CaptchaUrl=%s\n"
                                       "CaptchaToken=%s\n",
                                       "CaptchaRequired",
                                       kUnlockUrl,
                                       kCaptchaUrlFragment,
                                       kCaptchaToken));
}

CaptchaFetcher::~CaptchaFetcher() {}

// static
std::string CaptchaFetcher::GetCaptchaToken() {
  return kCaptchaToken;
}

// static
std::string CaptchaFetcher::GetCaptchaUrl() {
  return std::string(kCaptchaUrlBase).append(kCaptchaUrlFragment);
}

// static
std::string CaptchaFetcher::GetUnlockUrl() {
  return kUnlockUrl;
}

void CaptchaFetcher::Start() {
  delegate()->OnURLFetchComplete(this);
}

HostedFetcher::HostedFetcher(bool success,
                             const GURL& url,
                             const std::string& results,
                             content::URLFetcher::RequestType request_type,
                             content::URLFetcherDelegate* d)
    : TestURLFetcher(0, url, d) {
  set_url(url);
  set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
  set_response_code(net::HTTP_OK);
}

HostedFetcher::~HostedFetcher() {}

void HostedFetcher::Start() {
  VLOG(1) << upload_data();
  if (upload_data().find("HOSTED") == std::string::npos) {
    VLOG(1) << "HostedFetcher failing request";
    set_response_code(net::HTTP_FORBIDDEN);
    SetResponseString("Error=BadAuthentication");
  }
  delegate()->OnURLFetchComplete(this);
}

}  // namespace chromeos
