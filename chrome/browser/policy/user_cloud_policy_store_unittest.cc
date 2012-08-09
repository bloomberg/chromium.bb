// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_store.h"

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AllOf;
using testing::Eq;
using testing::Property;

namespace policy {

namespace {

class MockCloudPolicyStoreObserver : public CloudPolicyStore::Observer {
 public:
  MockCloudPolicyStoreObserver() {}
  virtual ~MockCloudPolicyStoreObserver() {}

  MOCK_METHOD1(OnStoreLoaded, void(CloudPolicyStore* store));
  MOCK_METHOD1(OnStoreError, void(CloudPolicyStore* store));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyStoreObserver);
};

class UserCloudPolicyStoreTest : public testing::Test {
 public:
  UserCloudPolicyStoreTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_),
        profile_(new TestingProfile()) {}

  virtual void SetUp() OVERRIDE {
    SigninManager* signin = static_cast<SigninManager*>(
      SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(), FakeSigninManager::Build));
    signin->SetAuthenticatedUsername(PolicyBuilder::kFakeUsername);
    store_.reset(new UserCloudPolicyStore(profile_.get()));
    store_->AddObserver(&observer_);

    policy_.payload().mutable_showhomebutton()->set_showhomebutton(true);
    policy_.Build();
  }

  virtual void TearDown() OVERRIDE {
    store_->RemoveObserver(&observer_);
    store_.reset();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Verifies that store_->policy_map() has the ShowHomeButton entry.
  void VerifyPolicyMap() {
    EXPECT_EQ(1U, store_->policy_map().size());
    const PolicyMap::Entry* entry =
        store_->policy_map().Get(key::kShowHomeButton);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(base::FundamentalValue(true).Equals(entry->value));
  }

  // Install an expectation on |observer_| for an error code.
  void ExpectError(CloudPolicyStore::Status error) {
    EXPECT_CALL(observer_,
                OnStoreError(AllOf(Eq(store_.get()),
                                   Property(&CloudPolicyStore::status,
                                            Eq(error)))));
  }

  UserPolicyBuilder policy_;
  MockCloudPolicyStoreObserver observer_;
  scoped_ptr<UserCloudPolicyStore> store_;

  // CloudPolicyValidator() requires a FILE thread so declare one here. Both
  // |ui_thread_| and |file_thread_| share the same MessageLoop |loop_| so
  // callers can use RunLoop to manage both virtual threads.
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreTest);
};

TEST_F(UserCloudPolicyStoreTest, Store) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy and make sure it ends up as the currently active
  // policy.
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->Store(policy_.policy());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Policy should be decoded and stored.
  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreTest, StoreValidationError) {
  // Create an invalid policy (no policy type).
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  // Store policy.
  ExpectError(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store_->Store(policy_.policy());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  ASSERT_FALSE(store_->policy());
}

}  // namespace

}  // namespace policy

