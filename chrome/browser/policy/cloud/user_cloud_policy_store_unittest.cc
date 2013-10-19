// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_store.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/cloud/mock_cloud_external_data_manager.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_store.h"
#include "chrome/browser/policy/cloud/policy_builder.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AllOf;
using testing::Eq;
using testing::Mock;
using testing::Property;
using testing::Sequence;

namespace policy {

namespace {

void RunUntilIdle() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

class UserCloudPolicyStoreTest : public testing::Test {
 public:
  UserCloudPolicyStoreTest()
      : loop_(base::MessageLoop::TYPE_UI),
        profile_(new TestingProfile()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    SigninManager* signin = static_cast<SigninManager*>(
      SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(), FakeSigninManager::Build));
    profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                    PolicyBuilder::kFakeUsername);
    signin->Initialize(profile_.get(), NULL);
    store_.reset(new UserCloudPolicyStore(
        profile_.get(), policy_file(), loop_.message_loop_proxy()));
    external_data_manager_.reset(new MockCloudExternalDataManager);
    external_data_manager_->SetPolicyStore(store_.get());
    store_->AddObserver(&observer_);

    policy_.payload().mutable_passwordmanagerenabled()->set_value(true);
    policy_.payload().mutable_urlblacklist()->mutable_value()->add_entries(
        "chromium.org");

    policy_.Build();
  }

  virtual void TearDown() OVERRIDE {
    store_->RemoveObserver(&observer_);
    external_data_manager_.reset();
    store_.reset();
    RunUntilIdle();
  }

  base::FilePath policy_file() {
    return tmp_dir_.path().AppendASCII("policy");
  }

  // Verifies that store_->policy_map() has the appropriate entries.
  void VerifyPolicyMap(CloudPolicyStore* store) {
    EXPECT_EQ(2U, store->policy_map().size());
    const PolicyMap::Entry* entry =
        store->policy_map().Get(key::kPasswordManagerEnabled);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(base::FundamentalValue(true).Equals(entry->value));
    ASSERT_TRUE(store->policy_map().Get(key::kURLBlacklist));
  }

  // Install an expectation on |observer_| for an error code.
  void ExpectError(CloudPolicyStore* store, CloudPolicyStore::Status error) {
    EXPECT_CALL(observer_,
                OnStoreError(AllOf(Eq(store),
                                   Property(&CloudPolicyStore::status,
                                            Eq(error)))));
  }

  UserPolicyBuilder policy_;
  MockCloudPolicyStoreObserver observer_;
  scoped_ptr<UserCloudPolicyStore> store_;
  scoped_ptr<MockCloudExternalDataManager> external_data_manager_;

  // CloudPolicyValidator() requires a FILE thread so declare one here. Both
  // |ui_thread_| and |file_thread_| share the same MessageLoop |loop_| so
  // callers can use RunLoop to manage both virtual threads.
  base::MessageLoop loop_;

  scoped_ptr<TestingProfile> profile_;
  base::ScopedTempDir tmp_dir_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreTest);
};

TEST_F(UserCloudPolicyStoreTest, LoadWithNoFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  Sequence s;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
  store_->Load();
  RunUntilIdle();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_F(UserCloudPolicyStoreTest, LoadWithInvalidFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Create a bogus file.
  ASSERT_TRUE(file_util::CreateDirectory(policy_file().DirName()));
  std::string bogus_data = "bogus_data";
  int size = bogus_data.size();
  ASSERT_EQ(size, file_util::WriteFile(policy_file(),
                                       bogus_data.c_str(),
                                       bogus_data.size()));

  ExpectError(store_.get(), CloudPolicyStore::STATUS_LOAD_ERROR);
  store_->Load();
  RunUntilIdle();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_F(UserCloudPolicyStoreTest, LoadImmediatelyWithNoFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  Sequence s;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
  store_->LoadImmediately();  // Should load without running the message loop.

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_F(UserCloudPolicyStoreTest, LoadImmediatelyWithInvalidFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Create a bogus file.
  ASSERT_TRUE(file_util::CreateDirectory(policy_file().DirName()));
  std::string bogus_data = "bogus_data";
  int size = bogus_data.size();
  ASSERT_EQ(size, file_util::WriteFile(policy_file(),
                                       bogus_data.c_str(),
                                       bogus_data.size()));

  ExpectError(store_.get(), CloudPolicyStore::STATUS_LOAD_ERROR);
  store_->LoadImmediately();  // Should load without running the message loop.

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_F(UserCloudPolicyStoreTest, Store) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy and make sure it ends up as the currently active
  // policy.
  Sequence s;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
  store_->Store(policy_.policy());
  RunUntilIdle();

  // Policy should be decoded and stored.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(store_.get());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreTest, StoreThenClear) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy and make sure the file exists.
  // policy.
  Sequence s1;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s1);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s1);
  store_->Store(policy_.policy());
  RunUntilIdle();

  EXPECT_TRUE(store_->policy());
  EXPECT_FALSE(store_->policy_map().empty());

  Mock::VerifyAndClearExpectations(external_data_manager_.get());
  Mock::VerifyAndClearExpectations(&observer_);

  // Policy file should exist.
  ASSERT_TRUE(base::PathExists(policy_file()));

  Sequence s2;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s2);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s2);
  store_->Clear();
  RunUntilIdle();

  // Policy file should not exist.
  ASSERT_TRUE(!base::PathExists(policy_file()));

  // Policy should be gone.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreTest, StoreTwoTimes) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy then store a second policy before the first one
  // finishes validating, and make sure the second policy ends up as the active
  // policy.
  Sequence s1;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s1);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s1);

  UserPolicyBuilder first_policy;
  first_policy.payload().mutable_passwordmanagerenabled()->set_value(false);
  first_policy.Build();
  store_->Store(first_policy.policy());
  RunUntilIdle();

  Mock::VerifyAndClearExpectations(external_data_manager_.get());
  Mock::VerifyAndClearExpectations(&observer_);

  Sequence s2;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s2);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s2);

  store_->Store(policy_.policy());
  RunUntilIdle();

  // Policy should be decoded and stored.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(store_.get());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreTest, StoreThenLoad) {
  // Store a simple policy and make sure it can be read back in.
  // policy.
  Sequence s;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
  store_->Store(policy_.policy());
  RunUntilIdle();

  // Now, make sure the policy can be read back in from a second store.
  scoped_ptr<UserCloudPolicyStore> store2(new UserCloudPolicyStore(
      profile_.get(), policy_file(), loop_.message_loop_proxy()));
  store2->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnStoreLoaded(store2.get()));
  store2->Load();
  RunUntilIdle();

  ASSERT_TRUE(store2->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store2->policy()->SerializeAsString());
  VerifyPolicyMap(store2.get());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store2->status());
  store2->RemoveObserver(&observer_);
}

