// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/bookmarks/browser/bookmark_node.h"

// The E2E_ONLY tests are designed to run only against real backend
// servers. They are disabled on regular bots. http://crbug.com/431366
#define E2E_ONLY(x) DISABLED_##x

using bookmarks_helper::AddURL;
using bookmarks_helper::AwaitAllModelsMatch;
using bookmarks_helper::CountAllBookmarks;
using bookmarks_helper::GetBookmarkBarNode;

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

// Verify that a bookmark added on a client with bookmark syncing disabled gets
// synced to a second client once bookmark syncing is re-enabled.
IN_PROC_BROWSER_TEST_F(TwoClientE2ETest, AddBookmarkWhileDisabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitAllModelsMatch())
      << "Initial bookmark models did not match for all profiles";
  const int initial_count = CountAllBookmarks(0);

  // Verify that we can sync. Add a bookmark on the first client and verify it's
  // synced to the second client.
  const std::string url_title = "a happy little url";
  const GURL url("https://example.com");
  ASSERT_TRUE(AddURL(0, GetBookmarkBarNode(0), 0, url_title, url) != NULL);
  ASSERT_TRUE(AwaitAllModelsMatch());
  ASSERT_EQ(initial_count + 1, CountAllBookmarks(0));
  ASSERT_EQ(initial_count + 1, CountAllBookmarks(1));

  // Disable bookmark syncing on the first client, add another bookmark,
  // re-enable bookmark syncing and see that the second bookmark reaches the
  // second client.
  ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::BOOKMARKS));
  const std::string url_title_2 = "another happy little url";
  const GURL url_2("https://example.com/second");
  ASSERT_TRUE(AddURL(0, GetBookmarkBarNode(0), 0, url_title_2, url_2) != NULL);
  ASSERT_TRUE(GetClient(0)->EnableSyncForDatatype(syncer::BOOKMARKS));
  ASSERT_TRUE(AwaitAllModelsMatch());
  ASSERT_EQ(initial_count + 2, CountAllBookmarks(0));
  ASSERT_EQ(initial_count + 2, CountAllBookmarks(1));
}
