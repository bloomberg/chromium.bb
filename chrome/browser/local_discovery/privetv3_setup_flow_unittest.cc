// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_setup_flow.h"

#include "base/json/json_reader.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_discovery {

namespace {

using testing::HasSubstr;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArgs;
using testing::_;

const char kServiceName[] = "test_service";

const char kRegistrationTicketResponse[] =
    "{"
    "\"kind\": \"clouddevices#registrationTicket\","
    "\"id\": \"test_ticket\""
    "}";

class MockPrivetHTTPClient : public PrivetHTTPClient {
 public:
  MockPrivetHTTPClient() {
    request_context_ =
        new net::TestURLRequestContextGetter(base::MessageLoopProxy::current());
  }

  MOCK_METHOD0(GetName, const std::string&());
  MOCK_METHOD1(
      CreateInfoOperationPtr,
      PrivetJSONOperation*(const PrivetJSONOperation::ResultCallback&));

  virtual void RefreshPrivetToken(
      const PrivetURLFetcher::TokenCallback& callback) OVERRIDE {
    callback.Run("x-privet-token");
  }

  virtual scoped_ptr<PrivetJSONOperation> CreateInfoOperation(
      const PrivetJSONOperation::ResultCallback& callback) OVERRIDE {
    return make_scoped_ptr(CreateInfoOperationPtr(callback));
  }

  virtual scoped_ptr<PrivetURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      PrivetURLFetcher::Delegate* delegate) OVERRIDE {
    return make_scoped_ptr(
        new PrivetURLFetcher(url, request_type, request_context_, delegate));
  }

  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
};

class MockDelegate : public PrivetV3SetupFlow::Delegate {
 public:
  MockDelegate() : privet_client_ptr_(NULL) {}

  class MockGCDApiFlow : public GCDApiFlow {
   public:
    explicit MockGCDApiFlow(MockDelegate* delegate) : delegate_(delegate) {}

    virtual void Start(scoped_ptr<Request> request) OVERRIDE {
      ASSERT_FALSE(delegate_->gcd_request_);
      delegate_->gcd_request_ = request.Pass();
      delegate_->ReplyWithToken();
    }

   private:
    MockDelegate* delegate_;
  };

  MOCK_METHOD1(GetWiFiCredentials, void(const CredentialsCallback&));
  MOCK_METHOD1(SwitchToSetupWiFi, void(const ResultCallback&));
  virtual void CreatePrivetV3Client(
      const std::string& service_name,
      const PrivetClientCallback& callback) OVERRIDE {
    scoped_ptr<MockPrivetHTTPClient> privet_client(new MockPrivetHTTPClient());
    privet_client_ptr_ = privet_client.get();
    callback.Run(privet_client.PassAs<PrivetHTTPClient>());
  }
  MOCK_METHOD2(ConfirmSecurityCode,
               void(const std::string&, const ResultCallback&));
  MOCK_METHOD1(RestoreWifi, void(const ResultCallback&));
  MOCK_METHOD0(OnSetupDone, void());
  MOCK_METHOD0(OnSetupError, void());

  virtual scoped_ptr<GCDApiFlow> CreateApiFlow() OVERRIDE {
    scoped_ptr<MockGCDApiFlow> mock_gcd(new MockGCDApiFlow(this));
    return mock_gcd.PassAs<GCDApiFlow>();
  }

  void ReplyWithToken() {
    scoped_ptr<base::Value> value(base::JSONReader::Read(gcd_server_response_));
    const base::DictionaryValue* dictionary = NULL;
    value->GetAsDictionary(&dictionary);
    gcd_request_->OnGCDAPIFlowComplete(*dictionary);
  }

  void ConfirmCode(const ResultCallback& confirm_callback) {
    confirm_callback.Run(true);
  }

  std::string gcd_server_response_;
  scoped_ptr<GCDApiFlow::Request> gcd_request_;
  MockPrivetHTTPClient* privet_client_ptr_;
};

class PrivetV3SetupFlowTest : public testing::Test {
 public:
  PrivetV3SetupFlowTest() : setup_(&delegate_) {}

  virtual ~PrivetV3SetupFlowTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(delegate_, GetWiFiCredentials(_)).Times(0);
    EXPECT_CALL(delegate_, SwitchToSetupWiFi(_)).Times(0);
    EXPECT_CALL(delegate_, ConfirmSecurityCode(_, _)).Times(0);
    EXPECT_CALL(delegate_, RestoreWifi(_)).Times(0);
    EXPECT_CALL(delegate_, OnSetupDone()).Times(0);
    EXPECT_CALL(delegate_, OnSetupError()).Times(0);
  }

  void SimulateFetch(int response_code, const std::string& response) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    EXPECT_THAT(fetcher->GetOriginalURL().spec(),
                testing::HasSubstr("/privet/v3/setup/start"));
    fetcher->set_response_code(response_code);
    scoped_refptr<net::HttpResponseHeaders> response_headers(
        new net::HttpResponseHeaders(""));
    response_headers->AddHeader("Content-Type: application/json");
    fetcher->set_response_headers(response_headers);
    fetcher->SetResponseString(response);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  base::MessageLoop loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;

  StrictMock<MockDelegate> delegate_;
  PrivetV3SetupFlow setup_;
};

TEST_F(PrivetV3SetupFlowTest, InvalidTicket) {
  EXPECT_CALL(delegate_, OnSetupError()).Times(1);
  delegate_.gcd_server_response_ = "{}";
  setup_.Register(kServiceName);
}

TEST_F(PrivetV3SetupFlowTest, InvalidDeviceResponce) {
  EXPECT_CALL(delegate_, OnSetupError()).Times(1);
  EXPECT_CALL(delegate_, ConfirmSecurityCode(_, _)).Times(1).WillOnce(
      WithArgs<1>(Invoke(&delegate_, &MockDelegate::ConfirmCode)));
  delegate_.gcd_server_response_ = kRegistrationTicketResponse;
  setup_.Register(kServiceName);
  SimulateFetch(0, "{}");
}

TEST_F(PrivetV3SetupFlowTest, Success) {
  EXPECT_CALL(delegate_, OnSetupDone()).Times(1);
  EXPECT_CALL(delegate_, ConfirmSecurityCode(_, _)).Times(1).WillOnce(
      WithArgs<1>(Invoke(&delegate_, &MockDelegate::ConfirmCode)));
  delegate_.gcd_server_response_ = kRegistrationTicketResponse;
  setup_.Register(kServiceName);
  SimulateFetch(200, "{}");
}

}  // namespace

}  // namespace local_discovery
