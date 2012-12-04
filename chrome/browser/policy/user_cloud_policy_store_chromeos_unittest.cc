// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_store_chromeos.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/mock_cloud_policy_store.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "content/public/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AllOf;
using testing::Eq;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace policy {

namespace {

const char kLegacyDeviceId[] = "legacy-device-id";
const char kLegacyToken[] = "legacy-token";

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
    user_manager_.user_manager()->SetLoggedInUser(PolicyBuilder::kFakeUsername);
    store_.reset(new UserCloudPolicyStoreChromeOS(&session_manager_client_,
                                                  token_file(),
                                                  policy_file()));
    store_->AddObserver(&observer_);

    policy_.payload().mutable_showhomebutton()->set_value(true);
    policy_.Build();
  }

  virtual void TearDown() OVERRIDE {
    store_->RemoveObserver(&observer_);
    store_.reset();
    loop_.RunUntilIdle();
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
    loop_.RunUntilIdle();
    ASSERT_FALSE(retrieve_callback.is_null());

    // Run the callback.
    retrieve_callback.Run(response);
    loop_.RunUntilIdle();
  }

  // Verifies that store_->policy_map() has the ShowHomeButton entry.
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
  UserPolicyBuilder policy_;
  MockCloudPolicyStoreObserver observer_;
  scoped_ptr<UserCloudPolicyStoreChromeOS> store_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  base::ScopedTempDir tmp_dir_;
  chromeos::ScopedMockUserManagerEnabler user_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreChromeOSTest);
};

TEST_F(UserCloudPolicyStoreChromeOSTest, Store) {
  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  EXPECT_CALL(session_manager_client_, StoreUserPolicy(policy_.GetBlob(), _))
      .WillOnce(SaveArg<1>(&store_callback));
  store_->Store(policy_.policy());
  loop_.RunUntilIdle();

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
  loop_.RunUntilIdle();
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  ASSERT_FALSE(retrieve_callback.is_null());

  // Finish the retrieve callback.
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  retrieve_callback.Run(policy_.GetBlob());
  loop_.RunUntilIdle();
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, StoreFail) {
  // Store policy.
  chromeos::SessionManagerClient::StorePolicyCallback store_callback;
  EXPECT_CALL(session_manager_client_, StoreUserPolicy(policy_.GetBlob(), _))
      .WillOnce(SaveArg<1>(&store_callback));
  store_->Store(policy_.policy());
  loop_.RunUntilIdle();

  // Let the store operation complete.
  ASSERT_FALSE(store_callback.is_null());
  ExpectError(CloudPolicyStore::STATUS_STORE_ERROR);
  store_callback.Run(false);
  loop_.RunUntilIdle();
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
  EXPECT_CALL(session_manager_client_, StoreUserPolicy(policy_.GetBlob(), _))
      .Times(0);
  store_->Store(policy_.policy());
  loop_.RunUntilIdle();
}

TEST_F(UserCloudPolicyStoreChromeOSTest, Load) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(policy_.GetBlob()));

  // Verify that the policy has been loaded.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
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

  // Verify no policy has been installed.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
}

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationFull) {
  testing::Sequence seq;
  std::string data;

  em::DeviceCredentials credentials;
  credentials.set_device_token(kLegacyToken);
  credentials.set_device_id(kLegacyDeviceId);
  ASSERT_TRUE(credentials.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(token_file(), data.c_str(), data.size()));

  em::CachedCloudPolicyResponse cached_policy;
  cached_policy.mutable_cloud_policy()->CopyFrom(policy_.policy());
  ASSERT_TRUE(cached_policy.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(policy_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));

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
  VerifyPolicyMap();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
};

TEST_F(UserCloudPolicyStoreChromeOSTest, MigrationNoToken) {
  std::string data;
  testing::Sequence seq;

  em::CachedCloudPolicyResponse cached_policy;
  cached_policy.mutable_cloud_policy()->CopyFrom(policy_.policy());
  ASSERT_TRUE(cached_policy.SerializeToString(&data));
  ASSERT_NE(-1, file_util::WriteFile(policy_file(), data.c_str(), data.size()));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  ASSERT_NO_FATAL_FAILURE(PerformPolicyLoad(""));

  // Verify the legacy cache has been loaded.
  em::PolicyData expected_policy_data;
  EXPECT_TRUE(expected_policy_data.ParseFromString(
                  cached_policy.cloud_policy().policy_data()));
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

}  // namespace

}  // namespace policy
