// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_store_chromeos.h"

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "content/test/test_browser_thread.h"
#include "policy/policy_constants.h"

namespace em = enterprise_management;

using testing::AllOf;
using testing::Eq;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace policy {

namespace {

const char kFakeDeviceId[] = "device-id";
const char kFakeMachineName[] = "machine-name";
const char kFakeSignature[] = "signature";
const char kFakeToken[] = "token";
const char kFakeUsername[] = "username@example.com";
const char kLegacyDeviceId[] = "legacy-device-id";
const char kLegacyToken[] = "legacy-token";
const int kFakePublicKeyVersion = 17;

class MockCloudPolicyStoreObserver : public CloudPolicyStore::Observer {
 public:
  MockCloudPolicyStoreObserver() {}
  virtual ~MockCloudPolicyStoreObserver() {}

  MOCK_METHOD1(OnStoreLoaded, void(CloudPolicyStore* store));
  MOCK_METHOD1(OnStoreError, void(CloudPolicyStore* store));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyStoreObserver);
};

class UserCloudPolicyStoreChromeOSTest : public testing::Test {
 protected:
  UserCloudPolicyStoreChromeOSTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    EXPECT_CALL(*user_manager_.user_manager(), IsUserLoggedIn())
        .WillRepeatedly(Return(true));
    user_manager_.user_manager()->SetLoggedInUser(kFakeUsername, false);
    store_.reset(new UserCloudPolicyStoreChromeOS(&session_manager_client_,
                                                  token_file(),
                                                  policy_file()));
    store_->AddObserver(&observer_);
  }

  virtual void TearDown() OVERRIDE {
    store_->RemoveObserver(&observer_);
    store_.reset();
    loop_.RunAllPending();
  }

  void CreatePolicyData(em::PolicyData* policy_data) {
    em::CloudPolicySettings policy_settings;
    policy_settings.mutable_showhomebutton()->set_showhomebutton(true);

    policy_data->set_policy_type(dm_protocol::kChromeUserPolicyType);
    policy_data->set_timestamp((base::Time::NowFromSystemTime() -
                               base::Time::UnixEpoch()).InMilliseconds());
    policy_data->set_request_token(kFakeToken);
    ASSERT_TRUE(
        policy_settings.SerializeToString(policy_data->mutable_policy_value()));
    policy_data->set_machine_name(kFakeMachineName);
    policy_data->set_public_key_version(kFakePublicKeyVersion);
    policy_data->set_username(kFakeUsername);
    policy_data->set_device_id(kFakeDeviceId);
    policy_data->set_state(em::PolicyData::ACTIVE);
  }

  void CreatePolicyResponse(em::PolicyFetchResponse* response) {
    em::PolicyData policy_data;
    ASSERT_NO_FATAL_FAILURE(CreatePolicyData(&policy_data));
    ASSERT_TRUE(policy_data.SerializeToString(response->mutable_policy_data()));
    response->set_policy_data_signature(kFakeSignature);
  }

  // Install an expectation on |observer_| for an error code.
  void ExpectError(CloudPolicyStore::Status error) {
    EXPECT_CALL(observer_,
                OnStoreError(AllOf(Eq(store_.get()),
                                   Property(&CloudPolicyStore::status,
                                            Eq(error)))));
  }

  // Triggers a store_->Load() operation, handles the expected call to
  // |session_manager_client_| and sends |response|.
  void PerformPolicyLoad(const std::string& response) {
    // Issue a load command.
    chromeos::SessionManagerClient::RetrievePolicyCallback retrieve_callback;
    EXPECT_CALL(session_manager_client_, RetrieveUserPolicy(_))
        .WillOnce(SaveArg<0>(&retrieve_callback));
    store_->Load();
    loop_.RunAllPending();
    ASSERT_FALSE(retrieve_callback.is_null());

    // Run the callback.
    retrieve_callback.Run(response);
    loop_.RunAllPending();
  }

  // Verifies that store_->policy_map() has the ShowHomeButton entry as created
  // by CreatePolicyResponse().
  void VerifyPolicyMap() {
    EXPECT_EQ(1U, store_->policy_map().size());
    const PolicyMap::Entry* entry =
        store_->policy_map().Get(key::kShowHomeButton);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(base::FundamentalValue(true).Equals(entry->value));
  }

  FilePath token_file() {
    return tmp_dir_.path().AppendASCII("token");
  }

  FilePath policy_file() {
    return tmp_dir_.path().AppendASCII("policy");
  }

  MessageLoop loop_;
  chromeos::MockSessionManagerClient session_manager_client_;
  MockCloudPolicyStoreObserver observer_;
  scoped_ptr<UserCloudPolicyStoreChromeOS> store_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  ScopedTempDir tmp_dir_;
  chromeos::ScopedMockUserManagerEnabler user_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreChromeOSTest);
};

