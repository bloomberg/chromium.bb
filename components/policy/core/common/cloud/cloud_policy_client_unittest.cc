// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Mock;
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
const char kDeviceCertificate[] = "fake-device-certificate";
const char kRequisition[] = "fake-requisition";
const char kStateKey[] = "fake-state-key";
const char kPayload[] = "input_payload";
const char kResultPayload[] = "output_payload";
const char kAssetId[] = "fake-asset-id";
const char kLocation[] = "fake-location";
const char kGcmID[] = "fake-gcm-id";

const int64_t kAgeOfCommand = 123123123;
const int64_t kLastCommandId = 123456789;
const int64_t kTimestamp = 987654321;

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

// A mock class to allow us to set expectations on upload callbacks.
class MockStatusCallbackObserver {
 public:
  MockStatusCallbackObserver() {}

  MOCK_METHOD1(OnCallbackComplete, void(bool));
};

// A mock class to allow us to set expectations on remote command fetch
// callbacks.
class MockRemoteCommandsObserver {
 public:
  MockRemoteCommandsObserver() {}

  MOCK_METHOD2(OnRemoteCommandsFetched,
               void(DeviceManagementStatus,
                    const std::vector<em::RemoteCommand>&));
};

}  // namespace

class CloudPolicyClientTest : public testing::Test {
 protected:
  CloudPolicyClientTest()
      : client_id_(kClientID),
        policy_type_(dm_protocol::kChromeUserPolicyType) {
    em::DeviceRegisterRequest* register_request =
        registration_request_.mutable_register_request();
    register_request->set_type(em::DeviceRegisterRequest::USER);
    register_request->set_machine_id(kMachineID);
    register_request->set_machine_model(kMachineModel);
    register_request->set_flavor(
        em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
    registration_response_.mutable_register_response()->
        set_device_management_token(kDMToken);

    em::PolicyFetchRequest* policy_fetch_request =
        policy_request_.mutable_policy_request()->add_request();
    policy_fetch_request->set_policy_type(dm_protocol::kChromeUserPolicyType);
    policy_fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
    policy_fetch_request->set_verification_key_hash(kPolicyVerificationKeyHash);
    policy_response_.mutable_policy_response()->add_response()->set_policy_data(
        CreatePolicyData("fake-policy-data"));

    unregistration_request_.mutable_unregister_request();
    unregistration_response_.mutable_unregister_response();
    upload_certificate_request_.mutable_cert_upload_request()->
        set_device_certificate(kDeviceCertificate);
    upload_certificate_response_.mutable_cert_upload_response();

    upload_status_request_.mutable_device_status_report_request();
    upload_status_request_.mutable_session_status_report_request();

    remote_command_request_.mutable_remote_command_request()
        ->set_last_command_unique_id(kLastCommandId);
    em::RemoteCommandResult* command_result =
        remote_command_request_.mutable_remote_command_request()
            ->add_command_results();
    command_result->set_unique_id(kLastCommandId);
    command_result->set_result(
        em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
    command_result->set_payload(kResultPayload);
    command_result->set_timestamp(kTimestamp);

    em::RemoteCommand* command =
        remote_command_response_.mutable_remote_command_response()
            ->add_commands();
    command->set_age_of_command(kAgeOfCommand);
    command->set_payload(kPayload);
    command->set_unique_id(kLastCommandId + 1);
    command->set_type(em::RemoteCommand_Type_COMMAND_ECHO_TEST);

    attribute_update_permission_request_.
        mutable_device_attribute_update_permission_request();
    attribute_update_permission_response_.
        mutable_device_attribute_update_permission_response()->
        set_result(
            em::DeviceAttributeUpdatePermissionResponse_ResultType_ATTRIBUTE_UPDATE_ALLOWED);

    attribute_update_request_.mutable_device_attribute_update_request()->
        set_asset_id(kAssetId);
    attribute_update_request_.mutable_device_attribute_update_request()->
        set_location(kLocation);
    attribute_update_response_.mutable_device_attribute_update_response()->
        set_result(
            em::DeviceAttributeUpdateResponse_ResultType_ATTRIBUTE_UPDATE_SUCCESS);

    gcm_id_update_request_.mutable_gcm_id_update_request()->set_gcm_id(kGcmID);
  }

  void SetUp() override {
    CreateClient();
  }

  void TearDown() override {
    client_->RemoveObserver(&observer_);
  }

  void Register() {
    EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
    client_->SetupRegistration(kDMToken, client_id_);
  }

  void CreateClient() {
    if (client_.get())
      client_->RemoveObserver(&observer_);

    request_context_ =
        new net::TestURLRequestContextGetter(loop_.task_runner());
    client_.reset(new CloudPolicyClient(kMachineID, kMachineModel,
                                        kPolicyVerificationKeyHash,
                                        &service_,
                                        request_context_));
    client_->AddPolicyTypeToFetch(policy_type_, std::string());
    client_->AddObserver(&observer_);
  }

  void ExpectRegistration(const std::string& oauth_token) {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION,
                          request_context_))
        .WillOnce(service_.SucceedJob(registration_response_));
    EXPECT_CALL(service_,
                StartJob(dm_protocol::kValueRequestRegister, std::string(),
                         oauth_token, std::string(), _,
                         MatchProto(registration_request_)))
        .WillOnce(SaveArg<4>(&client_id_));
  }

