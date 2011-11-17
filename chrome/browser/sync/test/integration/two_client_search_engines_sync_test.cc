// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using search_engines_helper::AddSearchEngine;
using search_engines_helper::AllServicesMatch;
using search_engines_helper::ChangeDefaultSearchProvider;
using search_engines_helper::CreateTestTemplateURL;
using search_engines_helper::DeleteSearchEngineByKeyword;
using search_engines_helper::DeleteSearchEngineBySeed;
using search_engines_helper::EditSearchEngine;
using search_engines_helper::GetServiceForProfile;
using search_engines_helper::GetVerifierService;
using search_engines_helper::ServiceMatchesVerifier;

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
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 8912240.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, AddMultiple) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  // Add a few entries.
  for (int i = 0; i < 3; ++i) {
    AddSearchEngine(0, i);
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 9011135.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, Duplicates) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  // Add two entries with the same Name and URL (but different keywords).
  // Note that we have to change the GUID of the duplicate.
  AddSearchEngine(0, 0);

  TemplateURL* dupe = CreateTestTemplateURL(0);
  dupe->set_keyword(ASCIIToUTF16("somethingelse"));
  dupe->set_sync_guid("newguid");
  GetServiceForProfile(0)->Add(dupe);

  TemplateURL* verifier_dupe = CreateTestTemplateURL(0);
  verifier_dupe->set_keyword(ASCIIToUTF16("somethingelse"));
  verifier_dupe->set_sync_guid("newguid");
  GetVerifierService()->Add(verifier_dupe);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 9004201.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, UpdateKeyword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);

  // Change the keyword.
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());

  EditSearchEngine(0, "test0", "test0", "newkeyword", "http://www.test0.com/");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 8894859.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, UpdateUrl) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());

  // Change the URL.
  EditSearchEngine(0, "test0", "test0", "test0",
                   "http://www.wikipedia.org/q=%s");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 8910490.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, UpdateName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());

  // Change the short name.
  EditSearchEngine(0, "test0", "New Name", "test0", "http://www.test0.com/");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 8898660.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());

  DeleteSearchEngineBySeed(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 9004196.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, ConflictKeyword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  ASSERT_TRUE(AllServicesMatch());

  // Add a different search engine to each client, but make their keywords
  // conflict.
  AddSearchEngine(0, 0);
  AddSearchEngine(1, 1);
  const TemplateURL* turl = GetServiceForProfile(1)->
      GetTemplateURLForKeyword(ASCIIToUTF16("test1"));
  EXPECT_TRUE(turl);
  GetServiceForProfile(1)->ResetTemplateURL(turl,
                                            turl->short_name(),
                                            ASCIIToUTF16("test0"),
                                            turl->url()->url());

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 9004187.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, MergeMultiple) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  ASSERT_TRUE(AllServicesMatch());

  // Set up some different search engines on each client, with some interesting
  // conflicts.
  // client0: { SE0, SE1, SE2 }
  for (int i = 0; i < 3; ++i) {
    AddSearchEngine(0, i);
  }

  // client1: { SE0, SE2, SE3, SE0 + different URL }
  AddSearchEngine(1, 0);
  AddSearchEngine(1, 2);
  AddSearchEngine(1, 3);
  TemplateURL* turl = CreateTestTemplateURL(0);
  turl->SetURL("http://www.somethingelse.com/", 0, 0);
  turl->set_keyword(ASCIIToUTF16("somethingelse.com"));
  turl->set_sync_guid("somethingelse");
  GetServiceForProfile(1)->Add(turl);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 8906436.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllServicesMatch());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  AddSearchEngine(0, 0);
  ASSERT_TRUE(
      GetClient(0)->AwaitFullSyncCompletion("Added a search engine."));
  ASSERT_TRUE(ServiceMatchesVerifier(0));
  ASSERT_FALSE(ServiceMatchesVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 8891809.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, SyncDefault) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Change the default to the new search engine, sync, and ensure that it
  // changed in the second client. AllServicesMatch does a default search
  // provider check.
  ChangeDefaultSearchProvider(0, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(AllServicesMatch());
}

// Ensure that we can change the search engine and immediately delete it
// without putting the clients out of sync.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, DeleteSyncedDefault) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);
  AddSearchEngine(0, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ChangeDefaultSearchProvider(0, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());

  // Change the default on the first client and delete the old default.
  ChangeDefaultSearchProvider(0, 1);
  DeleteSearchEngineBySeed(0, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(AllServicesMatch());
}
