// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/scoped_temp_dir.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

class MockSpellCheckHost : public SpellCheckHost {
 public:
  MOCK_METHOD0(UnsetProfile, void());
  MOCK_METHOD1(InitForRenderer, void(content::RenderProcessHost* process));
  MOCK_METHOD1(AddWord, void(const std::string& word));
  MOCK_CONST_METHOD0(GetDictionaryFile, const base::PlatformFile&());
  MOCK_CONST_METHOD0(GetCustomWords,
                     const SpellCheckProfile::CustomWordList&());
  MOCK_CONST_METHOD0(GetLastAddedFile, const std::string&());
  MOCK_CONST_METHOD0(GetLanguage, const std::string&());
  MOCK_CONST_METHOD0(IsUsingPlatformChecker, bool());
  MOCK_CONST_METHOD0(GetMetrics, SpellCheckHostMetrics*());
  MOCK_CONST_METHOD0(IsReady, bool());
};

class TestingSpellCheckProfile : public SpellCheckProfile {
 public:
  explicit TestingSpellCheckProfile(Profile* profile)
      : SpellCheckProfile(profile),
        create_host_calls_(0) {
  }
  virtual SpellCheckHost* CreateHost(
      SpellCheckProfileProvider* profile,
      const std::string& language,
      net::URLRequestContextGetter* request_context,
      SpellCheckHostMetrics* metrics) {
    create_host_calls_++;
    return returning_from_create_.release();
  }

  virtual bool IsTesting() const {
    return true;
  }

  bool IsCreatedHostReady() {
    return !!GetHost();
  }

  void SetHostToBeCreated(MockSpellCheckHost* host) {
    EXPECT_CALL(*host, UnsetProfile()).Times(1);
    EXPECT_CALL(*host, IsReady()).WillRepeatedly(testing::Return(true));
    returning_from_create_.reset(host);
  }

  size_t create_host_calls_;
  scoped_ptr<SpellCheckHost> returning_from_create_;
};

ProfileKeyedService* BuildTestingSpellCheckProfile(Profile* profile) {
  return new TestingSpellCheckProfile(profile);
}

class SpellCheckProfileTest : public testing::Test {
 protected:
  SpellCheckProfileTest()
      : file_thread_(BrowserThread::FILE),
        profile_(new TestingProfile()) {
    target_ = BuildSpellCheckProfileWith(profile_.get());
  }

  static TestingSpellCheckProfile* BuildSpellCheckProfileWith(
      TestingProfile* profile) {
    return static_cast<TestingSpellCheckProfile*>(
        SpellCheckFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, &BuildTestingSpellCheckProfile));
  }

  // SpellCheckHost will be deleted on FILE thread.
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;

  // Our normal profile, created as part of test startup.
  TestingSpellCheckProfile* target_;
};

}  // namespace

TEST_F(SpellCheckProfileTest, ReinitializeEnabled) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  target_->SetHostToBeCreated(host.release());

  // The first call should create host.
  SpellCheckProfile::ReinitializeResult result1 =
      target_->ReinitializeHostImpl(false, true, "", NULL);
  EXPECT_EQ(target_->create_host_calls_, 1U);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_CREATED_HOST);

  // The second call should be ignored.
  SpellCheckProfile::ReinitializeResult result2 =
      target_->ReinitializeHostImpl(false, true, "", NULL);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_DID_NOTHING);
  EXPECT_EQ(target_->create_host_calls_, 1U);

  // Host should become ready after the notification.
  EXPECT_FALSE(target_->IsCreatedHostReady());
  target_->SpellCheckHostInitialized(0);
  EXPECT_TRUE(target_->IsCreatedHostReady());
}

TEST_F(SpellCheckProfileTest, ReinitializeDisabled) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  target_->returning_from_create_.reset(host.release());

  // If enabled is false, nothing should happen
  SpellCheckProfile::ReinitializeResult result1 =
      target_->ReinitializeHostImpl(false, false, "", NULL);
  EXPECT_EQ(target_->create_host_calls_, 0U);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_DID_NOTHING);

  // Nothing should happen even if forced.
  SpellCheckProfile::ReinitializeResult result2 =
      target_->ReinitializeHostImpl(true, false, "", NULL);
  EXPECT_EQ(target_->create_host_calls_, 0U);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_DID_NOTHING);
}

TEST_F(SpellCheckProfileTest, ReinitializeRemove) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  target_->SetHostToBeCreated(host.release());

  // At first, create the host.
  SpellCheckProfile::ReinitializeResult result1 =
      target_->ReinitializeHostImpl(false, true, "", NULL);
  target_->SpellCheckHostInitialized(0);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_CREATED_HOST);
  EXPECT_TRUE(target_->IsCreatedHostReady());

  // Then the host should be deleted if it's forced to be disabled.
  SpellCheckProfile::ReinitializeResult result2 =
      target_->ReinitializeHostImpl(true, false, "", NULL);
  target_->SpellCheckHostInitialized(0);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_REMOVED_HOST);
  EXPECT_FALSE(target_->IsCreatedHostReady());
}