  void ExpectPolicyFetch(const std::string& dm_token) {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH,
                          request_context_))
        .WillOnce(service_.SucceedJob(policy_response_));
    EXPECT_CALL(service_,
                StartJob(dm_protocol::kValueRequestPolicy, std::string(),
                         std::string(), dm_token, client_id_,
                         MatchProto(policy_request_)));
  }

  void ExpectUnregistration(const std::string& dm_token) {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION,
                          request_context_))
        .WillOnce(service_.SucceedJob(unregistration_response_));
    EXPECT_CALL(service_,
                StartJob(dm_protocol::kValueRequestUnregister, std::string(),
                         std::string(), dm_token, client_id_,
                         MatchProto(unregistration_request_)));
  }

  void ExpectUploadCertificate() {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_CERTIFICATE,
                          request_context_))
        .WillOnce(service_.SucceedJob(upload_certificate_response_));
    EXPECT_CALL(service_,
                StartJob(dm_protocol::kValueRequestUploadCertificate,
                         std::string(), std::string(), kDMToken, client_id_,
                         MatchProto(upload_certificate_request_)));
  }

  void ExpectUploadStatus() {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_STATUS,
                          request_context_))
        .WillOnce(service_.SucceedJob(upload_status_response_));
    EXPECT_CALL(service_,
                StartJob(dm_protocol::kValueRequestUploadStatus,
                         std::string(), std::string(), kDMToken,
                         client_id_, MatchProto(upload_status_request_)));
  }

  void ExpectFetchRemoteCommands() {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REMOTE_COMMANDS,
                          request_context_))
        .WillOnce(service_.SucceedJob(remote_command_response_));
    EXPECT_CALL(service_,
                StartJob(dm_protocol::kValueRequestRemoteCommands,
                         std::string(), std::string(), kDMToken,
                         client_id_, MatchProto(remote_command_request_)));
  }

  void ExpectAttributeUpdatePermission(const std::string& oauth_token) {
    EXPECT_CALL(service_,
          CreateJob(
              DeviceManagementRequestJob::TYPE_ATTRIBUTE_UPDATE_PERMISSION,
              request_context_))
          .WillOnce(service_.SucceedJob(attribute_update_permission_response_));
    EXPECT_CALL(service_,
                StartJob(
                    dm_protocol::kValueRequestDeviceAttributeUpdatePermission,
                    std::string(), oauth_token, std::string(),
                    client_id_,
                    MatchProto(attribute_update_permission_request_)));
  }

  void ExpectAttributeUpdate(const std::string& oauth_token) {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_ATTRIBUTE_UPDATE,
                          request_context_))
        .WillOnce(service_.SucceedJob(attribute_update_response_));
    EXPECT_CALL(service_,
                StartJob(dm_protocol::kValueRequestDeviceAttributeUpdate,
                         std::string(), oauth_token, std::string(),
                         client_id_, MatchProto(attribute_update_request_)));
  }

  void ExpectGcmIdUpdate() {
    EXPECT_CALL(service_,
                CreateJob(DeviceManagementRequestJob::TYPE_GCM_ID_UPDATE,
                          request_context_))
        .WillOnce(service_.SucceedJob(gcm_id_update_response_));
    EXPECT_CALL(service_,
                StartJob(dm_protocol::kValueRequestGcmIdUpdate, std::string(),
                         std::string(), kDMToken, client_id_,
                         MatchProto(gcm_id_update_request_)));
  }

  void CheckPolicyResponse() {
    ASSERT_TRUE(client_->GetPolicyFor(policy_type_, std::string()));
    EXPECT_THAT(*client_->GetPolicyFor(policy_type_, std::string()),
                MatchProto(policy_response_.policy_response().response(0)));
  }

  std::string CreatePolicyData(const std::string& policy_value) {
    em::PolicyData policy_data;
    policy_data.set_policy_type(dm_protocol::kChromeUserPolicyType);
    policy_data.set_policy_value(policy_value);
    return policy_data.SerializeAsString();
  }

  // Request protobufs used as expectations for the client requests.
  em::DeviceManagementRequest registration_request_;
  em::DeviceManagementRequest policy_request_;
  em::DeviceManagementRequest unregistration_request_;
  em::DeviceManagementRequest upload_certificate_request_;
  em::DeviceManagementRequest upload_status_request_;
  em::DeviceManagementRequest remote_command_request_;
  em::DeviceManagementRequest attribute_update_permission_request_;
  em::DeviceManagementRequest attribute_update_request_;
  em::DeviceManagementRequest gcm_id_update_request_;

  // Protobufs used in successful responses.
  em::DeviceManagementResponse registration_response_;
  em::DeviceManagementResponse policy_response_;
  em::DeviceManagementResponse unregistration_response_;
  em::DeviceManagementResponse upload_certificate_response_;
  em::DeviceManagementResponse upload_status_response_;
  em::DeviceManagementResponse remote_command_response_;
  em::DeviceManagementResponse attribute_update_permission_response_;
  em::DeviceManagementResponse attribute_update_response_;
  em::DeviceManagementResponse gcm_id_update_response_;

  base::MessageLoop loop_;
  std::string client_id_;
  std::string policy_type_;
  MockDeviceManagementService service_;
  StrictMock<MockCloudPolicyClientObserver> observer_;
  StrictMock<MockStatusCallbackObserver> callback_observer_;
  std::unique_ptr<CloudPolicyClient> client_;
  // Pointer to the client's request context.
  scoped_refptr<net::URLRequestContextGetter> request_context_;
};

