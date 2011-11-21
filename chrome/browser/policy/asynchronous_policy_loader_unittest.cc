// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::_;

namespace policy {

namespace {

void IgnoreCallback() {
}

}  // namespace

class AsynchronousPolicyLoaderTest : public AsynchronousPolicyTestBase {
 public:
  AsynchronousPolicyLoaderTest() {}
  virtual ~AsynchronousPolicyLoaderTest() {}

  virtual void SetUp() {
    AsynchronousPolicyTestBase::SetUp();
    mock_provider_.reset(new MockConfigurationPolicyProvider());
    ignore_callback_ = base::Bind(&IgnoreCallback);
  }

 protected:
  base::Closure ignore_callback_;
  scoped_ptr<MockConfigurationPolicyProvider> mock_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsynchronousPolicyLoaderTest);
};

ACTION(CreateTestDictionary) {
  return new DictionaryValue();
}

ACTION_P(CreateSequencedTestDictionary, number) {
  DictionaryValue* test_dictionary = new DictionaryValue();
  test_dictionary->SetInteger("id", ++(*number));
  return test_dictionary;
}

ACTION(RescheduleImmediatePolicyReload) {
  *arg1 = base::TimeDelta();
  return false;
}

TEST_F(AsynchronousPolicyLoaderTest, InitialLoad) {
  DictionaryValue* template_dict(new DictionaryValue());
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, Load()).WillOnce(Return(template_dict));
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate, 10);
  loader->Init(ignore_callback_);
  const DictionaryValue* loaded_dict(loader->policy());
  EXPECT_TRUE(loaded_dict->Equals(template_dict));
}

// Verify that the fallback policy requests are made.
TEST_F(AsynchronousPolicyLoaderTest, InitialLoadWithFallback) {
  int dictionary_number = 0;
  InSequence s;
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number));
  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number));
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate, 10);
  loader->Init(ignore_callback_);
  loop_.RunAllPending();
  loader->Reload(true);
  loop_.RunAllPending();

  const DictionaryValue* loaded_dict(loader->policy());
  int loaded_number;
  EXPECT_TRUE(loaded_dict->GetInteger("id", &loaded_number));
  EXPECT_EQ(dictionary_number, loaded_number);
}

// Ensure that calling stop on the loader stops subsequent reloads from
// happening.
TEST_F(AsynchronousPolicyLoaderTest, Stop) {
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  ON_CALL(*delegate, Load()).WillByDefault(CreateTestDictionary());
  EXPECT_CALL(*delegate, Load()).Times(1);
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate, 10);
  loader->Init(ignore_callback_);
  loop_.RunAllPending();
  loader->Stop();
  loop_.RunAllPending();
  loader->Reload(true);
  loop_.RunAllPending();
}

// Verifies that the provider is notified upon policy reload.
TEST_F(AsynchronousPolicyLoaderTest, ProviderNotificationOnPolicyChange) {
  InSequence s;
  MockConfigurationPolicyObserver observer;
  int dictionary_number_1 = 0;
  int dictionary_number_2 = 0;

  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number_1));

  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate, 10);
  AsynchronousPolicyProvider provider(NULL, loader);
  // |registrar| must be declared last so that it is destroyed first.
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(&provider, &observer);
  Mock::VerifyAndClearExpectations(delegate);

  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number_2));
  EXPECT_CALL(observer, OnUpdatePolicy(_)).Times(1);
  provider.RefreshPolicies();
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(delegate);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(*delegate, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number_1));
  EXPECT_CALL(observer, OnUpdatePolicy(_)).Times(1);
  provider.RefreshPolicies();
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(delegate);
  Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace policy
