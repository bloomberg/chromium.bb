// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

// The E2E tests are designed to run only against real backend servers. They are
// disabled on regular bots. http://crbug.com/431366
#define E2E_ONLY(x) DISABLED_##x

using bookmarks_helper::AddURL;
using bookmarks_helper::AwaitAllModelsMatch;
using bookmarks_helper::CountAllBookmarks;

class TwoClientE2ETest : public SyncTest {
 public:
  TwoClientE2ETest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientE2ETest() override {}
  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientE2ETest);
};

IN_PROC_BROWSER_TEST_F(TwoClientE2ETest, E2E_ONLY(SanitySetup)) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
}

IN_PROC_BROWSER_TEST_F(TwoClientE2ETest, E2E_ONLY(OneClientAddsBookmark)) {
  DisableVerifier();
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
  // All profiles should sync same bookmarks.
  ASSERT_TRUE(AwaitAllModelsMatch()) <<
      "Initial bookmark models did not match for all profiles";
  // For clean profiles, the bookmarks count should be zero. We are not
  // enforcing this, we only check that the final count is equal to initial
  // count plus new bookmarks count.
  int init_bookmarks_count = CountAllBookmarks(0);

  // Add one new bookmark to the first profile.
  ASSERT_TRUE(
      AddURL(0, "Google URL 0", GURL("http://www.google.com/0")) != NULL);

  // Blocks and waits for bookmarks models in all profiles to match.
  ASSERT_TRUE(AwaitAllModelsMatch());
  // Check that total number of bookmarks is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(CountAllBookmarks(i), init_bookmarks_count + 1) <<
        "Total bookmark count is wrong.";
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientE2ETest, E2E_ONLY(TwoClientsAddBookmarks)) {
  DisableVerifier();
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
  // ALl profiles should sync same bookmarks.
  ASSERT_TRUE(AwaitAllModelsMatch()) <<
      "Initial bookmark models did not match for all profiles";
  // For clean profiles, the bookmarks count should be zero. We are not
  // enforcing this, we only check that the final count is equal to initial
  // count plus new bookmarks count.
  int init_bookmarks_count = CountAllBookmarks(0);

  // Add one new bookmark per profile.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(AddURL(i, base::StringPrintf("Google URL %d", i),
        GURL(base::StringPrintf("http://www.google.com/%d", i))) != NULL);
  }

  // Blocks and waits for bookmarks models in all profiles to match.
  ASSERT_TRUE(AwaitAllModelsMatch());

  // Check that total number of bookmarks is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(CountAllBookmarks(i), init_bookmarks_count + num_clients()) <<
        "Total bookmark count is wrong.";
  }
}
