// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/scoped_temp_dir.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_profile.h"
#include "content/test/test_browser_thread.h"
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
  explicit TestingSpellCheckProfile(const FilePath& profile_dir)
      : SpellCheckProfile(profile_dir),
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

typedef SpellCheckProfile::ReinitializeResult ResultType;
}  // namespace

class SpellCheckProfileTest : public testing::Test {
 protected:
  SpellCheckProfileTest()
      : file_thread_(BrowserThread::FILE) {
  }

  // SpellCheckHost will be deleted on FILE thread.
  content::TestBrowserThread file_thread_;
};

TEST_F(SpellCheckProfileTest, ReinitializeEnabled) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  TestingSpellCheckProfile target(dir.path());
  target.SetHostToBeCreated(host.release());

  // The first call should create host.
  ResultType result1 = target.ReinitializeHost(false, true, "", NULL);
  EXPECT_EQ(target.create_host_calls_, 1U);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_CREATED_HOST);

  // The second call should be ignored.
  ResultType result2 = target.ReinitializeHost(false, true, "", NULL);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_DID_NOTHING);
  EXPECT_EQ(target.create_host_calls_, 1U);

  // Host should become ready after the notification.
  EXPECT_FALSE(target.IsCreatedHostReady());
  target.SpellCheckHostInitialized(0);
  EXPECT_TRUE(target.IsCreatedHostReady());
}

TEST_F(SpellCheckProfileTest, ReinitializeDisabled) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  TestingSpellCheckProfile target(dir.path());

  target.returning_from_create_.reset(host.release());

  // If enabled is false, nothing should happen
  ResultType result1 = target.ReinitializeHost(false, false, "", NULL);
  EXPECT_EQ(target.create_host_calls_, 0U);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_DID_NOTHING);

  // Nothing should happen even if forced.
  ResultType result2 = target.ReinitializeHost(true, false, "", NULL);
  EXPECT_EQ(target.create_host_calls_, 0U);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_DID_NOTHING);
}

TEST_F(SpellCheckProfileTest, ReinitializeRemove) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  TestingSpellCheckProfile target(dir.path());

  target.SetHostToBeCreated(host.release());

  // At first, create the host.
  ResultType result1 = target.ReinitializeHost(false, true, "", NULL);
  target.SpellCheckHostInitialized(0);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_CREATED_HOST);
  EXPECT_TRUE(target.IsCreatedHostReady());

  // Then the host should be deleted if it's forced to be disabled.
  ResultType result2 = target.ReinitializeHost(true, false, "", NULL);
  target.SpellCheckHostInitialized(0);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_REMOVED_HOST);
  EXPECT_FALSE(target.IsCreatedHostReady());
}

TEST_F(SpellCheckProfileTest, ReinitializeRecreate) {
  scoped_ptr<MockSpellCheckHost> host1(new MockSpellCheckHost());
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  TestingSpellCheckProfile target(dir.path());

  target.SetHostToBeCreated(host1.release());

  // At first, create the host.
  ResultType result1 = target.ReinitializeHost(false, true, "", NULL);
  target.SpellCheckHostInitialized(0);
  EXPECT_EQ(target.create_host_calls_, 1U);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_CREATED_HOST);
  EXPECT_TRUE(target.IsCreatedHostReady());

  // Then the host should be re-created if it's forced to recreate.
  scoped_ptr<MockSpellCheckHost> host2(new MockSpellCheckHost());
  target.SetHostToBeCreated(host2.release());

  ResultType result2 = target.ReinitializeHost(true, true, "", NULL);
  target.SpellCheckHostInitialized(0);
  EXPECT_EQ(target.create_host_calls_, 2U);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_CREATED_HOST);
  EXPECT_TRUE(target.IsCreatedHostReady());
}

TEST_F(SpellCheckProfileTest, SpellCheckHostInitializedWithCustomWords) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  TestingSpellCheckProfile target(dir.path());

  target.SetHostToBeCreated(host.release());
  target.ReinitializeHost(false, true, "", NULL);

  scoped_ptr<SpellCheckProfile::CustomWordList> loaded_custom_words
    (new SpellCheckProfile::CustomWordList());
  loaded_custom_words->push_back("foo");
  loaded_custom_words->push_back("bar");
  SpellCheckProfile::CustomWordList expected(*loaded_custom_words);
  target.SpellCheckHostInitialized(loaded_custom_words.release());
  EXPECT_EQ(target.GetCustomWords(), expected);
}

