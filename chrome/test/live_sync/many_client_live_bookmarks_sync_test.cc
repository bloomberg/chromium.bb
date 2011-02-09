// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

// TODO(rsimha): Enable once http://crbug.com/69604 is fixed.
IN_PROC_BROWSER_TEST_F(ManyClientLiveBookmarksSyncTest, DISABLED_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AddURL(0, L"Google URL", GURL("http://www.google.com/")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));
  ASSERT_TRUE(AllModelsMatch());
}
