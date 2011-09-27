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
using search_engines_helper::CreateTestTemplateURL;
using search_engines_helper::DeleteSearchEngine;
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

// TCM ID - 9004201.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, UpdateKeyword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);

  // Change the keyword.
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());

  EditSearchEngine(0, "test0", "newkeyword", "test0", "http://www.test0.com/");

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

// TCM ID - 8898660.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllServicesMatch());

  AddSearchEngine(0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());

  DeleteSearchEngine(0, "test0");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllServicesMatch());
}

// TCM ID - 8906436.
IN_PROC_BROWSER_TEST_F(TwoClientSearchEnginesSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllServicesMatch());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  AddSearchEngine(0, 0);
  ASSERT_TRUE(
      GetClient(0)->AwaitSyncCycleCompletion("Added a search engine."));
  ASSERT_TRUE(ServiceMatchesVerifier(0));
  ASSERT_FALSE(ServiceMatchesVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllServicesMatch());
}
