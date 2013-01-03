// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_client.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kClientID[] = "fake-client-id";
const char kMachineID[] = "fake-machine-id";
const char kMachineModel[] = "fake-machine-model";
const char kOAuthToken[] = "fake-oauth-token";
const char kDMToken[] = "fake-dm-token";

class MockObserver : public CloudPolicyClient::Observer {
 public:
  MockObserver() {}
  virtual ~MockObserver() {}

  MOCK_METHOD1(OnPolicyFetched, void(CloudPolicyClient*));
  MOCK_METHOD1(OnRegistrationStateChanged, void(CloudPolicyClient*));
  MOCK_METHOD1(OnClientError, void(CloudPolicyClient*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class MockStatusProvider : public CloudPolicyClient::StatusProvider {
 public:
  MockStatusProvider() {}
  virtual ~MockStatusProvider() {}

  MOCK_METHOD1(GetDeviceStatus, bool(em::DeviceStatusReportRequest* status));
  MOCK_METHOD1(GetSessionStatus, bool(em::SessionStatusReportRequest* status));
  MOCK_METHOD0(OnSubmittedSuccessfully, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStatusProvider);
};

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

}  // namespace

class CloudPolicyClientTest : public testing::Test {
 protected:
  CloudPolicyClientTest()
      : client_id_(kClientID) {
    em::DeviceRegisterRequest* register_request =
        registration_request_.mutable_register_request();
    register_request->set_type(em::DeviceRegisterRequest::USER);
    register_request->set_machine_id(kMachineID);
    register_request->set_machine_model(kMachineModel);
    registration_response_.mutable_register_response()->
        set_device_management_token(kDMToken);

    em::PolicyFetchRequest* policy_fetch_request =
        policy_request_.mutable_policy_request()->add_request();
    policy_fetch_request->set_policy_type(dm_protocol::kChromeUserPolicyType);
    policy_fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
    policy_response_.mutable_policy_response()->add_response()->set_policy_data(
        "fake-policy-data");

    unregistration_request_.mutable_unregister_request();
    unregistration_response_.mutable_unregister_response();
  }

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(status_provider_, GetDeviceStatus(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(status_provider_, GetSessionStatus(_))
        .WillRepeatedly(Return(false));
    CreateClient(USER_AFFILIATION_NONE, CloudPolicyClient::POLICY_TYPE_USER);
  }

  virtual void TearDown() OVERRIDE {
    client_->RemoveObserver(&observer_);
  }

  void Register() {
    EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
    client_->SetupRegistration(kDMToken, client_id_);
  }

  void CreateClient(UserAffiliation user_affiliation,
                    CloudPolicyClient::PolicyType type) {
    if (client_.get())
      client_->RemoveObserver(&observer_);

    client_.reset(new CloudPolicyClient(kMachineID, kMachineModel,
                                        user_affiliation, type,
                                        &status_provider_, &service_));
    client_->AddObserver(&observer_);
  }

  void ExpectRegistration(const std::string& oauth_token) {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION))
        .WillOnce(service_.SucceedJob(registration_response_));
    EXPECT_CALL(service_, StartJob(dm_protocol::kValueRequestRegister,
                                   "", oauth_token, "", "", _,
                                   MatchProto(registration_request_)))
        .WillOnce(SaveArg<5>(&client_id_));
  }

  void ExpectPolicyFetch(const std::string& dm_token,
                         const std::string& user_affiliation) {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
        .WillOnce(service_.SucceedJob(policy_response_));
    EXPECT_CALL(service_, StartJob(dm_protocol::kValueRequestPolicy,
                                   "", "", dm_token, user_affiliation,
                                   client_id_,
                                   MatchProto(policy_request_)));
  }

  void ExpectUnregistration(const std::string& dm_token) {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION))
        .WillOnce(service_.SucceedJob(unregistration_response_));
    EXPECT_CALL(service_, StartJob(dm_protocol::kValueRequestUnregister,
                                   "", "", dm_token, "", client_id_,
                                   MatchProto(unregistration_request_)));
  }

  void CheckPolicyResponse() {
    ASSERT_TRUE(client_->policy());
    EXPECT_THAT(*client_->policy(),
                MatchProto(policy_response_.policy_response().response(0)));
  }

  // Request protobufs used as expectations for the client requests.
  em::DeviceManagementRequest registration_request_;
  em::DeviceManagementRequest policy_request_;
  em::DeviceManagementRequest unregistration_request_;

  // Protobufs used in successful responses.
  em::DeviceManagementResponse registration_response_;
  em::DeviceManagementResponse policy_response_;
  em::DeviceManagementResponse unregistration_response_;

  std::string client_id_;
  MockDeviceManagementService service_;
  StrictMock<MockStatusProvider> status_provider_;
  StrictMock<MockObserver> observer_;
  scoped_ptr<CloudPolicyClient> client_;
};