TEST_F(UserCloudPolicyStoreChromeOSTest, Store) {
  em::PolicyFetchResponse policy;
  ASSERT_NO_FATAL_FAILURE(CreatePolicyResponse(&policy));

  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  EXPECT_CALL(session_manager_client_,
              StoreUserPolicy(policy.SerializeAsString(), _))
      .WillOnce(SaveArg<1>(&store_callback));
  store_->Store(policy);
  loop_.RunAllPending();

  // No policy should be present yet.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  ASSERT_FALSE(store_callback.is_null());

  // Let the store operation complete.
  chromeos::SessionManagerClient::RetrievePolicyCallback retrieve_callback;
  EXPECT_CALL(session_manager_client_, RetrieveUserPolicy(_))
      .WillOnce(SaveArg<0>(&retrieve_callback));
  store_callback.Run(true);
  loop_.RunAllPending();
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  ASSERT_FALSE(retrieve_callback.is_null());

  // Finish the retrieve callback.
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  retrieve_callback.Run(policy.SerializeAsString());
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy.policy_data(), store_->policy()->SerializeAsString());
  VerifyPolicyMap();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreFail) {
  em::PolicyFetchResponse policy;
  ASSERT_NO_FATAL_FAILURE(CreatePolicyResponse(&policy));

  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  EXPECT_CALL(session_manager_client_,
              StoreUserPolicy(policy.SerializeAsString(), _))
      .WillOnce(SaveArg<1>(&store_callback));
  store_->Store(policy);
  loop_.RunAllPending();

  // Let the store operation complete.
  ASSERT_FALSE(store_callback.is_null());
  ExpectError(CloudPolicyStore::STATUS_PERSIST_STORE_ERROR);
  store_callback.Run(false);
  loop_.RunAllPending();
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_PERSIST_STORE_ERROR, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, Load) {
  em::PolicyFetchResponse policy;
  ASSERT_NO_FATAL_FAILURE(CreatePolicyResponse(&policy));
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy.SerializeAsString()));

  // Verify that the policy has been loaded.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy.policy_data(), store_->policy()->SerializeAsString());
  VerifyPolicyMap();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadNoPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadInvalidPolicy) {
  ExpectError(CloudPolicyStore::STATUS_PERSIST_PARSE_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad("invalid"));

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_PERSIST_PARSE_ERROR, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationFull) {
  testing::Sequence seq;
  std::string data;

  em::DeviceCredentials credentials;
  credentials.set_device_token(kLegacyToken);
  credentials.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(credentials.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(token_file(), data.c_str(), data.size()));

  em::CachedCloudPolicyResponse policy;
  ASSERT_NO_FATAL_FAILURE(CreatePolicyResponse(policy.mutable_cloud_policy()));
  ASSERT_TRUE(policy.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(policy_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));

  // Verify that legacy user policy and token have been loaded.
  em::PolicyData expected_policy_data;
  EXPECT_TRUE(expected_policy_data.ParseFromString(
                  policy.cloud_policy().policy_data()));
  expected_policy_data.clear_public_key_version();
  expected_policy_data.set_request_token(kLegacyToken);
  expected_policy_data.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(expected_policy_data.SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
};

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationNoToken) {
  testing::Sequence seq;
  std::string data;

  em::CachedCloudPolicyResponse policy;
  ASSERT_NO_FATAL_FAILURE(CreatePolicyResponse(policy.mutable_cloud_policy()));
  ASSERT_TRUE(policy.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(policy_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));

  // Verify the legacy cache has been loaded.
  em::PolicyData expected_policy_data;
  EXPECT_TRUE(expected_policy_data.ParseFromString(
                  policy.cloud_policy().policy_data()));
  expected_policy_data.clear_public_key_version();
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(expected_policy_data.SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
};

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationNoPolicy) {
  testing::Sequence seq;
  std::string data;

  em::DeviceCredentials credentials;
  credentials.set_device_token(kLegacyToken);
  credentials.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(credentials.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(token_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));

  // Verify that legacy user policy and token have been loaded.
  em::PolicyData expected_policy_data;
  expected_policy_data.set_request_token(kLegacyToken);
  expected_policy_data.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(expected_policy_data.SerializeAsString(),
            store_->policy()->SerializeAsString());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
};

