// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/mock_url_fetchers.h"

#include <errno.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

ExpectCanceledFetcher::ExpectCanceledFetcher(
    bool success,
    const GURL& url,
    const std::string& results,
    net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* d)
    : net::TestURLFetcher(0, url, d),
      weak_factory_(this) {
}

ExpectCanceledFetcher::~ExpectCanceledFetcher() {
}

void ExpectCanceledFetcher::Start() {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExpectCanceledFetcher::CompleteFetch,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(100));
}

void ExpectCanceledFetcher::CompleteFetch() {
  ADD_FAILURE() << "Fetch completed in ExpectCanceledFetcher!";
  base::MessageLoop::current()->Quit();  // Allow exiting even if we mess up.
}

GotCanceledFetcher::GotCanceledFetcher(
    bool success,
    const GURL& url,
    const std::string& results,
    net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* d)
    : net::TestURLFetcher(0, url, d) {
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
                               net::URLFetcher::RequestType request_type,
                               net::URLFetcherDelegate* d)
    : net::TestURLFetcher(0, url, d) {
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
                         net::URLFetcher::RequestType request_type,
                         net::URLFetcherDelegate* d)
    : net::TestURLFetcher(0, url, d) {
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
                               net::URLFetcher::RequestType request_type,
                               net::URLFetcherDelegate* d)
    : net::TestURLFetcher(0, url, d) {
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
                             net::URLFetcher::RequestType request_type,
                             net::URLFetcherDelegate* d)
    : net::TestURLFetcher(0, url, d) {
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
