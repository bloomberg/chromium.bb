// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

using base::ASCIIToUTF16;
using sync_integration_test_util::AwaitCommitActivityCompletion;

class TwoClientSearchEnginesSyncTest : public SyncTest {
 public:
  TwoClientSearchEnginesSyncTest() : SyncTest(TWO_CLIENT) {}
  virtual ~TwoClientSearchEnginesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientSearchEnginesSyncTest);
};

// TCM ID - 8898628.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, Add) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 8912240.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, AddMultiple) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Add a few entries.
  for (int i = 0; i < 3; ++i)
    search_engines_helper::AddSearchEngine(0, i);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 9011135.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, Duplicates) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Add two entries with the same Name and URL (but different keywords).
  // Note that we have to change the GUID of the duplicate.
  search_engines_helper::AddSearchEngine(0, 0);
  Profile* profile = sync_datatype_helper::test()->GetProfile(0);
  TemplateURLServiceFactory::GetForProfile(profile)->Add(
      search_engines_helper::CreateTestTemplateURL(profile, 0,
          ASCIIToUTF16("somethingelse"), "newguid"));
  search_engines_helper::GetVerifierService()->Add(
      search_engines_helper::CreateTestTemplateURL(profile, 0,
          ASCIIToUTF16("somethingelse"), "newguid"));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 9004201.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, UpdateKeyword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);

  // Change the keyword.
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::EditSearchEngine(0, ASCIIToUTF16("test0"),
      ASCIIToUTF16("test0"), ASCIIToUTF16("newkeyword"),
      "http://www.test0.com/");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 8894859.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, UpdateUrl) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Change the URL.
  search_engines_helper::EditSearchEngine(0, ASCIIToUTF16("test0"),
      ASCIIToUTF16("test0"), ASCIIToUTF16("test0"),
      "http://www.wikipedia.org/q=%s");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 8910490.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, UpdateName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Change the short name.
  search_engines_helper::EditSearchEngine(0, ASCIIToUTF16("test0"),
      ASCIIToUTF16("New Name"), ASCIIToUTF16("test0"), "http://www.test0.com/");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 8898660.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::DeleteSearchEngineBySeed(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 9004196.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, ConflictKeyword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Add a different search engine to each client, but make their keywords
  // conflict.
  search_engines_helper::AddSearchEngine(0, 0);
  search_engines_helper::AddSearchEngine(1, 1);
  TemplateURLService* service =
      search_engines_helper::GetServiceForBrowserContext(1);
  TemplateURL* turl = service->GetTemplateURLForKeyword(ASCIIToUTF16("test1"));
  EXPECT_TRUE(turl);
  service->ResetTemplateURL(turl, turl->short_name(), ASCIIToUTF16("test0"),
                            turl->url());

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 9004187.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, MergeMultiple) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Set up some different search engines on each client, with some interesting
  // conflicts.
  // client0: { SE0, SE1, SE2 }
  for (int i = 0; i < 3; ++i)
    search_engines_helper::AddSearchEngine(0, i);

  // client1: { SE0, SE2, SE3, SE0 + different URL }
  search_engines_helper::AddSearchEngine(1, 0);
  search_engines_helper::AddSearchEngine(1, 2);
  search_engines_helper::AddSearchEngine(1, 3);
  Profile* profile = sync_datatype_helper::test()->GetProfile(1);
  TemplateURLServiceFactory::GetForProfile(profile)->Add(
      search_engines_helper::CreateTestTemplateURL(profile, 0,
          ASCIIToUTF16("somethingelse.com"), "http://www.somethingelse.com/",
          "somethingelse"));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 8906436.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  search_engines_helper::AddSearchEngine(0, 0);
  ASSERT_TRUE(
      AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
  ASSERT_FALSE(search_engines_helper::ServiceMatchesVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// TCM ID - 8891809.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, SyncDefault) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Change the default to the new search engine, sync, and ensure that it
  // changed in the second client. AllServicesMatch does a default search
  // provider check.
  search_engines_helper::ChangeDefaultSearchProvider(0, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

// Ensure that we can change the search engine and immediately delete it
// without putting the clients out of sync.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, DeleteSyncedDefault) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);
  search_engines_helper::AddSearchEngine(0, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  search_engines_helper::ChangeDefaultSearchProvider(0, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Change the default on the first client and delete the old default.
  search_engines_helper::ChangeDefaultSearchProvider(0, 1);
  search_engines_helper::DeleteSearchEngineBySeed(0, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}
