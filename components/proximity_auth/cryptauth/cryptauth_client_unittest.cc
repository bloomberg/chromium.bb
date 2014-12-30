// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_client.h"

#include "base/command_line.h"
#include "base/test/null_task_runner.h"
#include "components/proximity_auth/cryptauth/cryptauth_access_token_fetcher.h"
#include "components/proximity_auth/cryptauth/cryptauth_api_call_flow.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/switches.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace proximity_auth {

namespace {

const char kTestGoogleApisUrl[] = "https://www.testgoogleapis.com";
const char kAccessToken[] = "access_token";
const char kPublicKey1[] = "public_key1";
const char kPublicKey2[] = "public_key2";
const char kBluetoothAddress1[] = "AA:AA:AA:AA:AA:AA";
const char kBluetoothAddress2[] = "BB:BB:BB:BB:BB:BB";

// CryptAuthAccessTokenFetcher implementation simply returning a predetermined
// access token.
class FakeCryptAuthAccessTokenFetcher : public CryptAuthAccessTokenFetcher {
 public:
  FakeCryptAuthAccessTokenFetcher() : access_token_(kAccessToken) {}

  void FetchAccessToken(const AccessTokenCallback& callback) override {
    callback.Run(access_token_);
  }

  void set_access_token(const std::string& access_token) {
    access_token_ = access_token;
  };

 private:
  std::string access_token_;
};

// Mock CryptAuthApiCallFlow, which handles the HTTP requests to CryptAuth.
class MockCryptAuthApiCallFlow : public CryptAuthApiCallFlow {
 public:
  MockCryptAuthApiCallFlow() : CryptAuthApiCallFlow(GURL(std::string())) {}
  virtual ~MockCryptAuthApiCallFlow() {}

  MOCK_METHOD5(Start,
               void(net::URLRequestContextGetter* context,
                    const std::string& access_token,
                    const std::string& serialized_request,
                    const ResultCallback& result_callback,
                    const ErrorCallback& error_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCryptAuthApiCallFlow);
};

// Subclass of CryptAuthClient to use as test harness.
class MockCryptAuthClient : public CryptAuthClient {
 public:
  // Ownership of |access_token_fetcher| is passed to the superclass. Due to the
  // limitations of gmock, we need to use a raw pointer argument rather than a
  // scoped_ptr.
  MockCryptAuthClient(
      CryptAuthAccessTokenFetcher* access_token_fetcher,
      scoped_refptr<net::URLRequestContextGetter> url_request_context)
      : CryptAuthClient(make_scoped_ptr(access_token_fetcher),
                        url_request_context) {}
  virtual ~MockCryptAuthClient() {}

  MOCK_METHOD1(CreateFlowProxy, CryptAuthApiCallFlow*(const GURL& request_url));

  scoped_ptr<CryptAuthApiCallFlow> CreateFlow(
      const GURL& request_url) override {
    return make_scoped_ptr(CreateFlowProxy(request_url));
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCryptAuthClient);
};

// Callback that should never be invoked.
template <class T>
void NotCalled(const T& type) {
  EXPECT_TRUE(false);
}

// Callback that saves the result returned by CryptAuthClient.
template <class T>
void SaveResult(T* out, const T& result) {
  *out = result;
}

}  // namespace

class ProximityAuthCryptAuthClientTest : public testing::Test {
 protected:
  ProximityAuthCryptAuthClientTest()
      : access_token_fetcher_(new FakeCryptAuthAccessTokenFetcher()),
        url_request_context_(
            new net::TestURLRequestContextGetter(new base::NullTaskRunner())),
        serialized_request_(std::string()) {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kCryptAuthHTTPHost, kTestGoogleApisUrl);

    client_.reset(new StrictMock<MockCryptAuthClient>(access_token_fetcher_,
                                                      url_request_context_));
  }

