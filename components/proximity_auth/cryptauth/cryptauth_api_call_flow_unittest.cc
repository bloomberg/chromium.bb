// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_api_call_flow.h"

#include "base/test/test_simple_task_runner.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {

namespace {

const char kSerializedRequestProto[] = "serialized_request_proto";
const char kSerializedResponseProto[] = "result_proto";
const char kRequestUrl[] = "https://googleapis.com/cryptauth/test";

// For the specifications of the mint access token request, see:
// https://developers.google.com/accounts/docs/OAuth2WebServer
const char kMintTokenPath[] = "/o/oauth2/token";
const char kMintTokenResponse[] =
    "{"
    "  \"access_token\":\"1/fFAGRNJru1FQd77BzhT3Zg\","
    "  \"expires_in\":3920,"
    "  \"token_type\":\"Bearer\""
    "}";

}  // namespace

class ProximityAuthCryptAuthApiCallFlowTest
    : public testing::Test,
      public net::TestURLFetcherDelegateForTests {
 protected:
  ProximityAuthCryptAuthApiCallFlowTest()
      : url_request_context_getter_(new net::TestURLRequestContextGetter(
            new base::TestSimpleTaskRunner())) {}

  virtual void SetUp() OVERRIDE {
    // The TestURLFetcherFactory will override the global URLFetcherFactory for
    // the entire test.
    url_fetcher_factory_.reset(new net::TestURLFetcherFactory());
    url_fetcher_factory_->SetDelegateForTests(this);

    flow_.reset(new CryptAuthApiCallFlow(url_request_context_getter_.get(),
                                         "refresh_token",
                                         "access_token",
                                         GURL(kRequestUrl)));
  }

  void StartApiCallFlow() {
    flow_->Start(kSerializedRequestProto,
                 base::Bind(&ProximityAuthCryptAuthApiCallFlowTest::OnResult,
                            base::Unretained(this)),
                 base::Bind(&ProximityAuthCryptAuthApiCallFlowTest::OnError,
                            base::Unretained(this)));
    // URLFetcher object for the API request should be created.
    CheckCryptAuthHttpRequest();
  }

  void OnResult(const std::string& result) {
    EXPECT_FALSE(result_);
    result_.reset(new std::string(result));
  }

  void OnError(const std::string& error_message) {
    EXPECT_FALSE(error_message_);
    error_message_.reset(new std::string(error_message));
  }

  void CheckCryptAuthHttpRequest() {
    ASSERT_TRUE(url_fetcher_);
    EXPECT_EQ(GURL(kRequestUrl), url_fetcher_->GetOriginalURL());
    EXPECT_EQ(kSerializedRequestProto, url_fetcher_->upload_data());

    net::HttpRequestHeaders request_headers;
    url_fetcher_->GetExtraRequestHeaders(&request_headers);

    EXPECT_EQ("application/x-protobuf", url_fetcher_->upload_content_type());
  }

  // Responds to the current HTTP request. If the |request_status| is not
  // success, then the |response_code| and |response_string| arguments will be
  // ignored.
  void CompleteCurrentRequest(net::URLRequestStatus::Status request_status,
                              int response_code,
                              const std::string& response_string) {
    ASSERT_TRUE(url_fetcher_);
    net::TestURLFetcher* url_fetcher = url_fetcher_;
    url_fetcher_ = NULL;
    url_fetcher->set_status(net::URLRequestStatus(request_status, 0));
    if (request_status == net::URLRequestStatus::SUCCESS) {
      url_fetcher->set_response_code(response_code);
      url_fetcher->SetResponseString(response_string);
    }
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  // net::TestURLFetcherDelegateForTests overrides.
  virtual void OnRequestStart(int fetcher_id) OVERRIDE {
    url_fetcher_ = url_fetcher_factory_->GetFetcherByID(fetcher_id);
  }

  virtual void OnChunkUpload(int fetcher_id) OVERRIDE {}

  virtual void OnRequestEnd(int fetcher_id) OVERRIDE {}

  net::TestURLFetcher* url_fetcher_;
  scoped_ptr<std::string> result_;
  scoped_ptr<std::string> error_message_;

 private:
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<net::TestURLFetcherFactory> url_fetcher_factory_;
  scoped_ptr<CryptAuthApiCallFlow> flow_;
};

TEST_F(ProximityAuthCryptAuthApiCallFlowTest, RequestSuccess) {
  StartApiCallFlow();
  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(error_message_);
}

TEST_F(ProximityAuthCryptAuthApiCallFlowTest, RequestFailure) {
  StartApiCallFlow();
  CompleteCurrentRequest(net::URLRequestStatus::FAILED, 0, std::string());
  EXPECT_FALSE(result_);
  EXPECT_EQ("Request failed", *error_message_);
}

TEST_F(ProximityAuthCryptAuthApiCallFlowTest, RequestStatus500) {
  StartApiCallFlow();
  CompleteCurrentRequest(net::URLRequestStatus::SUCCESS,
                         net::HTTP_INTERNAL_SERVER_ERROR,
                         "CryptAuth Meltdown.");
  EXPECT_FALSE(result_);
  EXPECT_EQ("HTTP status: 500", *error_message_);
}

TEST_F(ProximityAuthCryptAuthApiCallFlowTest, ResponseWithNoBody) {
  StartApiCallFlow();
  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_OK, std::string());
  EXPECT_FALSE(result_);
  EXPECT_EQ("No response body", *error_message_);
}

TEST_F(ProximityAuthCryptAuthApiCallFlowTest, MintTokenThenApiSuccess) {
  StartApiCallFlow();

  // A status code of 401 Unauthorized should cause the flow to mint a new
  // access token and try again.
  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_UNAUTHORIZED, std::string());
  EXPECT_FALSE(result_);
  EXPECT_FALSE(error_message_);

  EXPECT_EQ(kMintTokenPath, url_fetcher_->GetOriginalURL().path());
  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_OK, kMintTokenResponse);

  // Upon minting the access token, the flow should retry the original request.
  CheckCryptAuthHttpRequest();
  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_OK, kSerializedResponseProto);
  EXPECT_EQ(kSerializedResponseProto, *result_);
  EXPECT_FALSE(error_message_);
}

TEST_F(ProximityAuthCryptAuthApiCallFlowTest, MintTokenApiFailure) {
  StartApiCallFlow();

  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_UNAUTHORIZED, std::string());

  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_OK, kMintTokenResponse);

  CompleteCurrentRequest(net::URLRequestStatus::FAILED, 0, std::string());
  EXPECT_FALSE(result_);
  EXPECT_EQ("Request failed", *error_message_);
}

TEST_F(ProximityAuthCryptAuthApiCallFlowTest, MintTokenFailure) {
  StartApiCallFlow();

  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_UNAUTHORIZED, std::string());

  CompleteCurrentRequest(
      net::URLRequestStatus::SUCCESS, net::HTTP_UNAUTHORIZED, std::string());

  EXPECT_FALSE(result_);
  EXPECT_EQ("Could not mint access token", *error_message_);
}

}  // namespace proximity_auth
