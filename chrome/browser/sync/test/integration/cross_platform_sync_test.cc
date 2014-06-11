// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::ModelMatchesVerifier;
using sync_integration_test_util::AwaitCommitActivityCompletion;

// These tests are run on the Chrome on iOS buildbots as part of cross-platform
// sync integration tests, and are not meant to be run on the chromium
// buildbots. As a result, all tests below must have a DISABLED_ annotation,
// which will be overridden when they are run on the Chrome on iOS buildbots.
class CrossPlatformSyncTest : public SyncTest {
 public:
  CrossPlatformSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~CrossPlatformSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossPlatformSyncTest);
};

IN_PROC_BROWSER_TEST_F(CrossPlatformSyncTest, DISABLED_AddBookmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AddURL(0, "Google", GURL("http://www.google.co.uk")));
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));
}