TEST_F(CloudPolicyClientTest, Init) {
  EXPECT_CALL(service_, CreateJob(_)).Times(0);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->policy());
}

TEST_F(CloudPolicyClientTest, SetupRegistrationAndPolicyFetch) {
  EXPECT_CALL(service_, CreateJob(_)).Times(0);
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->SetupRegistration(kDMToken, client_id_);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->policy());

  ExpectPolicyFetch(kDMToken, dm_protocol::kValueUserAffiliationNone);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  EXPECT_CALL(status_provider_, OnSubmittedSuccessfully());
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, RegistrationAndPolicyFetch) {
  ExpectRegistration(kOAuthToken);
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->Register(kOAuthToken, std::string(), false);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->policy());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  ExpectPolicyFetch(kDMToken, dm_protocol::kValueUserAffiliationNone);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  EXPECT_CALL(status_provider_, OnSubmittedSuccessfully());
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, RegistrationParameters) {
  registration_request_.mutable_register_request()->set_reregister(true);
  registration_request_.mutable_register_request()->set_auto_enrolled(true);
  ExpectRegistration(kOAuthToken);
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->Register(kOAuthToken, kClientID, true);
  EXPECT_EQ(kClientID, client_id_);
}

TEST_F(CloudPolicyClientTest, RegistrationNoToken) {
  registration_response_.mutable_register_response()->
      clear_device_management_token();
  ExpectRegistration(kOAuthToken);
  EXPECT_CALL(observer_, OnClientError(_));
  client_->Register(kOAuthToken, std::string(), false);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->policy());
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->status());
}

TEST_F(CloudPolicyClientTest, RegistrationFailure) {
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION))
      .WillOnce(service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _, _));
  EXPECT_CALL(observer_, OnClientError(_));
  client_->Register(kOAuthToken, std::string(), false);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->policy());
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, PolicyUpdate) {
  Register();

  ExpectPolicyFetch(kDMToken, dm_protocol::kValueUserAffiliationNone);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  EXPECT_CALL(status_provider_, OnSubmittedSuccessfully());
  client_->FetchPolicy();
  CheckPolicyResponse();

  policy_response_.mutable_policy_response()->clear_response();
  policy_response_.mutable_policy_response()->add_response()->set_policy_data(
      "updated-fake-policy-data");
  ExpectPolicyFetch(kDMToken, dm_protocol::kValueUserAffiliationNone);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  EXPECT_CALL(status_provider_, OnSubmittedSuccessfully());
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithMetaData) {
  Register();

  const base::Time timestamp(
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(20));
  client_->set_submit_machine_id(true);
  client_->set_last_policy_timestamp(timestamp);
  client_->set_public_key_version(42);
  em::PolicyFetchRequest* policy_fetch_request =
      policy_request_.mutable_policy_request()->mutable_request(0);
  policy_fetch_request->set_machine_id(kMachineID);
  policy_fetch_request->set_timestamp(
      (timestamp - base::Time::UnixEpoch()).InMilliseconds());
  policy_fetch_request->set_public_key_version(42);

  ExpectPolicyFetch(kDMToken, dm_protocol::kValueUserAffiliationNone);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  EXPECT_CALL(status_provider_, OnSubmittedSuccessfully());
  client_->FetchPolicy();
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, BadPolicyResponse) {
  Register();

  policy_response_.clear_policy_response();
  ExpectPolicyFetch(kDMToken, dm_protocol::kValueUserAffiliationNone);
  EXPECT_CALL(observer_, OnClientError(_));
  client_->FetchPolicy();
  EXPECT_FALSE(client_->policy());
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->status());

  policy_response_.mutable_policy_response()->add_response()->set_policy_data(
      "fake-policy-data");
  policy_response_.mutable_policy_response()->add_response()->set_policy_data(
      "excess-fake-policy-data");
  ExpectPolicyFetch(kDMToken, dm_protocol::kValueUserAffiliationNone);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  EXPECT_CALL(status_provider_, OnSubmittedSuccessfully());
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, PolicyRequestFailure) {
  Register();

  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
      .WillOnce(service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _, _));
  EXPECT_CALL(observer_, OnClientError(_));
  EXPECT_CALL(status_provider_, OnSubmittedSuccessfully()).Times(0);
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
  EXPECT_FALSE(client_->policy());
}

TEST_F(CloudPolicyClientTest, Unregister) {
  Register();

  ExpectUnregistration(kDMToken);
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->Unregister();
  EXPECT_FALSE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UnregisterEmpty) {
  Register();

  unregistration_response_.clear_unregister_response();
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION))
      .WillOnce(service_.SucceedJob(unregistration_response_));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _, _));
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->Unregister();
  EXPECT_FALSE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UnregisterFailure) {
  Register();

  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION))
      .WillOnce(service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _, _));
  EXPECT_CALL(observer_, OnClientError(_));
  client_->Unregister();
  EXPECT_TRUE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

}  // namespace policy
