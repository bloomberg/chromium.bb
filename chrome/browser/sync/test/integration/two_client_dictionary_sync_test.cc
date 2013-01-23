// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/spellcheck_common.h"

class TwoClientDictionarySyncTest : public SyncTest {
 public:
  TwoClientDictionarySyncTest() : SyncTest(TWO_CLIENT) {}
  virtual ~TwoClientDictionarySyncTest() {}

  virtual void AddOptionalTypesToCommandLine(CommandLine* cl) OVERRIDE {
    dictionary_helper::EnableDictionarySync(cl);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientDictionarySyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  std::vector<std::string> words;
  words.push_back("foo");
  words.push_back("bar");
  ASSERT_EQ(num_clients(), static_cast<int>(words.size()));

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(dictionary_helper::AddWord(i, words[i]));
    ASSERT_TRUE(GetClient(i)->AwaitMutualSyncCycleCompletion(
        GetClient((i + 1) % 2)));
  }
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
  ASSERT_EQ(words.size(), dictionary_helper::GetDictionarySize(0));

  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(dictionary_helper::RemoveWord(i, words[i]));
    ASSERT_TRUE(GetClient(i)->AwaitMutualSyncCycleCompletion(
        GetClient((i + 1) % 2)));
  }
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
  ASSERT_EQ(0UL, dictionary_helper::GetDictionarySize(0));

  DisableVerifier();
  for (int i = 0; i < num_clients(); ++i)
    ASSERT_TRUE(dictionary_helper::AddWord(i, words[i]));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
  ASSERT_EQ(words.size(), dictionary_helper::GetDictionarySize(0));

  MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, SimultaneousAdd) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));

  MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, SimultaneousRemove) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::RemoveWord(i, "foo");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
  ASSERT_EQ(0UL, dictionary_helper::GetDictionarySize(0));

  MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, RemoveOnAAddOnB) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  std::string word = "foo";
  // Add on client A
  ASSERT_TRUE(dictionary_helper::AddWord(0, word));
  // Remove on client A
  ASSERT_TRUE(dictionary_helper::RemoveWord(0, word));
  ASSERT_TRUE(AwaitQuiescence());
  // Add on client B
  ASSERT_TRUE(dictionary_helper::AddWord(1, word));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));

  MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  ASSERT_TRUE(dictionary_helper::AddWord(0, "foo"));
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Added a word"));
  ASSERT_TRUE(dictionary_helper::DictionaryMatchesVerifier(0));
  ASSERT_FALSE(dictionary_helper::DictionaryMatchesVerifier(1));

  MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(TwoClientDictionarySyncTest, Limit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  ASSERT_TRUE(GetClient(0)->DisableSyncForAllDatatypes());
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;
       ++i) {
    ASSERT_TRUE(dictionary_helper::AddWord(
        0, "foo" + base::Uint64ToString(i)));
    ASSERT_TRUE(dictionary_helper::AddWord(
        1, "bar" + base::Uint64ToString(i)));
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(dictionary_helper::DictionariesMatch());

  // Client #0 should have only "foo" set of words.
  ASSERT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary_helper::GetDictionarySize(0));

  // Client #1 should have only "bar" set of words.
  ASSERT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary_helper::GetDictionarySize(1));

  ASSERT_TRUE(GetClient(0)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());

  // Client #0 should have both "foo" and "bar" sets of words.
  ASSERT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS * 2,
            dictionary_helper::GetDictionarySize(0));

  // The sync server and client #1 should have only "bar" set of words.
  ASSERT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary_helper::GetDictionarySize(1));

  MessageLoop::current()->RunUntilIdle();
}
