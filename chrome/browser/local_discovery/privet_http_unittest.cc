// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/local_discovery/privet_http_impl.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;
using testing::NiceMock;

namespace local_discovery {

namespace {

const char kSampleInfoResponse[] = "{"
    "       \"version\": \"1.0\","
    "       \"name\": \"Common printer\","
    "       \"description\": \"Printer connected through Chrome connector\","
    "       \"url\": \"https://www.google.com/cloudprint\","
    "       \"type\": ["
    "               \"printer\""
    "       ],"
    "       \"id\": \"11111111-2222-3333-4444-555555555555\","
    "       \"device_state\": \"idle\","
    "       \"connection_state\": \"online\","
    "       \"manufacturer\": \"Google\","
    "       \"model\": \"Google Chrome\","
    "       \"serial_number\": \"1111-22222-33333-4444\","
    "       \"firmware\": \"24.0.1312.52\","
    "       \"uptime\": 600,"
    "       \"setup_url\": \"http://support.google.com/\","
    "       \"support_url\": \"http://support.google.com/cloudprint/?hl=en\","
    "       \"update_url\": \"http://support.google.com/cloudprint/?hl=en\","
    "       \"x-privet-token\": \"SampleTokenForTesting\","
    "       \"api\": ["
    "               \"/privet/accesstoken\","
    "               \"/privet/capabilities\","
    "               \"/privet/printer/submitdoc\","
    "       ]"
    "}";

const char kSampleRegisterStartResponse[] = "{"
    "\"user\": \"example@google.com\","
    "\"action\": \"start\""
    "}";

const char kSampleRegisterGetClaimTokenResponse[] = "{"
    "       \"action\": \"getClaimToken\","
    "       \"user\": \"example@google.com\","
    "       \"token\": \"MySampleToken\","
    "       \"claim_url\": \"https://domain.com/SoMeUrL\""
    "}";

const char kSampleRegisterCompleteResponse[] = "{"
    "\"user\": \"example@google.com\","
    "\"action\": \"complete\","
    "\"device_id\": \"MyDeviceID\""
    "}";

const char kSampleXPrivetErrorResponse[] =
    "{ \"error\": \"invalid_x_privet_token\" }";

const char kSampleRegisterErrorTransient[] =
    "{ \"error\": \"device_busy\", \"timeout\": 1}";

const char kSampleRegisterErrorPermanent[] =
    "{ \"error\": \"user_cancel\" }";

const char kSampleInfoResponseBadJson[] = "{";

class MockTestURLFetcherFactoryDelegate
    : public net::TestURLFetcher::DelegateForTests {
 public:
  // Callback issued correspondingly to the call to the |Start()| method.
  MOCK_METHOD1(OnRequestStart, void(int fetcher_id));

  // Callback issued correspondingly to the call to |AppendChunkToUpload|.
  // Uploaded chunks can be retrieved with the |upload_chunks()| getter.
  MOCK_METHOD1(OnChunkUpload, void(int fetcher_id));

  // Callback issued correspondingly to the destructor.
  MOCK_METHOD1(OnRequestEnd, void(int fetcher_id));
};

class PrivetHTTPTest : public ::testing::Test {
 public:
  PrivetHTTPTest() {
    request_context_= new net::TestURLRequestContextGetter(
        base::MessageLoopProxy::current());
    privet_client_.reset(new PrivetHTTPClientImpl(
        "sampleDevice._privet._tcp.local",
        net::HostPortPair("10.0.0.8", 6006),
        request_context_.get()));
    fetcher_factory_.SetDelegateForTests(&fetcher_delegate_);
  }
  virtual ~PrivetHTTPTest() {
  }

 protected:
  base::MessageLoop loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory fetcher_factory_;
  scoped_ptr<PrivetHTTPClient> privet_client_;
  NiceMock<MockTestURLFetcherFactoryDelegate> fetcher_delegate_;
};

class MockInfoDelegate : public PrivetInfoOperation::Delegate {
 public:
  MockInfoDelegate() {}
  ~MockInfoDelegate() {}