TEST_F(SpellCheckProfileTest, ReinitializeRecreate) {
  scoped_ptr<MockSpellCheckHost> host1(new MockSpellCheckHost());
  target_->SetHostToBeCreated(host1.release());

  // At first, create the host.
  SpellCheckProfile::ReinitializeResult result1 =
      target_->ReinitializeHostImpl(false, true, "", NULL);
  target_->SpellCheckHostInitialized(0);
  EXPECT_EQ(target_->create_host_calls_, 1U);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_CREATED_HOST);
  EXPECT_TRUE(target_->IsCreatedHostReady());

  // Then the host should be re-created if it's forced to recreate.
  scoped_ptr<MockSpellCheckHost> host2(new MockSpellCheckHost());
  target_->SetHostToBeCreated(host2.release());

  SpellCheckProfile::ReinitializeResult result2 =
      target_->ReinitializeHostImpl(true, true, "", NULL);
  target_->SpellCheckHostInitialized(0);
  EXPECT_EQ(target_->create_host_calls_, 2U);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_CREATED_HOST);
  EXPECT_TRUE(target_->IsCreatedHostReady());
}

TEST_F(SpellCheckProfileTest, SpellCheckHostInitializedWithCustomWords) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  target_->SetHostToBeCreated(host.release());
  target_->ReinitializeHostImpl(false, true, "", NULL);

  scoped_ptr<SpellCheckProfile::CustomWordList> loaded_custom_words
    (new SpellCheckProfile::CustomWordList());
  loaded_custom_words->push_back("foo");
  loaded_custom_words->push_back("bar");
  SpellCheckProfile::CustomWordList expected(*loaded_custom_words);
  target_->SpellCheckHostInitialized(loaded_custom_words.release());
  EXPECT_EQ(target_->GetCustomWords(), expected);
}

TEST_F(SpellCheckProfileTest, CustomWordAddedLocally) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  target_->SetHostToBeCreated(host.release());
  target_->ReinitializeHostImpl(false, true, "", NULL);

  scoped_ptr<SpellCheckProfile::CustomWordList> loaded_custom_words
    (new SpellCheckProfile::CustomWordList());
  target_->SpellCheckHostInitialized(NULL);
  SpellCheckProfile::CustomWordList expected;
  EXPECT_EQ(target_->GetCustomWords(), expected);
  target_->CustomWordAddedLocally("foo");
  expected.push_back("foo");
  EXPECT_EQ(target_->GetCustomWords(), expected);
  target_->CustomWordAddedLocally("bar");
  expected.push_back("bar");
  EXPECT_EQ(target_->GetCustomWords(), expected);
}

TEST_F(SpellCheckProfileTest, SaveAndLoad) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  target_->SetHostToBeCreated(host.release());
  target_->ReinitializeHostImpl(false, true, "", NULL);

  scoped_ptr<SpellCheckProfile::CustomWordList> loaded_custom_words(
    new SpellCheckProfile::CustomWordList());
  target_->LoadCustomDictionary(loaded_custom_words.get());

  // The custom word list should be empty now.
  SpellCheckProfile::CustomWordList expected;
  EXPECT_EQ(*loaded_custom_words, expected);

  target_->WriteWordToCustomDictionary("foo");
  expected.push_back("foo");

  target_->WriteWordToCustomDictionary("bar");
  expected.push_back("bar");

  // The custom word list should include written words.
  target_->LoadCustomDictionary(loaded_custom_words.get());
  EXPECT_EQ(*loaded_custom_words, expected);

  // Load in another instance of SpellCheckProfile.
  // The result should be the same.
  scoped_ptr<MockSpellCheckHost> host2(new MockSpellCheckHost());
  TestingSpellCheckProfile* target2 =
      BuildSpellCheckProfileWith(profile_.get());
  target2->SetHostToBeCreated(host2.release());
  target2->ReinitializeHostImpl(false, true, "", NULL);
  scoped_ptr<SpellCheckProfile::CustomWordList> loaded_custom_words2(
      new SpellCheckProfile::CustomWordList());
  target2->LoadCustomDictionary(loaded_custom_words2.get());
  EXPECT_EQ(*loaded_custom_words2, expected);
}

TEST_F(SpellCheckProfileTest, MultiProfile) {
  scoped_ptr<MockSpellCheckHost> host1(new MockSpellCheckHost());
  scoped_ptr<MockSpellCheckHost> host2(new MockSpellCheckHost());

  TestingProfile profile2;
  TestingSpellCheckProfile* target2 = BuildSpellCheckProfileWith(&profile2);

  target_->SetHostToBeCreated(host1.release());
  target_->ReinitializeHostImpl(false, true, "", NULL);
  target2->SetHostToBeCreated(host2.release());
  target2->ReinitializeHostImpl(false, true, "", NULL);

  SpellCheckProfile::CustomWordList expected1;
  SpellCheckProfile::CustomWordList expected2;

  target_->WriteWordToCustomDictionary("foo");
  target_->WriteWordToCustomDictionary("bar");
  expected1.push_back("foo");
  expected1.push_back("bar");

  target2->WriteWordToCustomDictionary("hoge");
  target2->WriteWordToCustomDictionary("fuga");
  expected2.push_back("hoge");
  expected2.push_back("fuga");

  SpellCheckProfile::CustomWordList actual1;
  target_->LoadCustomDictionary(&actual1);
  EXPECT_EQ(actual1, expected1);

  SpellCheckProfile::CustomWordList actual2;
  target2->LoadCustomDictionary(&actual2);
  EXPECT_EQ(actual2, expected2);
}
