// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

IN_PROC_BROWSER_TEST_F(SingleClientLiveBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  BookmarkModel* bm = GetBookmarkModel(0);
  BookmarkModelVerifier* v = verifier_helper();

  // Starting state:
  // other_node
  //    -> top
  //      -> tier1_a
  //        -> http://mail.google.com  "tier1_a_url0"
  //        -> http://www.pandora.com  "tier1_a_url1"
  //        -> http://www.facebook.com "tier1_a_url2"
  //      -> tier1_b
  //        -> http://www.nhl.com "tier1_b_url0"
  const BookmarkNode* top = v->AddGroup(bm, bm->other_node(), 0, L"top");
  const BookmarkNode* tier1_a = v->AddGroup(bm, top, 0, L"tier1_a");
  const BookmarkNode* tier1_b = v->AddGroup(bm, top, 1, L"tier1_b");
  const BookmarkNode* t1au0 = v->AddURL(bm, tier1_a, 0, L"tier1_a_url0",
      GURL("http://mail.google.com"));
  const BookmarkNode* t1au1 = v->AddURL(bm, tier1_a, 1, L"tier1_a_url1",
      GURL("http://www.pandora.com"));
  const BookmarkNode* t1au2 = v->AddURL(
      bm, tier1_a, 2, L"tier1_a_url2", GURL("http://www.facebook.com"));
  const BookmarkNode* t1bu0 = v->AddURL(bm, tier1_b, 0, L"tier1_b_url0",
      GURL("http://www.nhl.com"));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for initial sync completed."));
  v->ExpectMatch(bm);

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
  const BookmarkNode* bar = GetBookmarkBarNode(0);
  const BookmarkNode* cnn =
      v->AddURL(bm, bar, 0, L"CNN", GURL("http://www.cnn.com"));
  ASSERT_TRUE(cnn != NULL);
  v->Move(bm, tier1_a, bar, 1);  // 1 should be the end at this point.
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Bookmark moved."));
  v->ExpectMatch(bm);

  const BookmarkNode* porsche = v->AddURL(
      bm, bar, 2, L"Porsche", GURL("http://www.porsche.com"));
  // Rearrange stuff in tier1_a.
  EXPECT_EQ(tier1_a, t1au2->GetParent());
  EXPECT_EQ(tier1_a, t1au1->GetParent());
  v->Move(bm, t1au2, tier1_a, 0);
  v->Move(bm, t1au1, tier1_a, 2);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Rearrange stuff in tier1_a"));
  v->ExpectMatch(bm);

  EXPECT_EQ(1, t1au0->GetParent()->IndexOfChild(t1au0));
  v->Move(bm, t1au0, bar, bar->GetChildCount());
  const BookmarkNode* boa = v->AddURL(bm, bar, bar->GetChildCount(),
      L"Bank of America", GURL("https://www.bankofamerica.com"));
  ASSERT_TRUE(boa != NULL);
  v->Move(bm, t1au0, top, top->GetChildCount());
  const BookmarkNode* bubble = v->AddURL(bm, bar, bar->GetChildCount(),
      L"Seattle Bubble", GURL("http://seattlebubble.com"));
  ASSERT_TRUE(bubble != NULL);
  const BookmarkNode* wired = v->AddURL(bm, bar, 2, L"Wired News",
                                         GURL("http://www.wired.com"));
  const BookmarkNode* tier2_b = v->AddGroup(bm, tier1_b, 0, L"tier2_b");
  v->Move(bm, t1bu0, tier2_b, 0);
  v->Move(bm, porsche, bar, 0);
  v->SetTitle(bm, wired, L"News Wired");
  v->SetTitle(bm, porsche, L"ICanHazPorsche?");
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Change title."));
  v->ExpectMatch(bm);

  EXPECT_EQ(t1au0->id(), top->GetChild(top->GetChildCount() - 1)->id());
  v->Remove(bm, top, top->GetChildCount() - 1);
  v->Move(bm, wired, tier1_b, 0);
  v->Move(bm, porsche, bar, 3);
  const BookmarkNode* tier3_b = v->AddGroup(bm, tier2_b, 1, L"tier3_b");
  const BookmarkNode* leafs = v->AddURL(
      bm, tier1_a, 0, L"Toronto Maple Leafs",
      GURL("http://mapleleafs.nhl.com"));
  const BookmarkNode* wynn = v->AddURL(
      bm, bar, 1, L"Wynn", GURL("http://www.wynnlasvegas.com"));

  v->Move(bm, wynn, tier3_b, 0);
  v->Move(bm, leafs, tier3_b, 0);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Move after addition of bookmarks."));
  v->ExpectMatch(bm);
}

// Connects a client with no bookmarks to a cloud account. As a natural
// consequence of shutdown, this will encode the BookmarkModel as JSON to the
// 'Bookmarks' file.  This is mostly useful to verify server state.
// DISABLED because it should be; we use this as a utility more than a test.
IN_PROC_BROWSER_TEST_F(SingleClientLiveBookmarksSyncTest, DISABLED_GetUpdates) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(GetClient(0)->ServiceIsPushingChanges());
  ProfileSyncService::Status status(GetService(0)->QueryDetailedSyncStatus());
  EXPECT_EQ(status.summary, ProfileSyncService::Status::READY);
  EXPECT_EQ(status.unsynced_count, 0);
}
