// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::AwaitAllModelsMatch;

class MultipleClientBookmarksSyncTest : public SyncTest {
 public:
  MultipleClientBookmarksSyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientBookmarksSyncTest() {}

  virtual bool TestUsesSelfNotifications() OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientBookmarksSyncTest);
};

IN_PROC_BROWSER_TEST_F(MultipleClientBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(AddURL(i, base::StringPrintf("Google URL %d", i),
        GURL(base::StringPrintf("http://www.google.com/%d", i))) != NULL);
  }
  ASSERT_TRUE(AwaitAllModelsMatch());
}