TEST_F(CloudPolicyClientTest, Init) {
  EXPECT_CALL(service_, CreateJob(_, _)).Times(0);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(0, client_->fetched_invalidation_version());
}

TEST_F(CloudPolicyClientTest, SetupRegistrationAndPolicyFetch) {
  EXPECT_CALL(service_, CreateJob(_, _)).Times(0);
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->SetupRegistration(kDMToken, client_id_);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));

  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, RegistrationAndPolicyFetch) {
  ExpectRegistration(kOAuthToken);
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->Register(em::DeviceRegisterRequest::USER,
                    em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION,
                    kOAuthToken, std::string(), std::string(), std::string());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, RegistrationParametersPassedThrough) {
  registration_request_.mutable_register_request()->set_reregister(true);
  registration_request_.mutable_register_request()->set_requisition(
      kRequisition);
  registration_request_.mutable_register_request()->set_server_backed_state_key(
      kStateKey);
  registration_request_.mutable_register_request()->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL);
  ExpectRegistration(kOAuthToken);
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->Register(em::DeviceRegisterRequest::USER,
                    em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL,
                    kOAuthToken, kClientID, kRequisition, kStateKey);
  EXPECT_EQ(kClientID, client_id_);
}

TEST_F(CloudPolicyClientTest, RegistrationNoToken) {
  registration_response_.mutable_register_response()->
      clear_device_management_token();
  ExpectRegistration(kOAuthToken);
  EXPECT_CALL(observer_, OnClientError(_));
  client_->Register(em::DeviceRegisterRequest::USER,
                    em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION,
                    kOAuthToken, std::string(), std::string(), std::string());
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->status());
}