  virtual void OnPrivetInfoDone(PrivetInfoOperation* operation,
                                int response_code,
                                const base::DictionaryValue* value) OVERRIDE {
    if (!value) {
      value_.reset();
    } else {
      value_.reset(value->DeepCopy());
    }

    OnPrivetInfoDoneInternal(response_code);
  }

  MOCK_METHOD1(OnPrivetInfoDoneInternal, void(int response_code));

  const base::DictionaryValue* value() { return value_.get(); }
 protected:
  scoped_ptr<base::DictionaryValue> value_;
};

class MockRegisterDelegate : public PrivetRegisterOperation::Delegate {
 public:
  MockRegisterDelegate() {
  }
  ~MockRegisterDelegate() {
  }

  virtual void OnPrivetRegisterClaimToken(
      PrivetRegisterOperation* operation,
      const std::string& token,
      const GURL& url) OVERRIDE {
    OnPrivetRegisterClaimTokenInternal(token, url);
  }

  MOCK_METHOD2(OnPrivetRegisterClaimTokenInternal, void(
      const std::string& token,
      const GURL& url));

  virtual void OnPrivetRegisterError(
      PrivetRegisterOperation* operation,
      const std::string& action,
      PrivetRegisterOperation::FailureReason reason,
      int printer_http_code,
      const DictionaryValue* json) OVERRIDE {
    // TODO(noamsml): Save and test for JSON?
    OnPrivetRegisterErrorInternal(action, reason, printer_http_code);
  }

  MOCK_METHOD3(OnPrivetRegisterErrorInternal,
               void(const std::string& action,
                    PrivetRegisterOperation::FailureReason reason,
                    int printer_http_code));

  virtual void OnPrivetRegisterDone(
      PrivetRegisterOperation* operation,
      const std::string& device_id) OVERRIDE {
    OnPrivetRegisterDoneInternal(device_id);
  }

  MOCK_METHOD1(OnPrivetRegisterDoneInternal,
               void(const std::string& device_id));
};

class PrivetInfoTest : public PrivetHTTPTest {
 public:
  PrivetInfoTest() {}

  virtual ~PrivetInfoTest() {}

  virtual void SetUp() OVERRIDE {
    info_operation_ = privet_client_->CreateInfoOperation(&info_delegate_);
  }

 protected:
  scoped_ptr<PrivetInfoOperation> info_operation_;
  StrictMock<MockInfoDelegate> info_delegate_;
};

TEST_F(PrivetInfoTest, SuccessfulInfo) {
  info_operation_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  EXPECT_EQ(GURL("http://10.0.0.8:6006/privet/info"),
            fetcher->GetOriginalURL());

  fetcher->SetResponseString(kSampleInfoResponse);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(info_delegate_, OnPrivetInfoDoneInternal(200));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  std::string name;

  privet_client_->GetCachedInfo()->GetString("name", &name);
  EXPECT_EQ("Common printer", name);
};

TEST_F(PrivetInfoTest, InfoSaveToken) {
  info_operation_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleInfoResponse);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(info_delegate_, OnPrivetInfoDoneInternal(200));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  info_operation_ = privet_client_->CreateInfoOperation(&info_delegate_);
  info_operation_->Start();

  fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);
  std::string header_token;
  ASSERT_TRUE(headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ("SampleTokenForTesting", header_token);
};

TEST_F(PrivetInfoTest, InfoFailureHTTP) {
  info_operation_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(404);

  EXPECT_CALL(info_delegate_, OnPrivetInfoDoneInternal(404));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(NULL, privet_client_->GetCachedInfo());
};

TEST_F(PrivetInfoTest, InfoFailureInternal) {
  info_operation_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(info_delegate_, OnPrivetInfoDoneInternal(-1));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(NULL, privet_client_->GetCachedInfo());
};

class PrivetRegisterTest : public PrivetHTTPTest {
 public:
  PrivetRegisterTest() {
  }
  virtual ~PrivetRegisterTest() {
  }

