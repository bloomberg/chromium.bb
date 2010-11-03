// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_backend_impl.h"

#include "base/message_loop.h"
#include "base/string_split.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/device_management_backend_mock.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace policy {

namespace {

const char kServiceURL[] = "https://example.com/management_service";

// Encoded error response messages for testing the error code paths.
const char kResponseEmpty[] = "\x08\x00";
const char kResponseErrorManagementNotSupported[] = "\x08\x01";
const char kResponseErrorDeviceNotFound[] = "\x08\x02";
const char kResponseErrorManagementTokenInvalid[] = "\x08\x03";
const char kResponseErrorActivationPending[] = "\x08\x04";

#define PROTO_STRING(name) (std::string(name, arraysize(name) - 1))

}  // namespace

// Unit tests for the google apps policy backend. The pattern here is each test
// case triggeres a request and installs a mock delegate. The test will run and
// the default action installed on the test delegate will quit the loop.
template<typename TESTBASE>
class DeviceManagementBackendImplTestBase : public TESTBASE {
 protected:
  DeviceManagementBackendImplTestBase()
      : io_thread_(BrowserThread::IO, &loop_),
        service_(kServiceURL) {}

  virtual void SetUp() {
    URLFetcher::set_factory(&factory_);
  }

  virtual void TearDown() {
    URLFetcher::set_factory(NULL);
    loop_.RunAllPending();
  }

  MessageLoopForUI loop_;
  BrowserThread io_thread_;
  TestURLFetcherFactory factory_;
  DeviceManagementBackendImpl service_;
};

struct FailedRequestParams {
  FailedRequestParams(DeviceManagementBackend::ErrorCode expected_error,
                      URLRequestStatus::Status request_status,
                      int http_status,
                      const std::string& response)
      : expected_error_(expected_error),
        request_status_(request_status, 0),
        http_status_(http_status),
        response_(response) {}

  DeviceManagementBackend::ErrorCode expected_error_;
  URLRequestStatus request_status_;
  int http_status_;
  std::string response_;
};

// A parameterized test case for erroneous response situations, they're mostly
// the same for all kinds of requests.
class DeviceManagementBackendImplFailedRequestTest
    : public DeviceManagementBackendImplTestBase<
          testing::TestWithParam<FailedRequestParams> > {
};

TEST_P(DeviceManagementBackendImplFailedRequestTest, RegisterRequest) {
  DeviceRegisterResponseDelegateMock mock;
  EXPECT_CALL(mock, OnError(GetParam().expected_error_));
  em::DeviceRegisterRequest request;
  service_.ProcessRegisterRequest("token", "device id", request, &mock);
  TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  fetcher->delegate()->OnURLFetchComplete(fetcher,
                                          GURL(kServiceURL),
                                          GetParam().request_status_,
                                          GetParam().http_status_,
                                          ResponseCookies(),
                                          GetParam().response_);
}

TEST_P(DeviceManagementBackendImplFailedRequestTest, UnregisterRequest) {
  DeviceUnregisterResponseDelegateMock mock;
  EXPECT_CALL(mock, OnError(GetParam().expected_error_));
  em::DeviceUnregisterRequest request;
  service_.ProcessUnregisterRequest("token", request, &mock);
  TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  fetcher->delegate()->OnURLFetchComplete(fetcher,
                                          GURL(kServiceURL),
                                          GetParam().request_status_,
                                          GetParam().http_status_,
                                          ResponseCookies(),
                                          GetParam().response_);
}

TEST_P(DeviceManagementBackendImplFailedRequestTest, PolicyRequest) {
  DevicePolicyResponseDelegateMock mock;
  EXPECT_CALL(mock, OnError(GetParam().expected_error_));
  em::DevicePolicyRequest request;
  request.set_policy_scope("Chrome");
  em::DevicePolicySettingRequest* setting_request =
      request.add_setting_request();
  setting_request->set_key("policy");
  service_.ProcessPolicyRequest("token", request, &mock);
  TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  fetcher->delegate()->OnURLFetchComplete(fetcher,
                                          GURL(kServiceURL),
                                          GetParam().request_status_,
                                          GetParam().http_status_,
                                          ResponseCookies(),
                                          GetParam().response_);
}