TEST_F(CloudPolicyClientTest, RegistrationFailure) {
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION,
                        request_context_))
      .WillOnce(service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  EXPECT_CALL(observer_, OnClientError(_));
  client_->Register(em::DeviceRegisterRequest::USER,
                    em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION,
                    kOAuthToken, std::string(), std::string(), std::string());
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, RetryRegistration) {
  // First registration does not set the re-register flag.
  EXPECT_FALSE(
      registration_request_.mutable_register_request()->has_reregister());
  MockDeviceManagementJob* register_job = nullptr;
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION,
                        request_context_))
      .WillOnce(service_.CreateAsyncJob(&register_job));
  EXPECT_CALL(service_,
              StartJob(dm_protocol::kValueRequestRegister, std::string(),
                       kOAuthToken, std::string(), _,
                       MatchProto(registration_request_)));
  client_->Register(em::DeviceRegisterRequest::USER,
                    em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION,
                    kOAuthToken, std::string(), std::string(), std::string());
  EXPECT_FALSE(client_->is_registered());
  Mock::VerifyAndClearExpectations(&service_);

  // Simulate a retry callback before proceeding; the re-register flag is set.
  registration_request_.mutable_register_request()->set_reregister(true);
  EXPECT_CALL(service_,
              StartJob(dm_protocol::kValueRequestRegister, std::string(),
                       kOAuthToken, std::string(), _,
                       MatchProto(registration_request_)));
  register_job->RetryJob();
  Mock::VerifyAndClearExpectations(&service_);

  // Subsequent retries keep the flag set.
  EXPECT_CALL(service_,
              StartJob(dm_protocol::kValueRequestRegister, std::string(),
                       kOAuthToken, std::string(), _,
                       MatchProto(registration_request_)));
  register_job->RetryJob();
  Mock::VerifyAndClearExpectations(&service_);
}

TEST_F(CloudPolicyClientTest, PolicyUpdate) {
  Register();

  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->FetchPolicy();
  CheckPolicyResponse();

  policy_response_.mutable_policy_response()->clear_response();
  policy_response_.mutable_policy_response()->add_response()->set_policy_data(
      CreatePolicyData("updated-fake-policy-data"));
  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
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

  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->FetchPolicy();
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithInvalidation) {
  Register();

  int64_t previous_version = client_->fetched_invalidation_version();
  client_->SetInvalidationInfo(12345, "12345");
  EXPECT_EQ(previous_version, client_->fetched_invalidation_version());
  em::PolicyFetchRequest* policy_fetch_request =
      policy_request_.mutable_policy_request()->mutable_request(0);
  policy_fetch_request->set_invalidation_version(12345);
  policy_fetch_request->set_invalidation_payload("12345");

  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->FetchPolicy();
  CheckPolicyResponse();
  EXPECT_EQ(12345, client_->fetched_invalidation_version());
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithInvalidationNoPayload) {
  Register();

  int64_t previous_version = client_->fetched_invalidation_version();
  client_->SetInvalidationInfo(-12345, std::string());
  EXPECT_EQ(previous_version, client_->fetched_invalidation_version());

  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->FetchPolicy();
  CheckPolicyResponse();
  EXPECT_EQ(-12345, client_->fetched_invalidation_version());
}

TEST_F(CloudPolicyClientTest, BadPolicyResponse) {
  Register();

  policy_response_.clear_policy_response();
  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnClientError(_));
  client_->FetchPolicy();
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->status());

  policy_response_.mutable_policy_response()->add_response()->set_policy_data(
      CreatePolicyData("fake-policy-data"));
  policy_response_.mutable_policy_response()->add_response()->set_policy_data(
      CreatePolicyData("excess-fake-policy-data"));
  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse();
}

