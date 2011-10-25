// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_url_fetchers.h"

#include <errno.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "chrome/common/net/http_return.h"
#include "content/browser/browser_thread.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

ExpectCanceledFetcher::ExpectCanceledFetcher(
    bool success,
    const GURL& url,
    const std::string& results,
    URLFetcher::RequestType request_type,
    content::URLFetcherDelegate* d)
    : URLFetcher(url, request_type, d),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

ExpectCanceledFetcher::~ExpectCanceledFetcher() {
}

void ExpectCanceledFetcher::Start() {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExpectCanceledFetcher::CompleteFetch,
                 weak_factory_.GetWeakPtr()),
      100);
}

void ExpectCanceledFetcher::CompleteFetch() {
  ADD_FAILURE() << "Fetch completed in ExpectCanceledFetcher!";
  MessageLoop::current()->Quit();  // Allow exiting even if we mess up.
}

GotCanceledFetcher::GotCanceledFetcher(bool success,
                                       const GURL& url,
                                       const std::string& results,
                                       URLFetcher::RequestType request_type,
                                       content::URLFetcherDelegate* d)
    : URLFetcher(url, request_type, d),
      url_(url),
      status_(net::URLRequestStatus::CANCELED, 0) {
}

GotCanceledFetcher::~GotCanceledFetcher() {}

void GotCanceledFetcher::Start() {
  delegate()->OnURLFetchComplete(this);
}

const GURL& GotCanceledFetcher::GetUrl() const {
  return url_;
}

const net::URLRequestStatus& GotCanceledFetcher::GetStatus() const {
  return status_;
}

int GotCanceledFetcher::GetResponseCode() const {
  return RC_FORBIDDEN;
}

SuccessFetcher::SuccessFetcher(bool success,
                               const GURL& url,
                               const std::string& results,
                               URLFetcher::RequestType request_type,
                               content::URLFetcherDelegate* d)
    : URLFetcher(url, request_type, d),
      url_(url),
      status_(net::URLRequestStatus::SUCCESS, 0) {
}

SuccessFetcher::~SuccessFetcher() {}

void SuccessFetcher::Start() {
  delegate()->OnURLFetchComplete(this);
}

const GURL& SuccessFetcher::GetUrl() const {
  return url_;
}

const net::URLRequestStatus& SuccessFetcher::GetStatus() const {
  return status_;
}

int SuccessFetcher::GetResponseCode() const {
  return RC_REQUEST_OK;
}

FailFetcher::FailFetcher(bool success,
                         const GURL& url,
                         const std::string& results,
                         URLFetcher::RequestType request_type,
                         content::URLFetcherDelegate* d)
    : URLFetcher(url, request_type, d),
      url_(url),
      status_(net::URLRequestStatus::FAILED, ECONNRESET) {
}

FailFetcher::~FailFetcher() {}

void FailFetcher::Start() {
  delegate()->OnURLFetchComplete(this);
}

const GURL& FailFetcher::GetUrl() const {
  return url_;
}

const net::URLRequestStatus& FailFetcher::GetStatus() const {
  return status_;
}

int FailFetcher::GetResponseCode() const {
  return RC_REQUEST_OK;
}

// static
const char CaptchaFetcher::kCaptchaToken[] = "token";
// static
const char CaptchaFetcher::kCaptchaUrlBase[] = "http://www.google.com/accounts/";
// static
const char CaptchaFetcher::kCaptchaUrlFragment[] = "fragment";
// static
const char CaptchaFetcher::kUnlockUrl[] = "http://what.ever";


CaptchaFetcher::CaptchaFetcher(bool success,
                               const GURL& url,
                               const std::string& results,
                               URLFetcher::RequestType request_type,
                               content::URLFetcherDelegate* d)
    : URLFetcher(url, request_type, d),
      url_(url),
      status_(net::URLRequestStatus::SUCCESS, 0) {
  data_ = base::StringPrintf("Error=%s\n"
                             "Url=%s\n"
                             "CaptchaUrl=%s\n"
                             "CaptchaToken=%s\n",
                             "CaptchaRequired",
                             kUnlockUrl,
                             kCaptchaUrlFragment,
                             kCaptchaToken);
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

const GURL& CaptchaFetcher::GetUrl() const {
  return url_;
}

const net::URLRequestStatus& CaptchaFetcher::GetStatus() const {
  return status_;
}

int CaptchaFetcher::GetResponseCode() const {
  return RC_FORBIDDEN;
}

bool CaptchaFetcher::GetResponseAsString(
    std::string* out_response_string) const {
  *out_response_string = data_;
  return true;
}

HostedFetcher::HostedFetcher(bool success,
                             const GURL& url,
                             const std::string& results,
                             URLFetcher::RequestType request_type,
                             content::URLFetcherDelegate* d)
    : URLFetcher(url, request_type, d),
      url_(url),
      status_(net::URLRequestStatus::SUCCESS, 0),
      response_code_(RC_REQUEST_OK) {
}

HostedFetcher::~HostedFetcher() {}

void HostedFetcher::Start() {
  VLOG(1) << upload_data();
  if (upload_data().find("HOSTED") == std::string::npos) {
    VLOG(1) << "HostedFetcher failing request";
    response_code_ = RC_FORBIDDEN;
    data_.assign("Error=BadAuthentication");
  }
  delegate()->OnURLFetchComplete(this);
}

const GURL& HostedFetcher::GetUrl() const {
  return url_;
}

const net::URLRequestStatus& HostedFetcher::GetStatus() const {
  return status_;
}

int HostedFetcher::GetResponseCode() const {
  return response_code_;;
}

bool HostedFetcher::GetResponseAsString(
    std::string* out_response_string) const {
  *out_response_string = data_;
  return true;
}

}  // namespace chromeos