  // Sets up an expectation and captures a CryptAuth API request to
  // |request_url|.
  void ExpectRequest(const std::string& request_url) {
    StrictMock<MockCryptAuthApiCallFlow>* api_call_flow =
        new StrictMock<MockCryptAuthApiCallFlow>();

    EXPECT_CALL(*client_, CreateFlowProxy(GURL(request_url)))
        .WillOnce(Return(api_call_flow));

    EXPECT_CALL(*api_call_flow,
                Start(url_request_context_.get(), kAccessToken, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&serialized_request_),
                        SaveArg<3>(&flow_result_callback_),
                        SaveArg<4>(&flow_error_callback_)));
  }

  // Returns |response_proto| as the result to the current API request.
  // ExpectResult() must have been called first.
  void FinishApiCallFlow(const google::protobuf::MessageLite* response_proto) {
    flow_result_callback_.Run(response_proto->SerializeAsString());
  }

  // Ends the current API request with |error_message|. ExpectResult() must have
  // been called first.
  void FailApiCallFlow(const std::string& error_message) {
    flow_error_callback_.Run(error_message);
  }

 protected:
  // Owned by |client_|.
  FakeCryptAuthAccessTokenFetcher* access_token_fetcher_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  scoped_ptr<StrictMock<MockCryptAuthClient>> client_;

  std::string serialized_request_;
  CryptAuthApiCallFlow::ResultCallback flow_result_callback_;
  CryptAuthApiCallFlow::ErrorCallback flow_error_callback_;
};

TEST_F(ProximityAuthCryptAuthClientTest, GetMyDevicesSuccess) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices?alt=proto");

  cryptauth::GetMyDevicesResponse result_proto;
  cryptauth::GetMyDevicesRequest request_proto;
  request_proto.set_allow_stale_read(true);
  client_->GetMyDevices(
      request_proto,
      base::Bind(&SaveResult<cryptauth::GetMyDevicesResponse>, &result_proto),
      base::Bind(&NotCalled<std::string>));

  cryptauth::GetMyDevicesRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_TRUE(expected_request.allow_stale_read());

  // Return two devices, one unlock key and one unlockable device.
  {
    cryptauth::GetMyDevicesResponse response_proto;
    response_proto.add_devices();
    response_proto.mutable_devices(0)->set_public_key(kPublicKey1);
    response_proto.mutable_devices(0)->set_unlock_key(true);
    response_proto.mutable_devices(0)
        ->set_bluetooth_address(kBluetoothAddress1);
    response_proto.add_devices();
    response_proto.mutable_devices(1)->set_public_key(kPublicKey2);
    response_proto.mutable_devices(1)->set_unlockable(true);
    FinishApiCallFlow(&response_proto);
  }

  // Check that the result received in callback is the same as the response.
  ASSERT_EQ(2, result_proto.devices_size());
  EXPECT_EQ(kPublicKey1, result_proto.devices(0).public_key());
  EXPECT_TRUE(result_proto.devices(0).unlock_key());
  EXPECT_EQ(kBluetoothAddress1, result_proto.devices(0).bluetooth_address());
  EXPECT_EQ(kPublicKey2, result_proto.devices(1).public_key());
  EXPECT_TRUE(result_proto.devices(1).unlockable());
}

TEST_F(ProximityAuthCryptAuthClientTest, GetMyDevicesFailure) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices?alt=proto");

  std::string error_message;
  client_->GetMyDevices(cryptauth::GetMyDevicesRequest(),
                        base::Bind(&NotCalled<cryptauth::GetMyDevicesResponse>),
                        base::Bind(&SaveResult<std::string>, &error_message));

  std::string kStatus500Error("HTTP status: 500");
  FailApiCallFlow(kStatus500Error);
  EXPECT_EQ(kStatus500Error, error_message);
}