TEST_F(UserCloudPolicyStoreTest, StoreThenLoadImmediately) {
  // Store a simple policy and make sure it can be read back in.
  // policy.
  Sequence s;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
  store_->Store(policy_.policy());
  RunUntilIdle();

  // Now, make sure the policy can be read back in from a second store.
  scoped_ptr<UserCloudPolicyStore> store2(new UserCloudPolicyStore(
      profile_.get(), policy_file(), loop_.message_loop_proxy()));
  store2->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnStoreLoaded(store2.get()));
  store2->LoadImmediately();  // Should load without running the message loop.

  ASSERT_TRUE(store2->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store2->policy()->SerializeAsString());
  VerifyPolicyMap(store2.get());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store2->status());
  store2->RemoveObserver(&observer_);
}

TEST_F(UserCloudPolicyStoreTest, StoreValidationError) {
  // Create an invalid policy (no policy type).
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  // Store policy.
  ExpectError(store_.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store_->Store(policy_.policy());
  RunUntilIdle();
  ASSERT_FALSE(store_->policy());
}

TEST_F(UserCloudPolicyStoreTest, LoadValidationError) {
  // Force a validation error by changing the username after policy is stored.
  Sequence s;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
  store_->Store(policy_.policy());
  RunUntilIdle();

  // Sign out, and sign back in as a different user, and try to load the profile
  // data (should fail due to mismatched username).
  SigninManagerFactory::GetForProfile(profile_.get())->SignOut();
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "foobar@foobar.com");

  scoped_ptr<UserCloudPolicyStore> store2(new UserCloudPolicyStore(
      profile_.get(), policy_file(), loop_.message_loop_proxy()));
  store2->AddObserver(&observer_);
  ExpectError(store2.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store2->Load();
  RunUntilIdle();

  ASSERT_FALSE(store2->policy());
  store2->RemoveObserver(&observer_);

  // Sign out - we should be able to load the policy (don't check usernames
  // when signed out).
  SigninManagerFactory::GetForProfile(profile_.get())->SignOut();
  scoped_ptr<UserCloudPolicyStore> store3(new UserCloudPolicyStore(
      profile_.get(), policy_file(), loop_.message_loop_proxy()));
  store3->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnStoreLoaded(store3.get()));
  store3->Load();
  RunUntilIdle();

  ASSERT_TRUE(store3->policy());
  store3->RemoveObserver(&observer_);

  // Now start a signin as a different user - this should fail validation.
  FakeSigninManager* signin = static_cast<FakeSigninManager*>(
      SigninManagerFactory::GetForProfile(profile_.get()));
  signin->set_auth_in_progress("foobar@foobar.com");

  scoped_ptr<UserCloudPolicyStore> store4(new UserCloudPolicyStore(
      profile_.get(), policy_file(), loop_.message_loop_proxy()));
  store4->AddObserver(&observer_);
  ExpectError(store4.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store4->Load();
  RunUntilIdle();

  ASSERT_FALSE(store4->policy());
  store4->RemoveObserver(&observer_);
}

}  // namespace

}  // namespace policy