TEST_F(CloudPolicyClientTest, PolicyRequestFailure) {
  Register();

  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH,
                        request_context_))
      .WillOnce(service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  EXPECT_CALL(observer_, OnClientError(_));
  client_->FetchPolicy();
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
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
              CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION,
                        request_context_))
      .WillOnce(service_.SucceedJob(unregistration_response_));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  client_->Unregister();
  EXPECT_FALSE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UnregisterFailure) {
  Register();

  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION,
                        request_context_))
      .WillOnce(service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  EXPECT_CALL(observer_, OnClientError(_));
  client_->Unregister();
  EXPECT_TRUE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithExtensionPolicy) {
  Register();

  // Set up the |expected_responses| and |policy_response_|.
  static const char* kExtensions[] = {
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "cccccccccccccccccccccccccccccccc",
  };
  typedef std::map<std::pair<std::string, std::string>, em::PolicyFetchResponse>
      ResponseMap;
  ResponseMap expected_responses;
  std::set<std::pair<std::string, std::string>> expected_namespaces;
  std::pair<std::string, std::string> key(dm_protocol::kChromeUserPolicyType,
                                          std::string());
  // Copy the user policy fetch request.
  expected_responses[key].CopyFrom(
      policy_response_.policy_response().response(0));
  expected_namespaces.insert(key);
  key.first = dm_protocol::kChromeExtensionPolicyType;
  expected_namespaces.insert(key);
  for (size_t i = 0; i < arraysize(kExtensions); ++i) {
    key.second = kExtensions[i];
    em::PolicyData policy_data;
    policy_data.set_policy_type(key.first);
    policy_data.set_settings_entity_id(key.second);
    expected_responses[key].set_policy_data(policy_data.SerializeAsString());
    policy_response_.mutable_policy_response()->add_response()->CopyFrom(
        expected_responses[key]);
  }

  // Make a policy fetch.
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH,
                        request_context_))
      .WillOnce(service_.SucceedJob(policy_response_));
  EXPECT_CALL(
      service_,
      StartJob(dm_protocol::kValueRequestPolicy, std::string(), std::string(),
               kDMToken, client_id_, _))
      .WillOnce(SaveArg<5>(&policy_request_));
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->AddPolicyTypeToFetch(dm_protocol::kChromeExtensionPolicyType,
                                std::string());
  client_->FetchPolicy();

  // Verify that the request includes the expected namespaces.
  ASSERT_TRUE(policy_request_.has_policy_request());
  const em::DevicePolicyRequest& policy_request =
      policy_request_.policy_request();
  ASSERT_EQ(2, policy_request.request_size());
  for (int i = 0; i < policy_request.request_size(); ++i) {
    const em::PolicyFetchRequest& fetch_request = policy_request.request(i);
    ASSERT_TRUE(fetch_request.has_policy_type());
    EXPECT_FALSE(fetch_request.has_settings_entity_id());
    std::pair<std::string, std::string> key(fetch_request.policy_type(),
                                            std::string());
    EXPECT_EQ(1u, expected_namespaces.erase(key));
  }
  EXPECT_TRUE(expected_namespaces.empty());

  // Verify that the client got all the responses mapped to their namespaces.
  for (ResponseMap::iterator it = expected_responses.begin();
       it != expected_responses.end(); ++it) {
    const em::PolicyFetchResponse* response =
        client_->GetPolicyFor(it->first.first, it->first.second);
    ASSERT_TRUE(response);
    EXPECT_EQ(it->second.SerializeAsString(), response->SerializeAsString());
  }
}

TEST_F(CloudPolicyClientTest, UploadCertificate) {
  Register();

  ExpectUploadCertificate();
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  client_->UploadCertificate(kDeviceCertificate, callback);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadCertificateEmpty) {
  Register();

  upload_certificate_response_.clear_cert_upload_response();
  ExpectUploadCertificate();
  EXPECT_CALL(callback_observer_, OnCallbackComplete(false)).Times(1);
  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  client_->UploadCertificate(kDeviceCertificate, callback);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadCertificateFailure) {
  Register();

  EXPECT_CALL(callback_observer_, OnCallbackComplete(false)).Times(1);
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_CERTIFICATE,
                        request_context_))
      .WillOnce(service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  EXPECT_CALL(observer_, OnClientError(_));
  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  client_->UploadCertificate(kDeviceCertificate, callback);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadStatus) {
  Register();

  ExpectUploadStatus();
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  client_->UploadDeviceStatus(&device_status, &session_status, callback);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadStatusWhilePolicyFetchActive) {
  Register();
  MockDeviceManagementJob* upload_status_job = nullptr;
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_STATUS,
                        request_context_))
      .WillOnce(service_.CreateAsyncJob(&upload_status_job));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  client_->UploadDeviceStatus(&device_status, &session_status, callback);

  // Now initiate a policy fetch - this should not cancel the upload job.
  ExpectPolicyFetch(kDMToken);
  EXPECT_CALL(observer_, OnPolicyFetched(_));
  client_->FetchPolicy();
  CheckPolicyResponse();

  upload_status_job->SendResponse(DM_STATUS_SUCCESS, upload_status_response_);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, MultipleActiveRequests) {
  Register();

  // Set up pending upload status job.
  MockDeviceManagementJob* upload_status_job = nullptr;
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_STATUS,
                        request_context_))
      .WillOnce(service_.CreateAsyncJob(&upload_status_job));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  client_->UploadDeviceStatus(&device_status, &session_status, callback);

  // Set up pending upload certificate job.
  MockDeviceManagementJob* upload_certificate_job = nullptr;
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_CERTIFICATE,
                        request_context_))
      .WillOnce(service_.CreateAsyncJob(&upload_certificate_job));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));

  // Expect two calls on our upload observer, one for the status upload and
  // one for the certificate upload.
  CloudPolicyClient::StatusCallback callback2 = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  client_->UploadCertificate(kDeviceCertificate, callback2);
  EXPECT_EQ(2, client_->GetActiveRequestCountForTest());

  // Now satisfy both active jobs.
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(2);
  upload_status_job->SendResponse(DM_STATUS_SUCCESS, upload_status_response_);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  upload_certificate_job->SendResponse(DM_STATUS_SUCCESS,
                                       upload_certificate_response_);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
}