TEST_F(SpellCheckProfileTest, CustomWordAddedLocally) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  TestingSpellCheckProfile target(dir.path());

  target.SetHostToBeCreated(host.release());
  target.ReinitializeHost(false, true, "", NULL);

  scoped_ptr<SpellCheckProfile::CustomWordList> loaded_custom_words
    (new SpellCheckProfile::CustomWordList());
  target.SpellCheckHostInitialized(NULL);
  SpellCheckProfile::CustomWordList expected;
  EXPECT_EQ(target.GetCustomWords(), expected);
  target.CustomWordAddedLocally("foo");
  expected.push_back("foo");
  EXPECT_EQ(target.GetCustomWords(), expected);
  target.CustomWordAddedLocally("bar");
  expected.push_back("bar");
  EXPECT_EQ(target.GetCustomWords(), expected);
}

TEST_F(SpellCheckProfileTest, SaveAndLoad) {
  scoped_ptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  TestingSpellCheckProfile target(dir.path());

  target.SetHostToBeCreated(host.release());
  target.ReinitializeHost(false, true, "", NULL);

  scoped_ptr<SpellCheckProfile::CustomWordList> loaded_custom_words(
    new SpellCheckProfile::CustomWordList());
  target.LoadCustomDictionary(loaded_custom_words.get());

  // The custom word list should be empty now.
  SpellCheckProfile::CustomWordList expected;
  EXPECT_EQ(*loaded_custom_words, expected);

  target.WriteWordToCustomDictionary("foo");
  expected.push_back("foo");

  target.WriteWordToCustomDictionary("bar");
  expected.push_back("bar");

  // The custom word list should include written words.
  target.LoadCustomDictionary(loaded_custom_words.get());
  EXPECT_EQ(*loaded_custom_words, expected);

  // Load in another instance of SpellCheckProfile.
  // The result should be the same.
  scoped_ptr<MockSpellCheckHost> host2(new MockSpellCheckHost());
  TestingSpellCheckProfile target2(dir.path());
  target2.SetHostToBeCreated(host2.release());
  target2.ReinitializeHost(false, true, "", NULL);
  scoped_ptr<SpellCheckProfile::CustomWordList> loaded_custom_words2(
      new SpellCheckProfile::CustomWordList());
  target2.LoadCustomDictionary(loaded_custom_words2.get());
  EXPECT_EQ(*loaded_custom_words2, expected);
}

TEST_F(SpellCheckProfileTest, MultiProfile) {
  scoped_ptr<MockSpellCheckHost> host1(new MockSpellCheckHost());
  scoped_ptr<MockSpellCheckHost> host2(new MockSpellCheckHost());

  ScopedTempDir dir1;
  ScopedTempDir dir2;
  ASSERT_TRUE(dir1.CreateUniqueTempDir());
  ASSERT_TRUE(dir2.CreateUniqueTempDir());
  TestingSpellCheckProfile target1(dir1.path());
  TestingSpellCheckProfile target2(dir2.path());

  target1.SetHostToBeCreated(host1.release());
  target1.ReinitializeHost(false, true, "", NULL);
  target2.SetHostToBeCreated(host2.release());
  target2.ReinitializeHost(false, true, "", NULL);

  SpellCheckProfile::CustomWordList expected1;
  SpellCheckProfile::CustomWordList expected2;

  target1.WriteWordToCustomDictionary("foo");
  target1.WriteWordToCustomDictionary("bar");
  expected1.push_back("foo");
  expected1.push_back("bar");

  target2.WriteWordToCustomDictionary("hoge");
  target2.WriteWordToCustomDictionary("fuga");
  expected2.push_back("hoge");
  expected2.push_back("fuga");

  SpellCheckProfile::CustomWordList actual1;
  target1.LoadCustomDictionary(&actual1);
  EXPECT_EQ(actual1, expected1);

  SpellCheckProfile::CustomWordList actual2;
  target2.LoadCustomDictionary(&actual2);
  EXPECT_EQ(actual2, expected2);
}
