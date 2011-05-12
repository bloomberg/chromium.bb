// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

#include "base/stringprintf.h"
#include "chrome/browser/profiles/profile.h"

IN_PROC_BROWSER_TEST_F(MultipleClientLiveBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(AddURL(i, base::StringPrintf(L"Google URL %d", i),
        GURL(StringPrintf("http://www.google.com/%d", i))) != NULL);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}
