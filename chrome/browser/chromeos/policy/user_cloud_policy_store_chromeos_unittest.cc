// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_local.pb.h"
#include "crypto/rsa_private_key.h"
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

const char kSanitizedUsername[] =
    "0123456789ABCDEF0123456789ABCDEF012345678@example.com";
const char kDefaultHomepage[] = "http://chromium.org";

ACTION_P2(SendSanitizedUsername, call_status, sanitized_username) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(arg1, call_status, sanitized_username));
}

class UserCloudPolicyStoreChromeOSTest : public testing::Test {
 protected:
  UserCloudPolicyStoreChromeOSTest() {}

  void SetUp() override {
    EXPECT_CALL(cryptohome_client_, GetSanitizedUsername(cryptohome_id_, _))
        .Times(AnyNumber())
        .WillRepeatedly(SendSanitizedUsername(
            chromeos::DBUS_METHOD_CALL_SUCCESS, kSanitizedUsername));

    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    store_.reset(new UserCloudPolicyStoreChromeOS(
        &cryptohome_client_, &session_manager_client_, loop_.task_runner(),
        account_id_, user_policy_dir(), false /* is_active_directory */));
    store_->AddObserver(&observer_);

    // Install the initial public key, so that by default the validation of
    // the stored/loaded policy blob succeeds.
    std::string public_key = policy_.GetPublicSigningKeyAsString();
    ASSERT_FALSE(public_key.empty());
    StoreUserPolicyKey(public_key);

    policy_.payload().mutable_homepagelocation()->set_value(kDefaultHomepage);
    policy_.Build();
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    store_.reset();
    base::RunLoop().RunUntilIdle();
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
                RetrievePolicyForUser(cryptohome_id_, _))
        .WillOnce(SaveArg<1>(&retrieve_callback));
    store_->Load();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&session_manager_client_);
    ASSERT_FALSE(retrieve_callback.is_null());

    // Run the callback.
    retrieve_callback.Run(response);
    base::RunLoop().RunUntilIdle();
  }

  // Verifies that store_->policy_map() has the HomepageLocation entry with
  // the |expected_value|.
  void VerifyPolicyMap(const char* expected_value) {
    EXPECT_EQ(1U, store_->policy_map().size());
    const PolicyMap::Entry* entry =
        store_->policy_map().Get(key::kHomepageLocation);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(base::StringValue(expected_value).Equals(entry->value.get()));
  }

  void StoreUserPolicyKey(const std::string& public_key) {
    ASSERT_TRUE(base::CreateDirectory(user_policy_key_file().DirName()));
    ASSERT_TRUE(base::WriteFile(user_policy_key_file(), public_key.data(),
                                public_key.size()));
  }

  // Stores the current |policy_| and verifies that it is published.
  // If |new_public_key| is set then it will be persisted after storing but
  // before loading the policy, so that the signature validation can succeed.
  // If |previous_value| is set then a previously existing policy with that
  // value will be expected; otherwise no previous policy is expected.
  // If |new_value| is set then a new policy with that value is expected after
  // storing the |policy_| blob.
  void PerformStorePolicy(const std::string* new_public_key,
                          const char* previous_value,
                          const char* new_value) {
    const CloudPolicyStore::Status initial_status = store_->status();

    chromeos::SessionManagerClient::StorePolicyCallback store_callback;
    EXPECT_CALL(session_manager_client_,
                StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
        .WillOnce(SaveArg<2>(&store_callback));
    store_->Store(policy_.policy());
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&session_manager_client_);
    ASSERT_FALSE(store_callback.is_null());

    // The new policy shouldn't be present yet.
    PolicyMap previous_policy;
    EXPECT_EQ(previous_value != nullptr, store_->policy() != nullptr);
    if (previous_value) {
      previous_policy.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                          base::MakeUnique<base::StringValue>(previous_value),
                          nullptr);
    }
    EXPECT_TRUE(previous_policy.Equals(store_->policy_map()));
    EXPECT_EQ(initial_status, store_->status());

    // Store the new public key so that the validation after the retrieve
    // operation completes can verify the signature.
    if (new_public_key)
      StoreUserPolicyKey(*new_public_key);

    // Let the store operation complete.
    chromeos::SessionManagerClient::RetrievePolicyCallback retrieve_callback;
    EXPECT_CALL(session_manager_client_,
                RetrievePolicyForUser(cryptohome_id_, _))
        .WillOnce(SaveArg<1>(&retrieve_callback));
    store_callback.Run(true);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(previous_policy.Equals(store_->policy_map()));
    EXPECT_EQ(initial_status, store_->status());
    Mock::VerifyAndClearExpectations(&session_manager_client_);
    ASSERT_FALSE(retrieve_callback.is_null());

    // Finish the retrieve callback.
    EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
    retrieve_callback.Run(policy_.GetBlob());
    base::RunLoop().RunUntilIdle();
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

  base::FilePath user_policy_dir() {
    return tmp_dir_.GetPath().AppendASCII("var_run_user_policy");
  }

  base::FilePath user_policy_key_file() {
    return user_policy_dir().AppendASCII(kSanitizedUsername)
                            .AppendASCII("policy.pub");
  }

  base::MessageLoopForUI loop_;
  chromeos::MockCryptohomeClient cryptohome_client_;
  chromeos::MockSessionManagerClient session_manager_client_;
  UserPolicyBuilder policy_;
  MockCloudPolicyStoreObserver observer_;
  std::unique_ptr<UserCloudPolicyStoreChromeOS> store_;
  const AccountId account_id_ =
      AccountId::FromUserEmail(PolicyBuilder::kFakeUsername);
  const cryptohome::Identification cryptohome_id_ =
      cryptohome::Identification(account_id_);

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
  std::string new_public_key = policy_.GetPublicNewSigningKeyAsString();
  ASSERT_FALSE(new_public_key.empty());
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(&new_public_key, nullptr, kDefaultHomepage));
  EXPECT_EQ(new_public_key, store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, InitialStoreValidationFail) {
  // Start without any public key to trigger the initial key checks.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  // Make the policy blob contain a new public key.
  policy_.SetDefaultSigningKey();
  policy_.Build();
  *policy_.policy().mutable_new_public_key_verification_signature_deprecated() =
      "garbage";

  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, InitialStoreMissingSignatureFailure) {
  // Start without any public key to trigger the initial key checks.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  // Make the policy blob contain a new public key.
  policy_.SetDefaultSigningKey();
  policy_.Build();
  policy_.policy().clear_new_public_key_verification_signature_deprecated();

  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithExistingKey) {
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(nullptr, nullptr, kDefaultHomepage));
  EXPECT_EQ(policy_.GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithRotation) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  std::string new_public_key = policy_.GetPublicNewSigningKeyAsString();
  ASSERT_FALSE(new_public_key.empty());
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(&new_public_key, nullptr, kDefaultHomepage));
  EXPECT_EQ(new_public_key, store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest,
       StoreWithRotationMissingSignatureError) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  policy_.policy().clear_new_public_key_verification_signature_deprecated();

  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithRotationValidationError) {
  // Make the policy blob contain a new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  *policy_.policy().mutable_new_public_key_verification_signature_deprecated() =
      "garbage";

  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreFail) {
  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
      .WillOnce(SaveArg<2>(&store_callback));
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  ASSERT_FALSE(store_callback.is_null());

  // Let the store operation complete.
  ExpectError(CloudPolicyStore::STATUS_STORE_ERROR);
  store_callback.Run(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_STORE_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreValidationError) {
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithoutPolicyKey) {
  // Make the dbus call to cryptohome fail.
  Mock::VerifyAndClearExpectations(&cryptohome_client_);
  EXPECT_CALL(cryptohome_client_, GetSanitizedUsername(cryptohome_id_, _))
      .Times(AnyNumber())
      .WillRepeatedly(SendSanitizedUsername(chromeos::DBUS_METHOD_CALL_FAILURE,
                                            std::string()));

  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreWithInvalidSignature) {
  // Break the signature.
  policy_.policy().mutable_policy_data_signature()->append("garbage");

  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_CALL(session_manager_client_,
              StorePolicyForUser(cryptohome_id_, policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, MultipleStoresWithRotation) {
  // Store initial policy signed with the initial public key.
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(nullptr, nullptr, kDefaultHomepage));
  const std::string initial_public_key = policy_.GetPublicSigningKeyAsString();
  EXPECT_EQ(initial_public_key, store_->policy_signature_public_key());

  // Try storing an invalid policy signed with the new public key.
  policy_.SetDefaultNewSigningKey();
  policy_.policy_data().clear_policy_type();
  policy_.Build();
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  // Still the initial public key is exposed.
  EXPECT_EQ(initial_public_key, store_->policy_signature_public_key());

  // Store the correct policy signed with the new public key.
  policy_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  policy_.Build();
  std::string new_public_key = policy_.GetPublicNewSigningKeyAsString();
  ASSERT_FALSE(new_public_key.empty());
  ASSERT_NO_FATAL_FAILURE(
      PerformStorePolicy(&new_public_key, kDefaultHomepage, kDefaultHomepage));
  EXPECT_EQ(policy_.GetPublicNewSigningKeyAsString(),
            store_->policy_signature_public_key());
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
  EXPECT_EQ(policy_.GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadNoPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadInvalidPolicy) {
  ExpectError(CloudPolicyStore::STATUS_PARSE_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad("invalid"));

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_PARSE_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadValidationError) {
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  VerifyStoreHasValidationError();
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadNoKey) {
  // The loaded policy can't be verified without the public key.
  ASSERT_TRUE(base::DeleteFile(user_policy_key_file(), false));
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  VerifyStoreHasValidationError();
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadInvalidSignature) {
  // Break the signature.
  policy_.policy().mutable_policy_data_signature()->append("garbage");
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));
  VerifyStoreHasValidationError();
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediately) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(cryptohome_id_))
      .WillOnce(Return(policy_.GetBlob()));
  EXPECT_CALL(cryptohome_client_, BlockingGetSanitizedUsername(cryptohome_id_))
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
  EXPECT_EQ(policy_.GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyNoPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(cryptohome_id_))
      .WillOnce(Return(""));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyInvalidBlob) {
  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(cryptohome_id_))
      .WillOnce(Return("le blob"));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_PARSE_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyDBusFailure) {
  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(cryptohome_id_))
      .WillOnce(Return(policy_.GetBlob()));
  EXPECT_CALL(cryptohome_client_, BlockingGetSanitizedUsername(cryptohome_id_))
      .WillOnce(Return(""));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  Mock::VerifyAndClearExpectations(&cryptohome_client_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, LoadImmediatelyNoUserPolicyKey) {
  EXPECT_CALL(observer_, OnStoreError(store_.get()));
  EXPECT_CALL(session_manager_client_,
              BlockingRetrievePolicyForUser(cryptohome_id_))
      .WillOnce(Return(policy_.GetBlob()));
  EXPECT_CALL(cryptohome_client_, BlockingGetSanitizedUsername(cryptohome_id_))
      .WillOnce(Return("wrong@example.com"));

  EXPECT_FALSE(store_->policy());
  store_->LoadImmediately();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&session_manager_client_);
  Mock::VerifyAndClearExpectations(&cryptohome_client_);

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

}  // namespace

}  // namespace policy
