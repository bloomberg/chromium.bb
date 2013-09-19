// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/local_discovery/privet_confirm_api_flow.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;

namespace local_discovery {

namespace {

const char kSampleConfirmResponse[] = "{"
    "   \"success\": true"
    "}";

const char kFailedConfirmResponse[] = "{"
    "   \"success\": false"
    "}";

const char kFailedConfirmResponseBadJson[] = "["
    "   \"success\""
    "]";

class TestOAuth2TokenService : public OAuth2TokenService {
 public:
  explicit TestOAuth2TokenService(net::URLRequestContextGetter* request_context)
      : request_context_(request_context) {
  }
 protected:
  virtual std::string GetRefreshToken() OVERRIDE {
    return "SampleToken";
  }

  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE {
    return request_context_.get();
  }

 private:
  scoped_refptr<net::URLRequestContextGetter> request_context_;
};

class MockableConfirmCallback {
 public:
  MOCK_METHOD1(ConfirmCallback, void(CloudPrintBaseApiFlow::Status));

  PrivetConfirmApiCallFlow::ResponseCallback callback() {
    return base::Bind(&MockableConfirmCallback::ConfirmCallback,
                      base::Unretained(this));
  }
};

class PrivetConfirmApiFlowTest : public testing::Test {
 public:
  PrivetConfirmApiFlowTest()
      : ui_thread_(content::BrowserThread::UI,
                   &loop_),
        request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())),
        token_service_(request_context_.get()) {
    ui_thread_.Stop();  // HACK: Fake being on the UI thread
  }

  virtual ~PrivetConfirmApiFlowTest() {
  }

 protected:
  base::MessageLoopForUI loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory fetcher_factory_;
  TestOAuth2TokenService token_service_;
  MockableConfirmCallback callback_;
};

TEST_F(PrivetConfirmApiFlowTest, SuccessOAuth2) {
  PrivetConfirmApiCallFlow confirm_flow(request_context_.get(),
                                        &token_service_,
                                        GURL("http://SoMeUrL.com"),
                                        callback_.callback());
  CloudPrintBaseApiFlow* cloudprint_flow =
      confirm_flow.GetBaseApiFlowForTests();

  confirm_flow.Start();

  cloudprint_flow->OnGetTokenSuccess(NULL, "SomeToken", base::Time());
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);

  EXPECT_EQ(GURL("http://SoMeUrL.com"), fetcher->GetOriginalURL());

  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);
  std::string oauth_header;
  std::string proxy;
  EXPECT_TRUE(headers.GetHeader("Authorization", &oauth_header));
  EXPECT_EQ("Bearer SomeToken", oauth_header);
  EXPECT_TRUE(headers.GetHeader("X-Cloudprint-Proxy", &proxy));
  EXPECT_EQ("Chrome", proxy);

  fetcher->SetResponseString(kSampleConfirmResponse);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(callback_, ConfirmCallback(CloudPrintBaseApiFlow::SUCCESS));

  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetConfirmApiFlowTest, SuccessCookies) {
  PrivetConfirmApiCallFlow confirm_flow(request_context_.get(),
                                        1,
                                        "SomeToken",
                                        GURL("http://SoMeUrL.com?token=tkn"),
                                        callback_.callback());

  confirm_flow.Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);

  EXPECT_EQ(GURL("http://SoMeUrL.com?token=tkn&xsrf=SomeToken&user=1"),
            fetcher->GetOriginalURL());

  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);
  std::string proxy;
  EXPECT_TRUE(headers.GetHeader("X-Cloudprint-Proxy", &proxy));
  EXPECT_EQ("Chrome", proxy);

  fetcher->SetResponseString(kSampleConfirmResponse);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(callback_, ConfirmCallback(CloudPrintBaseApiFlow::SUCCESS));

  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetConfirmApiFlowTest, BadToken) {
  PrivetConfirmApiCallFlow confirm_flow(request_context_.get(),
                                        &token_service_,
                                        GURL("http://SoMeUrL.com"),
                                        callback_.callback());

  confirm_flow.Start();

  CloudPrintBaseApiFlow* cloudprint_flow =
      confirm_flow.GetBaseApiFlowForTests();

  EXPECT_CALL(callback_,
              ConfirmCallback(CloudPrintBaseApiFlow::ERROR_TOKEN));
  cloudprint_flow->OnGetTokenFailure(NULL, GoogleServiceAuthError(
      GoogleServiceAuthError::USER_NOT_SIGNED_UP));
}

TEST_F(PrivetConfirmApiFlowTest, ServerFailure) {
  PrivetConfirmApiCallFlow confirm_flow(request_context_.get(),
                                        &token_service_,
                                        GURL("http://SoMeUrL.com"),
                                        callback_.callback());

  confirm_flow.Start();

  CloudPrintBaseApiFlow* cloudprint_flow =
      confirm_flow.GetBaseApiFlowForTests();

  cloudprint_flow->OnGetTokenSuccess(NULL, "SomeToken", base::Time());
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);

  EXPECT_EQ(GURL("http://SoMeUrL.com"), fetcher->GetOriginalURL());

  fetcher->SetResponseString(kFailedConfirmResponse);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(callback_,
              ConfirmCallback(CloudPrintBaseApiFlow::ERROR_FROM_SERVER));

  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetConfirmApiFlowTest, BadJson) {
  PrivetConfirmApiCallFlow confirm_flow(request_context_.get(),
                                        &token_service_,
                                        GURL("http://SoMeUrL.com"),
                                        callback_.callback());

  confirm_flow.Start();

  CloudPrintBaseApiFlow* cloudprint_flow =
      confirm_flow.GetBaseApiFlowForTests();

  cloudprint_flow->OnGetTokenSuccess(NULL, "SomeToken", base::Time());
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);

  EXPECT_EQ(GURL("http://SoMeUrL.com"), fetcher->GetOriginalURL());

  fetcher->SetResponseString(kFailedConfirmResponseBadJson);
  fetcher->set_status(net::URLRequestStatus(
      net::URLRequestStatus::SUCCESS,
      net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(callback_, ConfirmCallback
              (CloudPrintBaseApiFlow::ERROR_MALFORMED_RESPONSE));

  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

}  // namespace

}  // namespace local_discovery