INSTANTIATE_TEST_CASE_P(
    DeviceManagementBackendImplFailedRequestTestInstance,
    DeviceManagementBackendImplFailedRequestTest,
    testing::Values(
        FailedRequestParams(
            DeviceManagementBackend::kErrorRequestFailed,
            URLRequestStatus::FAILED,
            200,
            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(
            DeviceManagementBackend::kErrorHttpStatus,
            URLRequestStatus::SUCCESS,
            500,
            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(
            DeviceManagementBackend::kErrorResponseDecoding,
            URLRequestStatus::SUCCESS,
            200,
            PROTO_STRING("Not a protobuf.")),
        FailedRequestParams(
            DeviceManagementBackend::kErrorServiceManagementNotSupported,
            URLRequestStatus::SUCCESS,
            200,
            PROTO_STRING(kResponseErrorManagementNotSupported)),
        FailedRequestParams(
            DeviceManagementBackend::kErrorServiceDeviceNotFound,
            URLRequestStatus::SUCCESS,
            200,
            PROTO_STRING(kResponseErrorDeviceNotFound)),
        FailedRequestParams(
            DeviceManagementBackend::kErrorServiceManagementTokenInvalid,
            URLRequestStatus::SUCCESS,
            200,
            PROTO_STRING(kResponseErrorManagementTokenInvalid)),
        FailedRequestParams(
            DeviceManagementBackend::kErrorServiceActivationPending,
            URLRequestStatus::SUCCESS,
            200,
            PROTO_STRING(kResponseErrorActivationPending))));

class DeviceManagementBackendImplTest
    : public DeviceManagementBackendImplTestBase<testing::Test> {
};

MATCHER_P(MessageEquals, reference, "") {
  std::string reference_data;
  std::string arg_data;
  return arg.SerializeToString(&arg_data) &&
         reference.SerializeToString(&reference_data) &&
         arg_data == reference_data;
}

// Simple query parameter parser for testing.
class QueryParams {
 public:
  explicit QueryParams(const std::string& query) {
    base::SplitStringIntoKeyValuePairs(query, '=', '&', &params_);
  }

  bool Check(const std::string& name, const std::string& expected_value) {
    bool found = false;
    for (ParamMap::const_iterator i(params_.begin()); i != params_.end(); ++i) {
      std::string unescaped_name(
          UnescapeURLComponent(i->first,
                               UnescapeRule::NORMAL |
                               UnescapeRule::SPACES |
                               UnescapeRule::URL_SPECIAL_CHARS |
                               UnescapeRule::CONTROL_CHARS |
                               UnescapeRule::REPLACE_PLUS_WITH_SPACE));
      if (unescaped_name == name) {
        if (found)
          return false;
        found = true;
        std::string unescaped_value(
            UnescapeURLComponent(i->second,
                                 UnescapeRule::NORMAL |
                                 UnescapeRule::SPACES |
                                 UnescapeRule::URL_SPECIAL_CHARS |
                                 UnescapeRule::CONTROL_CHARS |
                                 UnescapeRule::REPLACE_PLUS_WITH_SPACE));
        if (unescaped_value != expected_value)
          return false;
      }
    }
    return found;
  }

 private:
  typedef std::vector<std::pair<std::string, std::string> > ParamMap;
  ParamMap params_;
};

TEST_F(DeviceManagementBackendImplTest, RegisterRequest) {
  DeviceRegisterResponseDelegateMock mock;
  em::DeviceRegisterResponse expected_response;
  expected_response.set_device_management_token("mtoken");
  EXPECT_CALL(mock, HandleRegisterResponse(MessageEquals(expected_response)));
  em::DeviceRegisterRequest request;
  service_.ProcessRegisterRequest("token", "device id", request, &mock);
  TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  // Check the data the fetcher received.
  const GURL& request_url(fetcher->original_url());
  const GURL service_url(kServiceURL);
  EXPECT_EQ(service_url.scheme(), request_url.scheme());
  EXPECT_EQ(service_url.host(), request_url.host());
  EXPECT_EQ(service_url.port(), request_url.port());
  EXPECT_EQ(service_url.path(), request_url.path());

  QueryParams query_params(request_url.query());
  EXPECT_TRUE(query_params.Check("request", "register"));

  em::DeviceManagementRequest expected_request_wrapper;
  expected_request_wrapper.mutable_register_request()->CopyFrom(request);
  std::string expected_request_data;
  ASSERT_TRUE(expected_request_wrapper.SerializeToString(
      &expected_request_data));
  EXPECT_EQ(expected_request_data, fetcher->upload_data());

  // Generate the response.
  std::string response_data;
  em::DeviceManagementResponse response_wrapper;
  response_wrapper.set_error(em::DeviceManagementResponse::SUCCESS);
  response_wrapper.mutable_register_response()->CopyFrom(expected_response);
  ASSERT_TRUE(response_wrapper.SerializeToString(&response_data));
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  fetcher->delegate()->OnURLFetchComplete(fetcher,
                                          GURL(kServiceURL),
                                          status,
                                          200,
                                          ResponseCookies(),
                                          response_data);
}

TEST_F(DeviceManagementBackendImplTest, UnregisterRequest) {
  DeviceUnregisterResponseDelegateMock mock;
  em::DeviceUnregisterResponse expected_response;
  EXPECT_CALL(mock, HandleUnregisterResponse(MessageEquals(expected_response)));
  em::DeviceUnregisterRequest request;
  service_.ProcessUnregisterRequest("dmtokenvalue", request, &mock);
  TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  // Check the data the fetcher received.
  const GURL& request_url(fetcher->original_url());
  const GURL service_url(kServiceURL);
  EXPECT_EQ(service_url.scheme(), request_url.scheme());
  EXPECT_EQ(service_url.host(), request_url.host());
  EXPECT_EQ(service_url.port(), request_url.port());
  EXPECT_EQ(service_url.path(), request_url.path());

  QueryParams query_params(request_url.query());
  EXPECT_TRUE(query_params.Check("request", "unregister"));

  em::DeviceManagementRequest expected_request_wrapper;
  expected_request_wrapper.mutable_unregister_request()->CopyFrom(request);
  std::string expected_request_data;
  ASSERT_TRUE(expected_request_wrapper.SerializeToString(
      &expected_request_data));
  EXPECT_EQ(expected_request_data, fetcher->upload_data());

  // Generate the response.
  std::string response_data;
  em::DeviceManagementResponse response_wrapper;
  response_wrapper.set_error(em::DeviceManagementResponse::SUCCESS);
  response_wrapper.mutable_unregister_response()->CopyFrom(expected_response);
  ASSERT_TRUE(response_wrapper.SerializeToString(&response_data));
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  fetcher->delegate()->OnURLFetchComplete(fetcher,
                                          GURL(kServiceURL),
                                          status,
                                          200,
                                          ResponseCookies(),
                                          response_data);
}

TEST_F(DeviceManagementBackendImplTest, PolicyRequest) {
  DevicePolicyResponseDelegateMock mock;
  em::DevicePolicyResponse expected_response;
  em::DevicePolicySetting* policy_setting = expected_response.add_setting();
  policy_setting->set_policy_key("policy");
  policy_setting->set_watermark("fresh");
  em::GenericSetting* policy_value = policy_setting->mutable_policy_value();
  em::GenericNamedValue* named_value = policy_value->add_named_value();
  named_value->set_name("HomepageLocation");
  named_value->mutable_value()->set_value_type(
      em::GenericValue::VALUE_TYPE_STRING);
  named_value->mutable_value()->set_string_value("http://www.chromium.org");
  named_value = policy_value->add_named_value();
  named_value->set_name("HomepageIsNewTabPage");
  named_value->mutable_value()->set_value_type(
      em::GenericValue::VALUE_TYPE_BOOL);
  named_value->mutable_value()->set_bool_value(false);
  EXPECT_CALL(mock, HandlePolicyResponse(MessageEquals(expected_response)));

  em::DevicePolicyRequest request;
  request.set_policy_scope("chromium");
  em::DevicePolicySettingRequest* setting_request =
      request.add_setting_request();
  setting_request->set_key("policy");
  setting_request->set_watermark("stale");
  service_.ProcessPolicyRequest("dmtokenvalue", request, &mock);
  TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  // Check the data the fetcher received.
  const GURL& request_url(fetcher->original_url());
  const GURL service_url(kServiceURL);
  EXPECT_EQ(service_url.scheme(), request_url.scheme());
  EXPECT_EQ(service_url.host(), request_url.host());
  EXPECT_EQ(service_url.port(), request_url.port());
  EXPECT_EQ(service_url.path(), request_url.path());

  QueryParams query_params(request_url.query());
  EXPECT_TRUE(query_params.Check("request", "policy"));

  em::DeviceManagementRequest expected_request_wrapper;
  expected_request_wrapper.mutable_policy_request()->CopyFrom(request);
  std::string expected_request_data;
  ASSERT_TRUE(expected_request_wrapper.SerializeToString(
      &expected_request_data));
  EXPECT_EQ(expected_request_data, fetcher->upload_data());

  // Generate the response.
  std::string response_data;
  em::DeviceManagementResponse response_wrapper;
  response_wrapper.set_error(em::DeviceManagementResponse::SUCCESS);
  response_wrapper.mutable_policy_response()->CopyFrom(expected_response);
  ASSERT_TRUE(response_wrapper.SerializeToString(&response_data));
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  fetcher->delegate()->OnURLFetchComplete(fetcher,
                                          GURL(kServiceURL),
                                          status,
                                          200,
                                          ResponseCookies(),
                                          response_data);
}

}  // namespace policy
