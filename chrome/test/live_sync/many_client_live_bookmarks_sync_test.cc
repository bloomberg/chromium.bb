// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

IN_PROC_BROWSER_TEST_F(ManyClientLiveBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AddURL(0, L"Google URL", GURL("http://www.google.com/")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));
  ASSERT_TRUE(AllModelsMatch());
}
