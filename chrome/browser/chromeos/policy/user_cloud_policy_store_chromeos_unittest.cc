// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "policy/policy_constants.h"
#include "policy/proto/cloud_policy.pb.h"
#include "policy/proto/device_management_local.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AllOf;
using testing::AnyNumber;
using testing::Eq;
using testing::Mock;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace policy {

namespace {

const char kLegacyDeviceId[] = "legacy-device-id";
const char kLegacyToken[] = "legacy-token";
const char kSanitizedUsername[] =
    "0123456789ABCDEF0123456789ABCDEF012345678@example.com";
const char kDefaultHomepage[] = "http://chromium.org";

ACTION_P2(SendSanitizedUsername, call_status, sanitized_username) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(arg1, call_status, sanitized_username));
}

class UserCloudPolicyStoreChromeOSTest : public testing::Test {
 protected:
  UserCloudPolicyStoreChromeOSTest() {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(cryptohome_client_,
                GetSanitizedUsername(PolicyBuilder::kFakeUsername, _))
        .Times(AnyNumber())
        .WillRepeatedly(
            SendSanitizedUsername(chromeos::DBUS_METHOD_CALL_SUCCESS,
                                  kSanitizedUsername));

    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    store_.reset(new UserCloudPolicyStoreChromeOS(&cryptohome_client_,
                                                  &session_manager_client_,
                                                  loop_.message_loop_proxy(),
                                                  PolicyBuilder::kFakeUsername,
                                                  user_policy_dir(),
                                                  token_file(),
                                                  policy_file()));
    store_->AddObserver(&observer_);

    // Install the initial public key, so that by default the validation of
    // the stored/loaded policy blob succeeds.
    std::vector<uint8> public_key;
    ASSERT_TRUE(policy_.GetSigningKey()->ExportPublicKey(&public_key));
    StoreUserPolicyKey(public_key);

    policy_.payload().mutable_homepagelocation()->set_value(kDefaultHomepage);
    policy_.Build();
  }

  virtual void TearDown() OVERRIDE {
    store_->RemoveObserver(&observer_);
    store_.reset();
    RunUntilIdle();
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
    EXPECT_CALL(session_manager_client_,
                RetrievePolicyForUser(PolicyBuilder::kFakeUsername, _))
        .WillOnce(SaveArg<1>(&retrieve_callback));
    store_->Load();
    RunUntilIdle();
    Mock::VerifyAndClearExpectations(&session_manager_client_);
    ASSERT_FALSE(retrieve_callback.is_null());

    // Run the callback.
    retrieve_callback.Run(response);
    RunUntilIdle();
  }

  // Verifies that store_->policy_map() has the HomepageLocation entry with
  // the |expected_value|.
  void VerifyPolicyMap(const char* expected_value) {
    EXPECT_EQ(1U, store_->policy_map().size());
    const PolicyMap::Entry* entry =
        store_->policy_map().Get(key::kHomepageLocation);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(base::StringValue(expected_value).Equals(entry->value));
  }

  void StoreUserPolicyKey(const std::vector<uint8>& public_key) {
    ASSERT_TRUE(base::CreateDirectory(user_policy_key_file().DirName()));
    ASSERT_TRUE(
        base::WriteFile(user_policy_key_file(),
                        reinterpret_cast<const char*>(public_key.data()),
                        public_key.size()));
  }

  // Stores the current |policy_| and verifies that it is published.
  // If |new_public_key| is set then it will be persisted after storing but
  // before loading the policy, so that the signature validation can succeed.
  // If |previous_value| is set then a previously existing policy with that
  // value will be expected; otherwise no previous policy is expected.
  // If |new_value| is set then a new policy with that value is expected after
  // storing the |policy_| blob.
  void PerformStorePolicy(const std::vector<uint8>* new_public_key,
                          const char* previous_value,
                          const char* new_value) {
    chromeos::SessionManagerClient::StorePolicyCallback store_callback;
    EXPECT_CALL(session_manager_client_,
                StorePolicyForUser(PolicyBuilder::kFakeUsername,
                                   policy_.GetBlob(), _))
        .WillOnce(SaveArg<2>(&store_callback));
    store_->Store(policy_.policy());
    RunUntilIdle();
    Mock::VerifyAndClearExpectations(&session_manager_client_);
    ASSERT_FALSE(store_callback.is_null());

    // The new policy shouldn't be present yet.
    PolicyMap previous_policy;
    EXPECT_EQ(previous_value != NULL, store_->policy() != NULL);
    if (previous_value) {
      previous_policy.Set(key::kHomepageLocation,
                          POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_USER,
                          new base::StringValue(previous_value), NULL);
    }
    EXPECT_TRUE(previous_policy.Equals(store_->policy_map()));
    EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());

