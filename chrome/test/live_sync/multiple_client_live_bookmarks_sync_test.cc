// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

IN_PROC_BROWSER_TEST_F(MultipleClientLiveBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  for (int i = 0; i < num_clients(); ++i) {
    v->AddURL(GetBookmarkModel(i), GetBookmarkBarNode(i), 0,
        StringPrintf(L"Google URL %d", i),
        GURL(StringPrintf("http://www.google.com/%d", i)));
  }
  ProfileSyncServiceTestHarness::AwaitQuiescence(clients());
  for (int i = 1; i < num_clients(); ++i) {
    BookmarkModelVerifier::ExpectModelsMatch(GetBookmarkModel(0),
        GetBookmarkModel(i));
  }
}
