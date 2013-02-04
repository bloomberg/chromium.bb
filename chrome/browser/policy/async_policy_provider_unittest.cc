// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/async_policy_provider.h"

#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/policy/async_policy_loader.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;
using testing::Sequence;

namespace policy {

namespace {

// Helper to write a policy in |bundle| with less code.
void SetPolicy(PolicyBundle* bundle,
               const std::string& name,
               const std::string& value) {
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(name,
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_USER,
           base::Value::CreateStringValue(value));
}

class MockPolicyLoader : public AsyncPolicyLoader {
 public:
  MockPolicyLoader();
  virtual ~MockPolicyLoader();

  // Load() returns a scoped_ptr<PolicyBundle> but it can't be mocked because
  // scoped_ptr is moveable but not copyable. This override forwards the
  // call to MockLoad() which returns a PolicyBundle*, and returns a copy
  // wrapped in a passed scoped_ptr.
  virtual scoped_ptr<PolicyBundle> Load() OVERRIDE;

  MOCK_METHOD0(MockLoad, const PolicyBundle*());
  MOCK_METHOD0(InitOnFile, void());
  MOCK_METHOD0(LastModificationTime, base::Time());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPolicyLoader);
};

MockPolicyLoader::MockPolicyLoader() {}

MockPolicyLoader::~MockPolicyLoader() {}

scoped_ptr<PolicyBundle> MockPolicyLoader::Load() {
  scoped_ptr<PolicyBundle> bundle;
  const PolicyBundle* loaded = MockLoad();
  if (loaded) {
    bundle.reset(new PolicyBundle());
    bundle->CopyFrom(*loaded);
  }
  return bundle.Pass();
}

}  // namespace

class AsyncPolicyProviderTest : public testing::Test {
 protected:
  AsyncPolicyProviderTest();
  virtual ~AsyncPolicyProviderTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  PolicyBundle initial_bundle_;
  MockPolicyLoader* loader_;
  scoped_ptr<AsyncPolicyProvider> provider_;

  MessageLoop loop_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPolicyProviderTest);
};

AsyncPolicyProviderTest::AsyncPolicyProviderTest()
    : ui_thread_(content::BrowserThread::UI, &loop_),
      file_thread_(content::BrowserThread::FILE, &loop_) {}

AsyncPolicyProviderTest::~AsyncPolicyProviderTest() {}

void AsyncPolicyProviderTest::SetUp() {
  SetPolicy(&initial_bundle_, "policy", "initial");
  loader_ = new MockPolicyLoader();
  EXPECT_CALL(*loader_, LastModificationTime())
      .WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*loader_, InitOnFile()).Times(1);
  EXPECT_CALL(*loader_, MockLoad()).WillOnce(Return(&initial_bundle_));

  provider_.reset(
      new AsyncPolicyProvider(scoped_ptr<AsyncPolicyLoader>(loader_)));
  provider_->Init();
  // Verify that the initial load is done synchronously:
  EXPECT_TRUE(provider_->policies().Equals(initial_bundle_));

  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(loader_);

  EXPECT_CALL(*loader_, LastModificationTime())
      .WillRepeatedly(Return(base::Time()));
}

void AsyncPolicyProviderTest::TearDown() {
  if (provider_) {
    provider_->Shutdown();
    provider_.reset();
  }
  loop_.RunUntilIdle();
}

TEST_F(AsyncPolicyProviderTest, RefreshPolicies) {
  PolicyBundle refreshed_bundle;
  SetPolicy(&refreshed_bundle, "policy", "refreshed");
  EXPECT_CALL(*loader_, MockLoad()).WillOnce(Return(&refreshed_bundle));

  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  loop_.RunUntilIdle();
  // The refreshed policies are now provided.
  EXPECT_TRUE(provider_->policies().Equals(refreshed_bundle));
  provider_->RemoveObserver(&observer);
}

TEST_F(AsyncPolicyProviderTest, RefreshPoliciesTwice) {
  PolicyBundle refreshed_bundle;
  SetPolicy(&refreshed_bundle, "policy", "refreshed");
  EXPECT_CALL(*loader_, MockLoad()).WillRepeatedly(Return(&refreshed_bundle));

  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  provider_->RefreshPolicies();
  // Doesn't refresh before going through the FILE thread.
  Mock::VerifyAndClearExpectations(&observer);

  // Doesn't refresh if another RefreshPolicies request is made.
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  loop_.RunUntilIdle();
  // The refreshed policies are now provided.
  EXPECT_TRUE(provider_->policies().Equals(refreshed_bundle));
  Mock::VerifyAndClearExpectations(&observer);
  provider_->RemoveObserver(&observer);
}

TEST_F(AsyncPolicyProviderTest, RefreshPoliciesDuringReload) {
  PolicyBundle reloaded_bundle;
  SetPolicy(&reloaded_bundle, "policy", "reloaded");
  PolicyBundle refreshed_bundle;
  SetPolicy(&refreshed_bundle, "policy", "refreshed");

  Sequence load_sequence;
  // Reload.
  EXPECT_CALL(*loader_, MockLoad()).InSequence(load_sequence)
                                   .WillOnce(Return(&reloaded_bundle));
  // RefreshPolicies.
  EXPECT_CALL(*loader_, MockLoad()).InSequence(load_sequence)
                                   .WillOnce(Return(&refreshed_bundle));

  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);

  // A Reload is triggered before RefreshPolicies, and it shouldn't trigger
  // notifications.
  loader_->Reload(true);
  Mock::VerifyAndClearExpectations(&observer);

  // Doesn't refresh before going through the FILE thread.
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  loop_.RunUntilIdle();
  // The refreshed policies are now provided, and the |reloaded_bundle| was
  // dropped.
  EXPECT_TRUE(provider_->policies().Equals(refreshed_bundle));
  Mock::VerifyAndClearExpectations(&observer);
  provider_->RemoveObserver(&observer);
}

TEST_F(AsyncPolicyProviderTest, Shutdown) {
  EXPECT_CALL(*loader_, MockLoad()).WillRepeatedly(Return(&initial_bundle_));

  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);

  // Though there is a pending Reload, the provider and the loader can be
  // deleted at any time.
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  loader_->Reload(true);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  provider_->Shutdown();
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  provider_->RemoveObserver(&observer);
  provider_.reset();
}

}  // namespace policy