TEST_F(ProximityAuthCryptAuthClientTest, FindEligibleUnlockDevicesSuccess) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "findeligibleunlockdevices?alt=proto");

  cryptauth::FindEligibleUnlockDevicesResponse result_proto;
  cryptauth::FindEligibleUnlockDevicesRequest request_proto;
  request_proto.set_callback_bluetooth_address(kBluetoothAddress2);
  client_->FindEligibleUnlockDevices(
      request_proto,
      base::Bind(&SaveResult<cryptauth::FindEligibleUnlockDevicesResponse>,
                 &result_proto),
      base::Bind(&NotCalled<std::string>));

  cryptauth::FindEligibleUnlockDevicesRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(kBluetoothAddress2, expected_request.callback_bluetooth_address());

  // Return a response proto with one eligible and one ineligible device.
  cryptauth::FindEligibleUnlockDevicesResponse response_proto;
  response_proto.add_eligible_devices();
  response_proto.mutable_eligible_devices(0)->set_public_key(kPublicKey1);

  const std::string kIneligibilityReason = "You require more vespine gas.";
  response_proto.add_ineligible_devices();
  response_proto.mutable_ineligible_devices(0)
      ->mutable_device()
      ->set_public_key(kPublicKey2);
  response_proto.mutable_ineligible_devices(0)
      ->add_reasons(kIneligibilityReason);
  FinishApiCallFlow(&response_proto);

  // Check that the result received in callback is the same as the response.
  ASSERT_EQ(1, result_proto.eligible_devices_size());
  EXPECT_EQ(kPublicKey1, result_proto.eligible_devices(0).public_key());
  ASSERT_EQ(1, result_proto.ineligible_devices_size());
  EXPECT_EQ(kPublicKey2,
            result_proto.ineligible_devices(0).device().public_key());
  ASSERT_EQ(1, result_proto.ineligible_devices(0).reasons_size());
  EXPECT_EQ(kIneligibilityReason,
            result_proto.ineligible_devices(0).reasons(0));
}

TEST_F(ProximityAuthCryptAuthClientTest, FindEligibleUnlockDevicesFailure) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "findeligibleunlockdevices?alt=proto");

  std::string error_message;
  cryptauth::FindEligibleUnlockDevicesRequest request_proto;
  request_proto.set_callback_bluetooth_address(kBluetoothAddress1);
  client_->FindEligibleUnlockDevices(
      request_proto,
      base::Bind(&NotCalled<cryptauth::FindEligibleUnlockDevicesResponse>),
      base::Bind(&SaveResult<std::string>, &error_message));

  std::string kStatus403Error("HTTP status: 403");
  FailApiCallFlow(kStatus403Error);
  EXPECT_EQ(kStatus403Error, error_message);
}

TEST_F(ProximityAuthCryptAuthClientTest, SendDeviceSyncTickleSuccess) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "senddevicesynctickle?alt=proto");

  cryptauth::SendDeviceSyncTickleResponse result_proto;
  client_->SendDeviceSyncTickle(
      cryptauth::SendDeviceSyncTickleRequest(),
      base::Bind(&SaveResult<cryptauth::SendDeviceSyncTickleResponse>,
                 &result_proto),
      base::Bind(&NotCalled<std::string>));

  cryptauth::SendDeviceSyncTickleRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));

  cryptauth::SendDeviceSyncTickleResponse response_proto;
  FinishApiCallFlow(&response_proto);
}

TEST_F(ProximityAuthCryptAuthClientTest, ToggleEasyUnlockSuccess) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "toggleeasyunlock?alt=proto");

  cryptauth::ToggleEasyUnlockResponse result_proto;
  cryptauth::ToggleEasyUnlockRequest request_proto;
  request_proto.set_enable(true);
  request_proto.set_apply_to_all(false);
  request_proto.set_public_key(kPublicKey1);
  client_->ToggleEasyUnlock(
      request_proto,
      base::Bind(&SaveResult<cryptauth::ToggleEasyUnlockResponse>,
                 &result_proto),
      base::Bind(&NotCalled<std::string>));

  cryptauth::ToggleEasyUnlockRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_TRUE(expected_request.enable());
  EXPECT_EQ(kPublicKey1, expected_request.public_key());
  EXPECT_FALSE(expected_request.apply_to_all());

  cryptauth::ToggleEasyUnlockResponse response_proto;
  FinishApiCallFlow(&response_proto);
}

