// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

// TODO(rsimha): Marking this as flaky until crbug.com/53931 is fixed.
IN_PROC_BROWSER_TEST_F(ManyClientLiveBookmarksSyncTest, FLAKY_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  v->AddURL(GetBookmarkModel(0), GetBookmarkBarNode(0), 0, L"Google URL",
      GURL("http://www.google.com/"));
  GetClient(0)->AwaitGroupSyncCycleCompletion(clients());

  for (int i = 0; i < num_clients(); ++i) {
    v->ExpectMatch(GetBookmarkModel(i));
  }
}
