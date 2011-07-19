// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace policy {

class MockConfigurationPolicyObserver
    : public ConfigurationPolicyProvider::Observer {
 public:
  MOCK_METHOD0(OnUpdatePolicy, void());
  void OnProviderGoingAway() {}
};

class AsynchronousPolicyLoaderTest : public AsynchronousPolicyTestBase {
 public:
  AsynchronousPolicyLoaderTest() {}
  virtual ~AsynchronousPolicyLoaderTest() {}

  virtual void SetUp() {
    AsynchronousPolicyTestBase::SetUp();
    mock_provider_.reset(new MockConfigurationPolicyProvider());
  }

 protected:
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
  EXPECT_CALL(*delegate_, Load()).WillOnce(Return(template_dict));
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate_.release(), 10);
  loader->Init();
  const DictionaryValue* loaded_dict(loader->policy());
  EXPECT_TRUE(loaded_dict->Equals(template_dict));
}

// Verify that the fallback policy requests are made.
TEST_F(AsynchronousPolicyLoaderTest, InitialLoadWithFallback) {
  int dictionary_number = 0;
  InSequence s;
  EXPECT_CALL(*delegate_, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number));
  EXPECT_CALL(*delegate_, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number));
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate_.release(), 10);
  loader->Init();
  loop_.RunAllPending();
  loader->Reload();
  loop_.RunAllPending();

  const DictionaryValue* loaded_dict(loader->policy());
  int loaded_number;
  EXPECT_TRUE(loaded_dict->GetInteger("id", &loaded_number));
  EXPECT_EQ(dictionary_number, loaded_number);
}

// Ensure that calling stop on the loader stops subsequent reloads from
// happening.
TEST_F(AsynchronousPolicyLoaderTest, Stop) {
  ON_CALL(*delegate_, Load()).WillByDefault(CreateTestDictionary());
  EXPECT_CALL(*delegate_, Load()).Times(1);
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate_.release(), 10);
  loader->Init();
  loop_.RunAllPending();
  loader->Stop();
  loop_.RunAllPending();
  loader->Reload();
  loop_.RunAllPending();
}

// Verifies that the provider is notified upon policy reload, but only
// if the policy changed.
TEST_F(AsynchronousPolicyLoaderTest, ProviderNotificationOnPolicyChange) {
  InSequence s;
  MockConfigurationPolicyObserver observer;
  int dictionary_number_1 = 0;
  int dictionary_number_2 = 0;
  EXPECT_CALL(*delegate_, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number_1));
  EXPECT_CALL(*delegate_, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number_2));
  EXPECT_CALL(observer, OnUpdatePolicy()).Times(0);
  EXPECT_CALL(*delegate_, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number_2));
  EXPECT_CALL(observer, OnUpdatePolicy()).Times(1);
  EXPECT_CALL(*delegate_, Load()).WillOnce(
      CreateSequencedTestDictionary(&dictionary_number_1));
  scoped_refptr<AsynchronousPolicyLoader> loader =
      new AsynchronousPolicyLoader(delegate_.release(), 10);
  AsynchronousPolicyProvider provider(NULL, loader);
  // |registrar| must be declared last so that it is destroyed first.
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(&provider, &observer);
  loop_.RunAllPending();
  loader->Reload();
  loop_.RunAllPending();
  loader->Reload();
  loop_.RunAllPending();
  loader->Reload();
  loop_.RunAllPending();
}

}  // namespace policy