// For testing PolicyFetchResponse/PolicyData error conditions.
struct PolicyErrorTestParam {
  // A function to modify em::PolicyData to introduce an error.
  typedef void(*PreparePolicyDataFunction)(em::PolicyData*);
  PreparePolicyDataFunction prepare_policy_data_function;

  // Called to tamper with em::PolicyFetchResponse to trigger errors.
  typedef void(*PreparePolicyResponseFunction)(em::PolicyFetchResponse*);
  PreparePolicyResponseFunction prepare_policy_response_function;

  // Expected status code.
  CloudPolicyStore::Status expected_status;
};

void TestNoPolicyType(em::PolicyData* policy) {
  policy->clear_policy_type();
}

void TestInvalidPolicyType(em::PolicyData* policy) {
  policy->set_policy_type("invalid");
}

void TestNoTimestamp(em::PolicyData* policy) {
  policy->clear_timestamp();
}

void TestTimestampFromFuture(em::PolicyData* policy) {
  base::Time timestamp(base::Time::NowFromSystemTime() +
                       base::TimeDelta::FromMinutes(5));
  policy->set_timestamp((timestamp - base::Time::UnixEpoch()).InMilliseconds());
}

void TestNoRequestToken(em::PolicyData* policy) {
  policy->clear_request_token();
}

void TestInvalidRequestToken(em::PolicyData* policy) {
  policy->set_request_token("invalid");
}

void TestNoPolicyValue(em::PolicyData* policy) {
  policy->clear_policy_value();
}

void TestInvalidPolicyValue(em::PolicyData* policy) {
  policy->set_policy_value("invalid");
}

void TestNoUsername(em::PolicyData* policy) {
  policy->clear_username();
}

void TestInvalidUsername(em::PolicyData* policy) {
  policy->set_username("invalid");
}

void TestErrorCode(em::PolicyFetchResponse* response) {
  response->set_error_code(42);
}

void TestErrorMessage(em::PolicyFetchResponse* response) {
  response->set_error_message("error!");
}

void TestInvalidPolicyData(em::PolicyFetchResponse* response) {
  response->set_policy_data("invalid");
}

const PolicyErrorTestParam kPolicyErrorTestCases[] = {
  { TestNoPolicyType, NULL,
    CloudPolicyStore::STATUS_VALIDATION_POLICY_TYPE },
  { TestInvalidPolicyType, NULL,
    CloudPolicyStore::STATUS_VALIDATION_POLICY_TYPE },
  { TestNoTimestamp, NULL,
    CloudPolicyStore::STATUS_VALIDATION_TIMESTAMP },
  { TestTimestampFromFuture, NULL,
    CloudPolicyStore::STATUS_VALIDATION_TIMESTAMP },
  { TestNoRequestToken, NULL,
    CloudPolicyStore::STATUS_VALIDATION_TOKEN },
  { TestInvalidRequestToken, NULL,
    CloudPolicyStore::STATUS_VALIDATION_TOKEN },
  { TestNoPolicyValue, NULL,
    CloudPolicyStore::STATUS_VALIDATION_POLICY_PARSE_ERROR },
  { TestInvalidPolicyValue, NULL,
    CloudPolicyStore::STATUS_VALIDATION_POLICY_PARSE_ERROR },
  { TestNoUsername, NULL,
    CloudPolicyStore::STATUS_VALIDATION_USERNAME },
  { TestInvalidUsername, NULL,
    CloudPolicyStore::STATUS_VALIDATION_USERNAME },
  { NULL, TestErrorCode,
    CloudPolicyStore::STATUS_VALIDATION_ERROR_CODE_PRESENT },
  { NULL, TestErrorMessage,
    CloudPolicyStore::STATUS_VALIDATION_ERROR_CODE_PRESENT },
  { NULL, TestInvalidPolicyData,
    CloudPolicyStore::STATUS_VALIDATION_PAYLOAD_PARSE_ERROR },
  // TODO: Add test cases for invalid signatures once that's checked.
};

