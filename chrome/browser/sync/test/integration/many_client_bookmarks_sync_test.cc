// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;

class ManyClientBookmarksSyncTest : public SyncTest {
 public:
  ManyClientBookmarksSyncTest() : SyncTest(MANY_CLIENT) {}
  virtual ~ManyClientBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientBookmarksSyncTest);
};

IN_PROC_BROWSER_TEST_F(ManyClientBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AddURL(0, L"Google URL", GURL("http://www.google.com/")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));
  ASSERT_TRUE(AllModelsMatch());
}