  virtual void SetUp() OVERRIDE {
    info_operation_ = privet_client_->CreateInfoOperation(&info_delegate_);
    register_operation_ =
        privet_client_->CreateRegisterOperation("example@google.com",
                                                &register_delegate_);
  }

 protected:
  bool SuccessfulResponseToURL(const GURL& url,
                               const std::string& response) {
    net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
    if (!fetcher || url != fetcher->GetOriginalURL())
      return false;

    fetcher->SetResponseString(response);
    fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                              net::OK));
    fetcher->set_response_code(200);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    return true;
  }

  void RunFor(base::TimeDelta time_period) {
    base::CancelableCallback<void()> callback(base::Bind(
        &PrivetRegisterTest::Stop, base::Unretained(this)));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);

    base::MessageLoop::current()->Run();
    callback.Cancel();
  }

  void Stop() {
    base::MessageLoop::current()->Quit();
  }

  scoped_ptr<PrivetInfoOperation> info_operation_;
  NiceMock<MockInfoDelegate> info_delegate_;
  scoped_ptr<PrivetRegisterOperation> register_operation_;
  StrictMock<MockRegisterDelegate> register_delegate_;
};

TEST_F(PrivetRegisterTest, RegisterSuccessSimple) {
  // Start with info request first to populate XSRF token.
  info_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/info"),
      kSampleInfoResponse));

  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=start&user=example@google.com"),
      kSampleRegisterStartResponse));

  EXPECT_CALL(register_delegate_, OnPrivetRegisterClaimTokenInternal(
      "MySampleToken",
      GURL("https://domain.com/SoMeUrL")));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=getClaimToken&user=example@google.com"),
      kSampleRegisterGetClaimTokenResponse));

  register_operation_->CompleteRegistration();

  EXPECT_CALL(register_delegate_, OnPrivetRegisterDoneInternal(
      "MyDeviceID"));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=complete&user=example@google.com"),
      kSampleRegisterCompleteResponse));
}

TEST_F(PrivetRegisterTest, RegisterNoInfoCall) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/info"),
      kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=start&user=example@google.com"),
      kSampleRegisterStartResponse));
}

TEST_F(PrivetRegisterTest, RegisterXSRFFailure) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/info"),
      kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=start&user=example@google.com"),
      kSampleRegisterStartResponse));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=getClaimToken&user=example@google.com"),
      kSampleXPrivetErrorResponse));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/info"),
      kSampleInfoResponse));

  EXPECT_CALL(register_delegate_, OnPrivetRegisterClaimTokenInternal(
      "MySampleToken", GURL("https://domain.com/SoMeUrL")));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=getClaimToken&user=example@google.com"),
      kSampleRegisterGetClaimTokenResponse));
}

TEST_F(PrivetRegisterTest, TransientFailure) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/info"),
      kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=start&user=example@google.com"),
      kSampleRegisterErrorTransient));

  EXPECT_CALL(fetcher_delegate_, OnRequestStart(0));

  RunFor(base::TimeDelta::FromSeconds(2));

  testing::Mock::VerifyAndClearExpectations(&fetcher_delegate_);

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=start&user=example@google.com"),
      kSampleRegisterStartResponse));
}

TEST_F(PrivetRegisterTest, PermanentFailure) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/info"),
      kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=start&user=example@google.com"),
      kSampleRegisterStartResponse));

  EXPECT_CALL(register_delegate_,
              OnPrivetRegisterErrorInternal(
                  "getClaimToken",
                  PrivetRegisterOperation::FAILURE_JSON_ERROR,
                  200));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/register?"
           "action=getClaimToken&user=example@google.com"),
      kSampleRegisterErrorPermanent));
}

TEST_F(PrivetRegisterTest, InfoFailure) {
  register_operation_->Start();

  EXPECT_CALL(register_delegate_,
              OnPrivetRegisterErrorInternal(
                  "info",
                  PrivetRegisterOperation::FAILURE_NETWORK,
                  -1));

  EXPECT_TRUE(SuccessfulResponseToURL(
      GURL("http://10.0.0.8:6006/privet/info"),
      kSampleInfoResponseBadJson));
}

}  // namespace

}  // namespace local_discovery