    // Store the new public key so that the validation after the retrieve
    // operation completes can verify the signature.
    if (new_public_key)
      StoreUserPolicyKey(*new_public_key);

    // Let the store operation complete.
    chromeos::SessionManagerClient::RetrievePolicyCallback retrieve_callback;
    EXPECT_CALL(session_manager_client_,
                RetrievePolicyForUser(PolicyBuilder::kFakeUsername, _))
        .WillOnce(SaveArg<1>(&retrieve_callback));
    store_callback.Run(true);
    RunUntilIdle();
    EXPECT_TRUE(previous_policy.Equals(store_->policy_map()));
    EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
    Mock::VerifyAndClearExpectations(&session_manager_client_);
    ASSERT_FALSE(retrieve_callback.is_null());

    // Finish the retrieve callback.
    EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
    retrieve_callback.Run(policy_.GetBlob());
    RunUntilIdle();
    ASSERT_TRUE(store_->policy());
    EXPECT_EQ(policy_.policy_data().SerializeAsString(),
              store_->policy()->SerializeAsString());
    VerifyPolicyMap(new_value);
    EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  }

  void VerifyStoreHasValidationError() {
    EXPECT_FALSE(store_->policy());
    EXPECT_TRUE(store_->policy_map().empty());
    EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  }

  void RunUntilIdle() {
    loop_.RunUntilIdle();
    loop_.RunUntilIdle();
  }

  base::FilePath user_policy_dir() {
    return tmp_dir_.path().AppendASCII("var_run_user_policy");
  }

  base::FilePath user_policy_key_file() {
    return user_policy_dir().AppendASCII(kSanitizedUsername)
                            .AppendASCII("policy.pub");
  }

  base::FilePath token_file() {
    return tmp_dir_.path().AppendASCII("token");
  }

  base::FilePath policy_file() {
    return tmp_dir_.path().AppendASCII("policy");
  }

  base::MessageLoopForUI loop_;
  chromeos::MockCryptohomeClient cryptohome_client_;
  chromeos::MockSessionManagerClient session_manager_client_;
  UserPolicyBuilder policy_;
  MockCloudPolicyStoreObserver observer_;
  scoped_ptr<UserCloudPolicyStoreChromeOS> store_;

 private:
  base::ScopedTempDir tmp_dir_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreChromeOSTest);
};

TEST_F(UserCloudPolicyStoreChromeOSTest, InitialStore) {
  // Start without any public key to trigger the initial key checks.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  std::vector<uint8> new_public_key;
  ASSERT_TRUE(policy_.GetNewSigningKey()->ExportPublicKey(&new_public_key));
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(&new_public_key, NULL, kDefaultHomepage));
}

TEST_F(UserCloudPolicyStoreChromeOSTest, InitialStoreValidationFail) {
  // Start without any public key to trigger the initial key checks.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  // Make the policy blob contain a new public key.
  policy_.SetDefaultSigningKey();
  policy_.Build();
  *policy_.policy().mutable_new_public_key_verification_signature() = "garbage";

  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(
                  PolicyBuilder::kFakeUsername, policy_.GetBlob(), _)).Times(0);
  store_->Store(policy_.policy());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
}

