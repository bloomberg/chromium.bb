// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/testing_policy_url_fetcher_factory.h"

#include "base/bind.h"
#include "chrome/browser/policy/logging_work_scheduler.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace {

// Extracts the GET parameter by the name "request" from an URL.
std::string GetRequestType(const GURL& url) {
  url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
  url_parse::Component key, value;
  const char* url_spec = url.spec().c_str();
  while (url_parse::ExtractQueryKeyValue(url_spec, &query, &key, &value)) {
    std::string key_str(url_spec, key.begin, key.len);
    std::string value_str(url_spec, value.begin, value.len);
    if (key_str == "request") {
      return value_str;
    }
  }
  return "";
}

}  // namespace

namespace policy {

// An URLFetcher that calls back to its factory to figure out what to respond.
class TestingPolicyURLFetcher : public URLFetcher {
 public:
  TestingPolicyURLFetcher(
      const base::WeakPtr<TestingPolicyURLFetcherFactory>& parent,
      const GURL& url,
      URLFetcher::RequestType request_type,
      URLFetcher::Delegate* delegate);

  virtual void Start() OVERRIDE;
  void Respond();

 private:
  GURL url_;
  TestURLResponse response_;
  base::WeakPtr<TestingPolicyURLFetcherFactory> parent_;

  DISALLOW_COPY_AND_ASSIGN(TestingPolicyURLFetcher);
};

TestingPolicyURLFetcher::TestingPolicyURLFetcher(
    const base::WeakPtr<TestingPolicyURLFetcherFactory>& parent,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcher::Delegate* delegate)
        : URLFetcher(url, request_type, delegate),
          url_(url),
          parent_(parent) {
}

void TestingPolicyURLFetcher::Start() {
  if (!parent_.get()) return;

  std::string auth_header;
  net::HttpRequestHeaders headers;
  std::string request = GetRequestType(url_);
  GetExtraRequestHeaders(&headers);
  headers.GetHeader("Authorization", &auth_header);
  // The following method is mocked by the currently running test.
  parent_->GetResponse(auth_header, request, &response_);

  // We need to channel this through the central event logger, so that ordering
  // with other logged tasks that have a delay is preserved.
  parent_->scheduler()->PostDelayedWork(
      base::Bind(&TestingPolicyURLFetcher::Respond, base::Unretained(this)),
      0);
}

void TestingPolicyURLFetcher::Respond() {
  delegate()->OnURLFetchComplete(
      this,
      url_,
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0),
      response_.response_code,
      net::ResponseCookies(),
      response_.response_data);
}

TestingPolicyURLFetcherFactory::TestingPolicyURLFetcherFactory(
    EventLogger* logger)
    : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      logger_(logger),
      scheduler_(new LoggingWorkScheduler(logger)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

TestingPolicyURLFetcherFactory::~TestingPolicyURLFetcherFactory() {
}

LoggingWorkScheduler* TestingPolicyURLFetcherFactory::scheduler() {
  return scheduler_.get();
}

void TestingPolicyURLFetcherFactory::GetResponse(
    const std::string& auth_header,
    const std::string& request,
    TestURLResponse* response) {
  logger_->RegisterEvent();
  Intercept(auth_header, request, response);
}

URLFetcher* TestingPolicyURLFetcherFactory::CreateURLFetcher(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcher::Delegate* delegate) {
  return new TestingPolicyURLFetcher(
      weak_ptr_factory_.GetWeakPtr(), url, request_type, delegate);
}

}  // namespace policy
