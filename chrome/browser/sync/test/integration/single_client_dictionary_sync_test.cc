// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

class SingleClientDictionarySyncTest : public SyncTest {
 public:
  SingleClientDictionarySyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientDictionarySyncTest() {}

  virtual void AddOptionalTypesToCommandLine(CommandLine* cl) OVERRIDE {
    dictionary_helper::EnableDictionarySync(cl);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientDictionarySyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientDictionarySyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  std::string word = "foo";
  ASSERT_TRUE(dictionary_helper::AddWord(0, word));
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Added a word"));
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  ASSERT_TRUE(dictionary_helper::RemoveWord(0, word));
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Removed a word"));
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  MessageLoop::current()->RunUntilIdle();
}
