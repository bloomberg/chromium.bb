// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/spellcheck_common.h"

using dictionary_helper::AwaitNumDictionaryEntries;
using chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;

class MultipleClientDictionarySyncTest : public SyncTest {
 public:
  MultipleClientDictionarySyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientDictionarySyncTest() {}

  virtual bool TestUsesSelfNotifications() OVERRIDE { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientDictionarySyncTest);
};

IN_PROC_BROWSER_TEST_F(MultipleClientDictionarySyncTest, AddToOne) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());

  ASSERT_TRUE(dictionary_helper::AddWord(0, "foo"));
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
}

IN_PROC_BROWSER_TEST_F(MultipleClientDictionarySyncTest, AddSameToAll) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo");
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(1UL, dictionary_helper::GetDictionarySize(0));
}

IN_PROC_BROWSER_TEST_F(MultipleClientDictionarySyncTest, AddDifferentToAll) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());

  for (int i = 0; i < num_clients(); ++i)
    dictionary_helper::AddWord(i, "foo" + base::IntToString(i));
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());
  ASSERT_EQ(num_clients(),
            static_cast<int>(dictionary_helper::GetDictionarySize(0)));
}

// Tests the case where the Nth client pushes the server beyond its
// MAX_SYNCABLE_DICTIONARY_WORDS limit.
IN_PROC_BROWSER_TEST_F(MultipleClientDictionarySyncTest, Limit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::AwaitDictionariesMatch());

  const int n = num_clients();

  // Pick a number of initial words per client such that
  // (num-clients()-1) * initial_words
  //     < MAX_SYNCABLE_DICTIONARY_WORDS
  //     < num_clients() * initial_words
  size_t initial_words =
      (MAX_SYNCABLE_DICTIONARY_WORDS + n) / n;

  // Add |initial_words| words to each of the clients before sync.
  for (int i = 0; i < n; ++i) {
    GetClient(i)->DisableSyncForAllDatatypes();
    for (size_t j = 0; j < initial_words; ++j) {
      ASSERT_TRUE(dictionary_helper::AddWord(
          i, "foo-" + base::IntToString(i) + "-" + base::Uint64ToString(j)));
    }
    ASSERT_EQ(initial_words, dictionary_helper::GetDictionarySize(i));
  }

  // As long as we don't get involved in any race conditions where two clients
  // are committing at once, we should be able to guarantee that the server has
  // at most MAX_SYNCABLE_DICTIONARY_WORDS words.  Every client will be able to
  // sync these items.  Clients are allowed to have more words if they're
  // available locally, but they won't be able to commit any words once the
  // server is full.
  //
  // As we enable clients one-by-one, all but the (N-1)th client should be able
  // to commit all of their items.  The last one will have some local data left
  // over.

  // Open the floodgates.   Allow N-1 clients to sync their items.
  for (int i = 0; i < n-1; ++i) {
    SCOPED_TRACE(i);

    // Client #i has |initial_words| words before sync.
    ASSERT_EQ(initial_words, dictionary_helper::GetDictionarySize(i));
    ASSERT_TRUE(GetClient(i)->EnableSyncForAllDatatypes());
  }

  // Wait for clients to catch up.  All should be in sync with the server
  // and have exactly (initial_words * (N-1)) words in their dictionaries.
  for (int i = 0; i < n-1; ++i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(AwaitNumDictionaryEntries(i, initial_words*(n-1)));
  }

  // Add the client that has engough new words to cause an overflow.
  ASSERT_EQ(initial_words, dictionary_helper::GetDictionarySize(n-1));
  ASSERT_TRUE(GetClient(n-1)->EnableSyncForAllDatatypes());

  // The Nth client will receive the initial_words * (n-1) entries that were on
  // the server.  It will commit some of the entries it had locally.  And it
  // will have a few uncommittable items left over.
  ASSERT_TRUE(AwaitNumDictionaryEntries(n-1, initial_words*n));

  // Everyone else should be at the limit.
  for (int i = 0; i < n-1; ++i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(AwaitNumDictionaryEntries(i, MAX_SYNCABLE_DICTIONARY_WORDS));
  }
}