class UserCloudPolicyStoreChromeOSPolicyErrorTest
    : public UserCloudPolicyStoreChromeOSTest,
      public testing::WithParamInterface<PolicyErrorTestParam> {
 protected:
  virtual void SetUp() OVERRIDE {
    UserCloudPolicyStoreChromeOSTest::SetUp();

    // Start with a valid PolicyData message.
    em::PolicyData policy_data;
    ASSERT_NO_FATAL_FAILURE(CreatePolicyData(&policy_data));
    em::CloudPolicySettings cloud_policy;
    policy_data.set_policy_value(cloud_policy.SerializeAsString());
    ASSERT_TRUE(policy_data.SerializeToString(policy_.mutable_policy_data()));

    // Save the policy in order to bring the store into steady state.
    chromeos::SessionManagerClient::RetrievePolicyCallback retrieve_callback;
    EXPECT_CALL(session_manager_client_, RetrieveUserPolicy(_))
        .WillOnce(SaveArg<0>(&retrieve_callback));
    store_->Load();
    ASSERT_FALSE(retrieve_callback.is_null());
    EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
    retrieve_callback.Run(policy_.SerializeAsString());
    loop_.RunAllPending();

    // Prepare |policy_data| and |policy_| for the actual test. A test-specific
    // function is called to introduce issues into policy_data, which is
    // supposed to trigger errors.
    if (GetParam().prepare_policy_data_function)
      GetParam().prepare_policy_data_function(&policy_data);
    ASSERT_TRUE(policy_data.SerializeToString(policy_.mutable_policy_data()));
    if (GetParam().prepare_policy_response_function)
      GetParam().prepare_policy_response_function(&policy_);

    // Record current values for comparisons after the test.
    ASSERT_TRUE(store_->policy());
    previous_policy_data_.CopyFrom(*store_->policy());
    previous_policy_map_.CopyFrom(store_->policy_map());
  }

  virtual void TearDown() OVERRIDE {
    // Policy metadata and decoded policy shouldn't have changed.
    ASSERT_TRUE(store_->policy());
    EXPECT_EQ(previous_policy_data_.SerializeAsString(),
              store_->policy()->SerializeAsString());
    EXPECT_TRUE(previous_policy_map_.Equals(store_->policy_map()));

    UserCloudPolicyStoreChromeOSTest::TearDown();
  }

  em::PolicyFetchResponse policy_;
  em::PolicyData previous_policy_data_;
  PolicyMap previous_policy_map_;
};

TEST_P(UserCloudPolicyStoreChromeOSPolicyErrorTest, Store) {
  // Try and store the policy blob.
  ExpectError(GetParam().expected_status);
  store_->Store(policy_);
  loop_.RunAllPending();

  // Check status.
  EXPECT_EQ(store_->status(), GetParam().expected_status);
}

TEST_P(UserCloudPolicyStoreChromeOSPolicyErrorTest, Load) {
  // Trigger the load.
  chromeos::SessionManagerClient::RetrievePolicyCallback retrieve_callback;
  EXPECT_CALL(session_manager_client_, RetrieveUserPolicy(_))
      .WillOnce(SaveArg<0>(&retrieve_callback));
  store_->Load();
  loop_.RunAllPending();
  ASSERT_FALSE(retrieve_callback.is_null());

  // Generate a policy load response.
  ExpectError(GetParam().expected_status);
  retrieve_callback.Run(policy_.SerializeAsString());
  loop_.RunAllPending();

  // Verify that the policy has been loaded.
  EXPECT_EQ(store_->status(), GetParam().expected_status);
}

INSTANTIATE_TEST_CASE_P(
    UserCloudPolicyStoreChromeOSPolicyErrorTest,
    UserCloudPolicyStoreChromeOSPolicyErrorTest,
    testing::ValuesIn(kPolicyErrorTestCases));

}  // namespace

}  // namespace policy
