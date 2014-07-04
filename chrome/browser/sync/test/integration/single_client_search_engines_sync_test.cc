// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/search_engines/template_url_service.h"

using sync_integration_test_util::AwaitCommitActivityCompletion;

class SingleClientSearchEnginesSyncTest : public SyncTest {
 public:
  SingleClientSearchEnginesSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientSearchEnginesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSearchEnginesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSearchEnginesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));

  search_engines_helper::AddSearchEngine(0, 0);

  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
}