TEST_F(ProximityAuthCryptAuthClientTest, SetupEnrollmentSuccess) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/enrollment/"
      "setupenrollment?alt=proto");

  std::string kApplicationId = "mkaes";
  std::vector<std::string> supported_protocols;
  supported_protocols.push_back("gcmV1");
  supported_protocols.push_back("testProtocol");

  cryptauth::SetupEnrollmentResponse result_proto;
  cryptauth::SetupEnrollmentRequest request_proto;
  request_proto.set_application_id(kApplicationId);
  request_proto.add_types("gcmV1");
  request_proto.add_types("testProtocol");
  client_->SetupEnrollment(
      request_proto, base::Bind(&SaveResult<cryptauth::SetupEnrollmentResponse>,
                                &result_proto),
      base::Bind(&NotCalled<std::string>));

  cryptauth::SetupEnrollmentRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(kApplicationId, expected_request.application_id());
  ASSERT_EQ(2, expected_request.types_size());
  EXPECT_EQ("gcmV1", expected_request.types(0));
  EXPECT_EQ("testProtocol", expected_request.types(1));

  // Return a fake enrollment session.
  {
    cryptauth::SetupEnrollmentResponse response_proto;
    response_proto.set_status("OK");
    response_proto.add_infos();
    response_proto.mutable_infos(0)->set_type("gcmV1");
    response_proto.mutable_infos(0)->set_enrollment_session_id("session_id");
    response_proto.mutable_infos(0)->set_server_ephemeral_key("ephemeral_key");
    FinishApiCallFlow(&response_proto);
  }

  // Check that the returned proto is the same as the one just created.
  EXPECT_EQ("OK", result_proto.status());
  ASSERT_EQ(1, result_proto.infos_size());
  EXPECT_EQ("gcmV1", result_proto.infos(0).type());
  EXPECT_EQ("session_id", result_proto.infos(0).enrollment_session_id());
  EXPECT_EQ("ephemeral_key", result_proto.infos(0).server_ephemeral_key());
}

TEST_F(ProximityAuthCryptAuthClientTest, FinishEnrollmentSuccess) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/enrollment/"
      "finishenrollment?alt=proto");

  const char kEnrollmentSessionId[] = "enrollment_session_id";
  const char kEnrollmentMessage[] = "enrollment_message";
  const char kDeviceEphemeralKey[] = "device_ephermal_key";
  cryptauth::FinishEnrollmentResponse result_proto;
  cryptauth::FinishEnrollmentRequest request_proto;
  request_proto.set_enrollment_session_id(kEnrollmentSessionId);
  request_proto.set_enrollment_message(kEnrollmentMessage);
  request_proto.set_device_ephemeral_key(kDeviceEphemeralKey);
  client_->FinishEnrollment(
      request_proto,
      base::Bind(&SaveResult<cryptauth::FinishEnrollmentResponse>,
                 &result_proto),
      base::Bind(&NotCalled<const std::string&>));

  cryptauth::FinishEnrollmentRequest expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request_));
  EXPECT_EQ(kEnrollmentSessionId, expected_request.enrollment_session_id());
  EXPECT_EQ(kEnrollmentMessage, expected_request.enrollment_message());
  EXPECT_EQ(kDeviceEphemeralKey, expected_request.device_ephemeral_key());

  {
    cryptauth::FinishEnrollmentResponse response_proto;
    response_proto.set_status("OK");
    FinishApiCallFlow(&response_proto);
  }
  EXPECT_EQ("OK", result_proto.status());
}

TEST_F(ProximityAuthCryptAuthClientTest, FetchAccessTokenFailure) {
  access_token_fetcher_->set_access_token("");

  std::string error_message;
  client_->GetMyDevices(cryptauth::GetMyDevicesRequest(),
                        base::Bind(&NotCalled<cryptauth::GetMyDevicesResponse>),
                        base::Bind(&SaveResult<std::string>, &error_message));

  EXPECT_EQ("Failed to get a valid access token.", error_message);
}

TEST_F(ProximityAuthCryptAuthClientTest, ParseResponseProtoFailure) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices?alt=proto");

  std::string error_message;
  client_->GetMyDevices(cryptauth::GetMyDevicesRequest(),
                        base::Bind(&NotCalled<cryptauth::GetMyDevicesResponse>),
                        base::Bind(&SaveResult<std::string>, &error_message));

  flow_result_callback_.Run("Not a valid serialized response message.");
  EXPECT_EQ("Failed to parse response proto.", error_message);
}

