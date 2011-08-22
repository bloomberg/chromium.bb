// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSpellCheckHost : public SpellCheckHost {
 public:
  MOCK_METHOD0(UnsetObserver, void());
  MOCK_METHOD1(InitForRenderer, void(RenderProcessHost* process));
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
  TestingSpellCheckProfile()
      : create_host_calls_(0) {
  }

  virtual SpellCheckHost* CreateHost(
      SpellCheckHostObserver* observer,
      const std::string& language,
      net::URLRequestContextGetter* request_context,
      SpellCheckHostMetrics* metrics) {
    create_host_calls_++;
    return returning_from_create_.get();
  }

  virtual bool IsTesting() const {
    return true;
  }

  bool IsCreatedHostReady() {
    return GetHost() == returning_from_create_.get();
  }

  void SetHostToBeCreated(MockSpellCheckHost* host) {
    EXPECT_CALL(*host, UnsetObserver()).Times(1);
    EXPECT_CALL(*host, IsReady()).WillRepeatedly(testing::Return(true));
    returning_from_create_ = host;
  }

  size_t create_host_calls_;
  scoped_refptr<SpellCheckHost> returning_from_create_;
};

typedef SpellCheckProfile::ReinitializeResult ResultType;
}  // namespace

class SpellCheckProfileTest : public testing::Test {
 protected:
  SpellCheckProfileTest()
      : file_thread_(BrowserThread::FILE) {
  }

  // SpellCheckHost will be deleted on FILE thread.
  BrowserThread file_thread_;
};

TEST_F(SpellCheckProfileTest, ReinitializeEnabled) {
  scoped_refptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  TestingSpellCheckProfile target;
  target.SetHostToBeCreated(host.get());

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
  scoped_refptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  TestingSpellCheckProfile target;
  target.returning_from_create_ = host.get();

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
  scoped_refptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  TestingSpellCheckProfile target;
  target.SetHostToBeCreated(host.get());


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
  scoped_refptr<MockSpellCheckHost> host1(new MockSpellCheckHost());
  TestingSpellCheckProfile target;
  target.SetHostToBeCreated(host1.get());

  // At first, create the host.
  ResultType result1 = target.ReinitializeHost(false, true, "", NULL);
  target.SpellCheckHostInitialized(0);
  EXPECT_EQ(target.create_host_calls_, 1U);
  EXPECT_EQ(result1, SpellCheckProfile::REINITIALIZE_CREATED_HOST);
  EXPECT_TRUE(target.IsCreatedHostReady());

  // Then the host should be re-created if it's forced to recreate.
  scoped_refptr<MockSpellCheckHost> host2(new MockSpellCheckHost());
  target.SetHostToBeCreated(host2.get());

  ResultType result2 = target.ReinitializeHost(true, true, "", NULL);
  target.SpellCheckHostInitialized(0);
  EXPECT_EQ(target.create_host_calls_, 2U);
  EXPECT_EQ(result2, SpellCheckProfile::REINITIALIZE_CREATED_HOST);
  EXPECT_TRUE(target.IsCreatedHostReady());
}

TEST_F(SpellCheckProfileTest, SpellCheckHostInitializedWithCustomWords) {
  scoped_refptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  TestingSpellCheckProfile target;
  target.SetHostToBeCreated(host.get());
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
  scoped_refptr<MockSpellCheckHost> host(new MockSpellCheckHost());
  TestingSpellCheckProfile target;
  target.SetHostToBeCreated(host.get());
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
