// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/safe_search_url_reporter.h"

#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSafeSearchReportApiUrl[] =
    "https://safesearch.googleapis.com/v1:report";
const char kEmail[] = "account@gmail.com";

}  // namespace

class SafeSearchURLReporterTest : public testing::Test {
 public:
  SafeSearchURLReporterTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    AccountInfo account_info =
        identity_test_env_.MakePrimaryAccountAvailable(kEmail);
    account_id_ = account_info.account_id;
    report_url_ = std::make_unique<SafeSearchURLReporter>(
        identity_test_env_.identity_manager(), test_shared_loader_factory_);
  }

 protected:
  void IssueAccessTokens() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        account_id_, "access_token",
        base::Time::Now() + base::TimeDelta::FromHours(1));
  }

  void IssueAccessTokenErrors() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        account_id_, GoogleServiceAuthError::FromServiceError("Error!"));
  }

  void SetupResponse(net::Error error) {
    network::ResourceResponseHead head;
    std::string headers("HTTP/1.1 200 OK\n\n");
    head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
    network::URLLoaderCompletionStatus status(error);
    test_url_loader_factory_.AddResponse(GURL(kSafeSearchReportApiUrl), head,
                                         std::string(), status);
  }

  void CreateRequest(const GURL& url) {
    report_url_->ReportUrl(
        url, base::BindOnce(&SafeSearchURLReporterTest::OnRequestCreated,
                            base::Unretained(this)));
  }

  void WaitForResponse() { base::RunLoop().RunUntilIdle(); }

  MOCK_METHOD1(OnRequestCreated, void(bool success));

  base::MessageLoop message_loop_;
  std::string account_id_;
  identity::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<SafeSearchURLReporter> report_url_;
};

TEST_F(SafeSearchURLReporterTest, Success) {
  CreateRequest(GURL("http://google.com"));
  CreateRequest(GURL("http://url.com"));

  IssueAccessTokens();

  EXPECT_CALL(*this, OnRequestCreated(true)).Times(2);
  SetupResponse(net::OK);
  SetupResponse(net::OK);
  WaitForResponse();
}

TEST_F(SafeSearchURLReporterTest, AccessTokenError) {
  CreateRequest(GURL("http://google.com"));

  EXPECT_CALL(*this, OnRequestCreated(false));
  IssueAccessTokenErrors();
}

TEST_F(SafeSearchURLReporterTest, NetworkError) {
  CreateRequest(GURL("http://google.com"));

  IssueAccessTokens();

  EXPECT_CALL(*this, OnRequestCreated(false));
  SetupResponse(net::ERR_ABORTED);
  WaitForResponse();
}
