// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/live_sync/bookmarks_helper.h"
#include "chrome/test/live_sync/live_sync_test.h"

class MultipleClientBookmarksSyncTest : public LiveSyncTest {
 public:
  MultipleClientBookmarksSyncTest() : LiveSyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientBookmarksSyncTest);
};

IN_PROC_BROWSER_TEST_F(MultipleClientBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(BookmarksHelper::AddURL(
        i, base::StringPrintf(L"Google URL %d", i),
            GURL(StringPrintf("http://www.google.com/%d", i))) != NULL);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(BookmarksHelper::AllModelsMatch());
}
