// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/spellcheck_common.h"

class MultipleClientDictionarySyncTest : public SyncTest {
 public:
  MultipleClientDictionarySyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientDictionarySyncTest() {}

  virtual void AddOptionalTypesToCommandLine(CommandLine* cl) OVERRIDE {
    dictionary_helper::EnableDictionarySync(cl);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientDictionarySyncTest);
};

IN_PROC_BROWSER_TEST_F(MultipleClientDictionarySyncTest, AddToOne) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  ASSERT_TRUE(dictionary_helper::AddWord(0, "foo"));
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(MultipleClientDictionarySyncTest, AddSameToAll) {
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

IN_PROC_BROWSER_TEST_F(MultipleClientDictionarySyncTest, AddDifferentToAll) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo" + base::IntToString(i));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
  ASSERT_EQ(num_clients(),
            static_cast<int>(dictionary_helper::GetDictionarySize(0)));

  MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(MultipleClientDictionarySyncTest, Limit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  // Add 1300 different words to each of the clients before sync.
  for (int i = 0; i < num_clients(); ++i) {
    GetClient(i)->DisableSyncForAllDatatypes();
    for (size_t j = 0;
         j < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;
         ++j) {
      ASSERT_TRUE(dictionary_helper::AddWord(
          i, "foo-" + base::IntToString(i) + "-" + base::Uint64ToString(j)));
    }
    ASSERT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
              dictionary_helper::GetDictionarySize(i));
  }

  // Turn on sync on client #0. The sync server should contain only data from
  // client #0.
  ASSERT_TRUE(GetClient(0)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary_helper::GetDictionarySize(0));

  // Turn on sync in the rest of the clients one-by-one.
  for (int i = 1; i < num_clients(); ++i) {
    // Client #i has 1300 words before sync.
    ASSERT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
              dictionary_helper::GetDictionarySize(i));
    ASSERT_TRUE(GetClient(i)->EnableSyncForAllDatatypes());
    ASSERT_TRUE(AwaitQuiescence());
    // Client #i should have 2600 words after sync: 1300 of own original words
    // and 1300 words from client #0.
    ASSERT_EQ(
        chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS * 2,
        dictionary_helper::GetDictionarySize(i));
  }

  // Client #0 should still have only 1300 words after syncing all other
  // clients, because it has the same data as the sync server.
  ASSERT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary_helper::GetDictionarySize(0));

  MessageLoop::current()->RunUntilIdle();
}
