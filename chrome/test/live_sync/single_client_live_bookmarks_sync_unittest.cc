// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/test/live_sync/bookmark_model_verifier.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

class SingleClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  SingleClientLiveBookmarksSyncTest() { }
  ~SingleClientLiveBookmarksSyncTest() { }

  bool SetupSync() {
    client_.reset(new ProfileSyncServiceTestHarness(
        browser()->profile(), username_, password_));
    return client_->SetupSync();
  }

  ProfileSyncServiceTestHarness* client() { return client_.get(); }
  ProfileSyncService* service() { return client_->service(); }

 private:
  scoped_ptr<ProfileSyncServiceTestHarness> client_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientLiveBookmarksSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientLiveBookmarksSyncTest, Sanity) {
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* m = browser()->profile()->GetBookmarkModel();
  BlockUntilLoaded(m);

  // Starting state:
  // other_node
  //    -> top
  //      -> tier1_a
  //        -> http://mail.google.com  "tier1_a_url0"
  //        -> http://www.pandora.com  "tier1_a_url1"
  //        -> http://www.facebook.com "tier1_a_url2"
  //      -> tier1_b
  //        -> http://www.nhl.com "tier1_b_url0"
  const BookmarkNode* top = verifier->AddGroup(m, m->other_node(), 0, L"top");
  const BookmarkNode* tier1_a = verifier->AddGroup(m, top, 0, L"tier1_a");
  const BookmarkNode* tier1_b = verifier->AddGroup(m, top, 1, L"tier1_b");
  const BookmarkNode* t1au0 = verifier->AddURL(m, tier1_a, 0, L"tier1_a_url0",
                                               GURL("http://mail.google.com"));
  const BookmarkNode* t1au1 = verifier->AddURL(m, tier1_a, 1, L"tier1_a_url1",
                                               GURL("http://www.pandora.com"));
  const BookmarkNode* t1au2 = verifier->AddURL(
      m, tier1_a, 2, L"tier1_a_url2", GURL("http://www.facebook.com"));
  const BookmarkNode* t1bu0 = verifier->AddURL(m, tier1_b, 0, L"tier1_b_url0",
                                               GURL("http://www.nhl.com"));

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  // SetupSync returns after init, which means got_zero_updates, which means
  // before we merge and associate, so we now have to wait for our above state
  // to round-trip.
  ASSERT_TRUE(client()->AwaitSyncCycleCompletion(
      "Waiting for initial sync completed."));
  verifier->ExpectMatch(m);

  //  Ultimately we want to end up with the following model; but this test is
  //  more about the journey than the destination.
  //
  //  bookmark_bar
  //    -> CNN (www.cnn.com)
  //    -> tier1_a
  //      -> tier1_a_url2 (www.facebook.com)
  //      -> tier1_a_url1 (www.pandora.com)
  //    -> Porsche (www.porsche.com)
  //    -> Bank of America (www.bankofamerica.com)
  //    -> Seattle Bubble
  //  other_node
  //    -> top
  //      -> tier1_b
  //        -> Wired News (www.wired.com)
  //        -> tier2_b
  //          -> tier1_b_url0
  //          -> tier3_b
  //            -> Toronto Maple Leafs (mapleleafs.nhl.com)
  //            -> Wynn (www.wynnlasvegas.com)
  //      -> tier1_a_url0
  const BookmarkNode* bar = m->GetBookmarkBarNode();
  const BookmarkNode* cnn =
      verifier->AddURL(m, bar, 0, L"CNN", GURL("http://www.cnn.com"));
  ASSERT_TRUE(cnn != NULL);
  verifier->Move(m, tier1_a, bar, 1);  // 1 should be the end at this point.
  ASSERT_TRUE(client()->AwaitSyncCycleCompletion("Bookmark moved."));
  verifier->ExpectMatch(m);

  const BookmarkNode* porsche = verifier->AddURL(
      m, bar, 2, L"Porsche", GURL("http://www.porsche.com"));
  // Rearrange stuff in tier1_a.
  EXPECT_EQ(tier1_a, t1au2->GetParent());
  EXPECT_EQ(tier1_a, t1au1->GetParent());
  verifier->Move(m, t1au2, tier1_a, 0);
  verifier->Move(m, t1au1, tier1_a, 2);
  ASSERT_TRUE(client()->AwaitSyncCycleCompletion("Rearrange stuff in tier1_a"));
  verifier->ExpectMatch(m);

  EXPECT_EQ(1, t1au0->GetParent()->IndexOfChild(t1au0));
  verifier->Move(m, t1au0, bar, bar->GetChildCount());
  const BookmarkNode* boa = verifier->AddURL(m, bar, bar->GetChildCount(),
      L"Bank of America", GURL("https://www.bankofamerica.com"));
  ASSERT_TRUE(boa != NULL);
  verifier->Move(m, t1au0, top, top->GetChildCount());
  const BookmarkNode* bubble = verifier->AddURL(m, bar, bar->GetChildCount(),
      L"Seattle Bubble", GURL("http://seattlebubble.com"));
  ASSERT_TRUE(bubble != NULL);
  const BookmarkNode* wired = verifier->AddURL(m, bar, 2, L"Wired News",
                                         GURL("http://www.wired.com"));
  const BookmarkNode* tier2_b = verifier->AddGroup(m, tier1_b, 0, L"tier2_b");
  verifier->Move(m, t1bu0, tier2_b, 0);
  verifier->Move(m, porsche, bar, 0);
  verifier->SetTitle(m, wired, L"News Wired");
  verifier->SetTitle(m, porsche, L"ICanHazPorsche?");
  ASSERT_TRUE(client()->AwaitSyncCycleCompletion("Change title."));
  verifier->ExpectMatch(m);

  EXPECT_EQ(t1au0->id(), top->GetChild(top->GetChildCount() - 1)->id());
  verifier->Remove(m, top, top->GetChildCount() - 1);
  verifier->Move(m, wired, tier1_b, 0);
  verifier->Move(m, porsche, bar, 3);
  const BookmarkNode* tier3_b = verifier->AddGroup(m, tier2_b, 1, L"tier3_b");
  const BookmarkNode* leafs = verifier->AddURL(
      m, tier1_a, 0, L"Toronto Maple Leafs",
      GURL("http://mapleleafs.nhl.com"));
  const BookmarkNode* wynn = verifier->AddURL(
      m, bar, 1, L"Wynn", GURL("http://www.wynnlasvegas.com"));

  verifier->Move(m, wynn, tier3_b, 0);
  verifier->Move(m, leafs, tier3_b, 0);
  ASSERT_TRUE(client()->AwaitSyncCycleCompletion(
      "Move after addition of bookmarks."));
  verifier->ExpectMatch(m);
}

// Connects a client with no bookmarks to a cloud account. As a natural
// consequence of shutdown, this will encode the BookmarkModel as JSON to the
// 'Bookmarks' file.  This is mostly useful to verify server state.
// DISABLED because it should be; we use this as a utility more than a test.
IN_PROC_BROWSER_TEST_F(SingleClientLiveBookmarksSyncTest, DISABLED_GetUpdates) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  BlockUntilLoaded(browser()->profile()->GetBookmarkModel());
  EXPECT_TRUE(client()->ServiceIsPushingChanges());
  ProfileSyncService::Status status(service()->QueryDetailedSyncStatus());
  EXPECT_EQ(status.summary, ProfileSyncService::Status::READY);
  EXPECT_EQ(status.unsynced_count, 0);
}
