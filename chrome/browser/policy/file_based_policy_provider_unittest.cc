// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/file_based_policy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace policy {

// Shorter reload intervals for testing FileBasedPolicyLoader.
const int kSettleIntervalSecondsForTesting = 0;
const int kReloadIntervalMinutesForTesting = 1;

// A delegate for testing that can feed arbitrary information to the loader.
class TestDelegate : public FileBasedPolicyProvider::Delegate {
 public:
  TestDelegate()
      : FileBasedPolicyProvider::Delegate(FilePath(FILE_PATH_LITERAL("fake"))) {
  }

  // FileBasedPolicyProvider::Delegate implementation:
  virtual DictionaryValue* Load() {
    return static_cast<DictionaryValue*>(dict_.DeepCopy());
  }

  virtual base::Time GetLastModification() {
    return last_modification_;
  }

  DictionaryValue* dict() { return &dict_; }
  void set_last_modification(const base::Time& last_modification) {
    last_modification_ = last_modification;
  }

 private:
  DictionaryValue dict_;
  base::Time last_modification_;
};

// A mock provider that allows us to capture reload notifications.
class MockPolicyProvider : public ConfigurationPolicyProvider,
                           public base::SupportsWeakPtr<MockPolicyProvider> {
 public:
  explicit MockPolicyProvider()
      : ConfigurationPolicyProvider(
          ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList()) {
  }

  virtual bool Provide(ConfigurationPolicyStoreInterface* store) {
    return true;
  }

  MOCK_METHOD0(NotifyStoreOfPolicyChange, void());
};

class FileBasedPolicyLoaderTest : public testing::Test {
 protected:
  FileBasedPolicyLoaderTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {}

  virtual void TearDown() {
    loop_.RunAllPending();
  }

  MessageLoop loop_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(FileBasedPolicyLoaderTest, BasicLoad) {
  TestDelegate* test_delegate = new TestDelegate;
  test_delegate->dict()->SetString("HomepageLocation", "http://www.google.com");

  scoped_refptr<FileBasedPolicyLoader> loader(
      new FileBasedPolicyLoader(base::WeakPtr<FileBasedPolicyProvider>(),
                                test_delegate,
                                kSettleIntervalSecondsForTesting,
                                kReloadIntervalMinutesForTesting));
  scoped_ptr<DictionaryValue> policy(loader->GetPolicy());
  EXPECT_TRUE(policy.get());
  EXPECT_EQ(1U, policy->size());

  std::string str_value;
  EXPECT_TRUE(policy->GetString("HomepageLocation", &str_value));
  EXPECT_EQ("http://www.google.com", str_value);

  loader->Stop();
}

TEST_F(FileBasedPolicyLoaderTest, TestRefresh) {
  MockPolicyProvider provider;
  TestDelegate* test_delegate = new TestDelegate;

  scoped_refptr<FileBasedPolicyLoader> loader(
      new FileBasedPolicyLoader(provider.AsWeakPtr(),
                                test_delegate,
                                kSettleIntervalSecondsForTesting,
                                kReloadIntervalMinutesForTesting));
  scoped_ptr<DictionaryValue> policy(loader->GetPolicy());
  EXPECT_TRUE(policy.get());
  EXPECT_EQ(0U, policy->size());

  test_delegate->dict()->SetString("HomepageLocation", "http://www.google.com");

  EXPECT_CALL(provider, NotifyStoreOfPolicyChange()).Times(1);
  loader->OnFilePathChanged(FilePath(FILE_PATH_LITERAL("fake")));

  // Run the loop. The refresh should be handled immediately since the settle
  // interval has been disabled.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&provider);

  policy.reset(loader->GetPolicy());
  EXPECT_TRUE(policy.get());
  EXPECT_EQ(1U, policy->size());

  std::string str_value;
  EXPECT_TRUE(policy->GetString("HomepageLocation", &str_value));
  EXPECT_EQ("http://www.google.com", str_value);

  loader->Stop();
}

}  // namespace policy
