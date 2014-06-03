// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/spellcheck_common.h"

using sync_integration_test_util::AwaitCommitActivityCompletion;

class TwoClientDictionarySyncTest : public SyncTest {
 public:
  TwoClientDictionarySyncTest() : SyncTest(TWO_CLIENT) {}
  virtual ~TwoClientDictionarySyncTest() {}

  virtual bool TestUsesSelfNotifications() OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientDictionarySyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());

  std::vector<std::string> words;
  words.push_back("foo");
  words.push_back("bar");
  ASSERT_EQ(num_clients(), static_cast<int>(words.size()));

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(dictionary_helper::AddWord(i, words[i]));
  }
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(words.size(), dictionary_helper::GetDictionarySize(0));

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(dictionary_helper::RemoveWord(i, words[i]));
  }
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(0UL, dictionary_helper::GetDictionarySize(0));

  DisableVerifier();
  for (int i = 0; i < num_clients(); ++i)
    ASSERT_TRUE(dictionary_helper::AddWord(i, words[i]));
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(words.size(), dictionary_helper::GetDictionarySize(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, SimultaneousAdd) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo");
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, SimultaneousRemove) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo");
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::RemoveWord(i, "foo");
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(0UL, dictionary_helper::GetDictionarySize(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, RemoveOnAAddOnB) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());

  std::string word = "foo";
  // Add on client A, check it appears on B.
  ASSERT_TRUE(dictionary_helper::AddWord(0, word));
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  // Remove on client A, check it disappears on B.
  ASSERT_TRUE(dictionary_helper::RemoveWord(0, word));
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  // Add on client B, check it appears on A.
  ASSERT_TRUE(dictionary_helper::AddWord(1, word));
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));
}