TEST_F(ProximityAuthCryptAuthClientTest,
       MakeSecondRequestBeforeFirstRequestSucceeds) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices?alt=proto");

  // Make first request.
  cryptauth::GetMyDevicesResponse result_proto;
  client_->GetMyDevices(
      cryptauth::GetMyDevicesRequest(),
      base::Bind(&SaveResult<cryptauth::GetMyDevicesResponse>, &result_proto),
      base::Bind(&NotCalled<std::string>));

  // With request pending, make second request.
  {
    std::string error_message;
    client_->FindEligibleUnlockDevices(
        cryptauth::FindEligibleUnlockDevicesRequest(),
        base::Bind(&NotCalled<cryptauth::FindEligibleUnlockDevicesResponse>),
        base::Bind(&SaveResult<std::string>, &error_message));
    EXPECT_EQ("Client has been used for another request. Do not reuse.",
              error_message);
  }

  // Complete first request.
  {
    cryptauth::GetMyDevicesResponse response_proto;
    response_proto.add_devices();
    response_proto.mutable_devices(0)->set_public_key(kPublicKey1);
    FinishApiCallFlow(&response_proto);
  }

  ASSERT_EQ(1, result_proto.devices_size());
  EXPECT_EQ(kPublicKey1, result_proto.devices(0).public_key());
}

TEST_F(ProximityAuthCryptAuthClientTest,
       MakeSecondRequestBeforeFirstRequestFails) {
  ExpectRequest(
      "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
      "getmydevices?alt=proto");

  // Make first request.
  std::string error_message;
  client_->GetMyDevices(cryptauth::GetMyDevicesRequest(),
                        base::Bind(&NotCalled<cryptauth::GetMyDevicesResponse>),
                        base::Bind(&SaveResult<std::string>, &error_message));

  // With request pending, make second request.
  {
    std::string error_message;
    client_->FindEligibleUnlockDevices(
        cryptauth::FindEligibleUnlockDevicesRequest(),
        base::Bind(&NotCalled<cryptauth::FindEligibleUnlockDevicesResponse>),
        base::Bind(&SaveResult<std::string>, &error_message));
    EXPECT_EQ("Client has been used for another request. Do not reuse.",
              error_message);
  }

  // Fail first request.
  std::string kStatus429Error = "HTTP status: 429";
  FailApiCallFlow(kStatus429Error);
  EXPECT_EQ(kStatus429Error, error_message);
}

TEST_F(ProximityAuthCryptAuthClientTest,
       MakeSecondRequestAfterFirstRequestSucceeds) {
  // Make first request successfully.
  {
    ExpectRequest(
        "https://www.testgoogleapis.com/cryptauth/v1/deviceSync/"
        "getmydevices?alt=proto");
    cryptauth::GetMyDevicesResponse result_proto;
    client_->GetMyDevices(
        cryptauth::GetMyDevicesRequest(),
        base::Bind(&SaveResult<cryptauth::GetMyDevicesResponse>, &result_proto),
        base::Bind(&NotCalled<std::string>));

    cryptauth::GetMyDevicesResponse response_proto;
    response_proto.add_devices();
    response_proto.mutable_devices(0)->set_public_key(kPublicKey1);
    FinishApiCallFlow(&response_proto);
    ASSERT_EQ(1, result_proto.devices_size());
    EXPECT_EQ(kPublicKey1, result_proto.devices(0).public_key());
  }

  // Second request fails.
  {
    std::string error_message;
    client_->FindEligibleUnlockDevices(
        cryptauth::FindEligibleUnlockDevicesRequest(),
        base::Bind(&NotCalled<cryptauth::FindEligibleUnlockDevicesResponse>),
        base::Bind(&SaveResult<std::string>, &error_message));
    EXPECT_EQ("Client has been used for another request. Do not reuse.",
              error_message);
  }
}

}  // namespace proximity_auth
