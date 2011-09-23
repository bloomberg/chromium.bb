// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using search_engines_helper::CreateTestTemplateURL;
using search_engines_helper::GetServiceForProfile;
using search_engines_helper::GetVerifierService;
using search_engines_helper::ServiceMatchesVerifier;

class SingleClientSearchEnginesSyncTest : public SyncTest {
 public:
  SingleClientSearchEnginesSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientSearchEnginesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSearchEnginesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSearchEnginesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ServiceMatchesVerifier(0));

  // TODO(stevet): Write a helper that adds a new search engine entry to both
  // the verifier and a particular profile.
  GetServiceForProfile(0)->Add(CreateTestTemplateURL(0));
  GetVerifierService()->Add(CreateTestTemplateURL(0));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for search engines to update."));
  ASSERT_TRUE(ServiceMatchesVerifier(0));
}