TEST_F(UserCloudPolicyStoreChromeOSTest, InitialStoreMissingSignatureFailure) {
  // Start without any public key to trigger the initial key checks.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  // Make the policy blob contain a new public key.
  policy_.SetDefaultSigningKey();
  policy_.Build();
  policy_.policy().clear_new_public_key_verification_signature();

  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(
                  PolicyBuilder::kFakeUsername, policy_.GetBlob(), _)).Times(0);
  store_->Store(policy_.policy());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithExistingKey) {
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(NULL, NULL, kDefaultHomepage));
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithRotation) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  std::vector<uint8> new_public_key;
  ASSERT_TRUE(policy_.GetNewSigningKey()->ExportPublicKey(&new_public_key));
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(&new_public_key, NULL, kDefaultHomepage));
}

TEST_F(UserCloudPolicyStoreChromeOSTest,
       StoreWithRotationMissingSignatureError) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  policy_.policy().clear_new_public_key_verification_signature();

  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(
                  PolicyBuilder::kFakeUsername, policy_.GetBlob(), _)).Times(0);
  store_->Store(policy_.policy());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithRotationValidationError) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  *policy_.policy().mutable_new_public_key_verification_signature() = "garbage";

  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(
                  PolicyBuilder::kFakeUsername, policy_.GetBlob(), _)).Times(0);
  store_->Store(policy_.policy());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreFail) {
  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(PolicyBuilder::kFakeUsername,
                                 policy_.GetBlob(), _))
      .WillOnce(SaveArg<2>(&store_callback));
  store_->Store(policy_.policy());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  ASSERT_FALSE(store_callback.is_null());

  // Let the store operation complete.
  ExpectError(CloudPolicyStore::STATUS_STORE_ERROR);
  store_callback.Run(false);
  RunUntilIdle();
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_STORE_ERROR, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreValidationError) {
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(PolicyBuilder::kFakeUsername,
                                 policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithoutPolicyKey) {
  // Make the dbus call to cryptohome fail.
  Mock::VerifyAndClearExpectations(&cryptohome_client_);
  EXPECT_CALL(cryptohome_client_,
              GetSanitizedUsername(PolicyBuilder::kFakeUsername, _))
      .Times(AnyNumber())
      .WillRepeatedly(SendSanitizedUsername(chromeos::DBUS_METHOD_CALL_FAILURE,
                                            std::string()));

  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(PolicyBuilder::kFakeUsername,
                                 policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithInvalidSignature) {
  // Break the signature.
  policy_.policy().mutable_policy_data_signature()->append("garbage");

  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(PolicyBuilder::kFakeUsername,
                                 policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
}

TEST_F(UserCloudPolicyStoreChromeOSTest, Load) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify that the policy has been loaded.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(kDefaultHomepage);
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadNoPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadInvalidPolicy) {
  ExpectError(CloudPolicyStore::STATUS_PARSE_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad("invalid"));

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_PARSE_ERROR, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadValidationError) {
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  VerifyStoreHasValidationError();
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadNoKey) {
  // The loaded policy can't be verified without the public key.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  VerifyStoreHasValidationError();
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadInvalidSignature) {
  // Break the signature.
  policy_.policy().mutable_policy_data_signature()->append("garbage");
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  VerifyStoreHasValidationError();
}

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationFull) {
  std::string data;

  em::DeviceCredentials credentials;
  credentials.set_device_token(kLegacyToken);
  credentials.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(credentials.SerializeToString(&data));
  ASSERT_NE(-1, base::WriteFile(token_file(), data.c_str(), data.size()));

  em::CachedCloudPolicyResponse cached_policy;
  cached_policy.mutable_cloud_policy()->CopyFrom(policy_.policy());
  ASSERT_TRUE(cached_policy.SerializeToString(&data));
  ASSERT_NE(-1, base::WriteFile(policy_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify that legacy user policy and token have been loaded.
  em::PolicyData expected_policy_data;
  EXPECT_TRUE(expected_policy_data.ParseFromString(
                  cached_policy.cloud_policy().policy_data()));
  expected_policy_data.clear_public_key_version();
  expected_policy_data.set_request_token(kLegacyToken);
  expected_policy_data.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(expected_policy_data.SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(kDefaultHomepage);
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationNoToken) {
  std::string data;
  testing::Sequence seq;

  em::CachedCloudPolicyResponse cached_policy;
  cached_policy.mutable_cloud_policy()->CopyFrom(policy_.policy());
  ASSERT_TRUE(cached_policy.SerializeToString(&data));
  ASSERT_NE(-1, base::WriteFile(policy_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the legacy cache has been loaded.
  em::PolicyData expected_policy_data;
  EXPECT_TRUE(expected_policy_data.ParseFromString(
                  cached_policy.cloud_policy().policy_data()));
  expected_policy_data.clear_public_key_version();
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(expected_policy_data.SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(kDefaultHomepage);
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationNoPolicy) {
  std::string data;

  em::DeviceCredentials credentials;
  credentials.set_device_token(kLegacyToken);
  credentials.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(credentials.SerializeToString(&data));
  ASSERT_NE(-1, base::WriteFile(token_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify that legacy user policy and token have been loaded.
  em::PolicyData expected_policy_data;
  expected_policy_data.set_request_token(kLegacyToken);
  expected_policy_data.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(expected_policy_data.SerializeAsString(),
            store_->policy()->SerializeAsString());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationAndStoreNew) {
  // Start without an existing public key.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));

  std::string data;
  em::CachedCloudPolicyResponse cached_policy;
  cached_policy.mutable_cloud_policy()->CopyFrom(policy_.policy());
  ASSERT_TRUE(cached_policy.SerializeToString(&data));
  ASSERT_NE(-1, base::WriteFile(policy_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the legacy cache has been loaded.
  em::PolicyData expected_policy_data;
  EXPECT_TRUE(expected_policy_data.ParseFromString(
                  cached_policy.cloud_policy().policy_data()));
  expected_policy_data.clear_public_key_version();
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(expected_policy_data.SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(kDefaultHomepage);
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(base::PathExists(policy_file()));

  // Now store a new policy using the new homepage location.
  const char kNewHomepage[] = "http://google.com";
  policy_.payload().mutable_homepagelocation()->set_value(kNewHomepage);
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  std::vector<uint8> new_public_key;
  ASSERT_TRUE(policy_.GetNewSigningKey()->ExportPublicKey(&new_public_key));
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(&new_public_key, kDefaultHomepage, kNewHomepage));
  VerifyPolicyMap(kNewHomepage);

  // Verify that the legacy cache has been removed.
  EXPECT_FALSE(base::PathExists(policy_file()));
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediately) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(PolicyBuilder::kFakeUsername))
      .WillOnce(Return(policy_.GetBlob()));
  EXPECT_CALL(cryptohome_client_,
              BlockingGetSanitizedUsername(PolicyBuilder::kFakeUsername))
      .WillOnce(Return(kSanitizedUsername));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  // Note: verify that the |observer_| got notified synchronously, without
  // having to spin the current loop. TearDown() will flush the loop so this
  // must be done within the test.
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  Mock::VerifyAndClearExpectations(&cryptohome_client_);

  // The policy should become available without having to spin any loops.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(kDefaultHomepage);
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyNoPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(PolicyBuilder::kFakeUsername))
      .WillOnce(Return(""));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyInvalidBlob) {
  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(PolicyBuilder::kFakeUsername))
      .WillOnce(Return("le blob"));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_PARSE_ERROR, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyDBusFailure) {
  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(PolicyBuilder::kFakeUsername))
      .WillOnce(Return(policy_.GetBlob()));
  EXPECT_CALL(cryptohome_client_,
              BlockingGetSanitizedUsername(PolicyBuilder::kFakeUsername))
      .WillOnce(Return(""));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  Mock::VerifyAndClearExpectations(&cryptohome_client_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyNoUserPolicyKey) {
  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(PolicyBuilder::kFakeUsername))
      .WillOnce(Return(policy_.GetBlob()));
  EXPECT_CALL(cryptohome_client_,
              BlockingGetSanitizedUsername(PolicyBuilder::kFakeUsername))
      .WillOnce(Return("wrong@example.com"));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  Mock::VerifyAndClearExpectations(&cryptohome_client_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
}

}  // namespace

}  // namespace policy