TEST_F(CloudPolicyClientTest, UploadStatusFailure) {
  Register();

  EXPECT_CALL(callback_observer_, OnCallbackComplete(false)).Times(1);
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_STATUS,
                        request_context_))
      .WillOnce(service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  EXPECT_CALL(observer_, OnClientError(_));
  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));

  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  client_->UploadDeviceStatus(&device_status, &session_status, callback);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestCancelOnUnregister) {
  Register();

  // Set up pending upload status job.
  MockDeviceManagementJob* upload_status_job = nullptr;
  EXPECT_CALL(service_,
              CreateJob(DeviceManagementRequestJob::TYPE_UPLOAD_STATUS,
                        request_context_))
      .WillOnce(service_.CreateAsyncJob(&upload_status_job));
  EXPECT_CALL(service_, StartJob(_, _, _, _, _, _));
  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  client_->UploadDeviceStatus(&device_status, &session_status, callback);
  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());
  EXPECT_CALL(observer_, OnRegistrationStateChanged(_));
  ExpectUnregistration(kDMToken);
  client_->Unregister();
  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
}

TEST_F(CloudPolicyClientTest, FetchRemoteCommands) {
  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;

  Register();

  ExpectFetchRemoteCommands();
  EXPECT_CALL(
      remote_commands_observer,
      OnRemoteCommandsFetched(
          DM_STATUS_SUCCESS,
          ElementsAre(MatchProto(
              remote_command_response_.remote_command_response().commands(0)))))
      .Times(1);
  const CloudPolicyClient::RemoteCommandCallback callback =
      base::Bind(&MockRemoteCommandsObserver::OnRemoteCommandsFetched,
                 base::Unretained(&remote_commands_observer));

  const std::vector<em::RemoteCommandResult> command_results(
      1, remote_command_request_.remote_command_request().command_results(0));
  client_->FetchRemoteCommands(
      base::WrapUnique(new RemoteCommandJob::UniqueIDType(kLastCommandId)),
      command_results, callback);

  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestDeviceAttributeUpdatePermission) {
  Register();
  ExpectAttributeUpdatePermission(kOAuthToken);
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback = base::Bind(
      &MockStatusCallbackObserver::OnCallbackComplete,
      base::Unretained(&callback_observer_));
  client_->GetDeviceAttributeUpdatePermission(kOAuthToken, callback);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestDeviceAttributeUpdate) {
  Register();
  ExpectAttributeUpdate(kOAuthToken);
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::Bind(&MockStatusCallbackObserver::OnCallbackComplete,
                 base::Unretained(&callback_observer_));
  client_->UpdateDeviceAttributes(kOAuthToken, kAssetId, kLocation, callback);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestGcmIdUpdate) {
  Register();
  ExpectGcmIdUpdate();
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::Bind(&MockStatusCallbackObserver::OnCallbackComplete,
                 base::Unretained(&callback_observer_));
  client_->UpdateGcmId(kGcmID, callback);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

}  // namespace policy
