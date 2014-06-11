// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "sync/test/fake_server/bookmark_entity_builder.h"
#include "sync/test/fake_server/entity_builder_factory.h"
#include "sync/test/fake_server/fake_server_verifier.h"
#include "ui/base/layout.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::CountBookmarksWithTitlesMatching;
using bookmarks_helper::Create1xFaviconFromPNGFile;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::GetBookmarkModel;
using bookmarks_helper::GetOtherNode;
using bookmarks_helper::ModelMatchesVerifier;
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using bookmarks_helper::RemoveAll;
using bookmarks_helper::SetFavicon;
using bookmarks_helper::SetTitle;
using sync_integration_test_util::AwaitCommitActivityCompletion;

class SingleClientBookmarksSyncTest : public SyncTest {
 public:
  SingleClientBookmarksSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientBookmarksSyncTest() {}

  // Verify that the local bookmark model (for the Profile corresponding to
  // |index|) matches the data on the FakeServer. It is assumed that FakeServer
  // is being used and each bookmark has a unique title. Folders are not
  // verified.
  void VerifyBookmarkModelMatchesFakeServer(int index);

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientBookmarksSyncTest);
};

void SingleClientBookmarksSyncTest::VerifyBookmarkModelMatchesFakeServer(
    int index) {
  fake_server::FakeServerVerifier fake_server_verifier(GetFakeServer());
  std::vector<BookmarkModel::URLAndTitle> local_bookmarks;
  GetBookmarkModel(index)->GetBookmarks(&local_bookmarks);

  // Verify that the number of local bookmarks matches the number in the
  // server.
  ASSERT_TRUE(fake_server_verifier.VerifyEntityCountByType(
      local_bookmarks.size(),
      syncer::BOOKMARKS));

  // Verify that all local bookmark titles exist once on the server.
  std::vector<BookmarkModel::URLAndTitle>::const_iterator it;
  for (it = local_bookmarks.begin(); it != local_bookmarks.end(); ++it) {
    ASSERT_TRUE(fake_server_verifier.VerifyEntityCountByTypeAndName(
        1,
        syncer::BOOKMARKS,
        base::UTF16ToUTF8(it->title)));
  }
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> top
  //      -> tier1_a
  //        -> http://mail.google.com  "tier1_a_url0"
  //        -> http://www.pandora.com  "tier1_a_url1"
  //        -> http://www.facebook.com "tier1_a_url2"
  //      -> tier1_b
  //        -> http://www.nhl.com "tier1_b_url0"
  const BookmarkNode* top = AddFolder(0, GetOtherNode(0), 0, "top");
  const BookmarkNode* tier1_a = AddFolder(0, top, 0, "tier1_a");
  const BookmarkNode* tier1_b = AddFolder(0, top, 1, "tier1_b");
  const BookmarkNode* tier1_a_url0 = AddURL(
      0, tier1_a, 0, "tier1_a_url0", GURL("http://mail.google.com"));
  const BookmarkNode* tier1_a_url1 = AddURL(
      0, tier1_a, 1, "tier1_a_url1", GURL("http://www.pandora.com"));
  const BookmarkNode* tier1_a_url2 = AddURL(
      0, tier1_a, 2, "tier1_a_url2", GURL("http://www.facebook.com"));
  const BookmarkNode* tier1_b_url0 = AddURL(
      0, tier1_b, 0, "tier1_b_url0", GURL("http://www.nhl.com"));

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

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
  const BookmarkNode* cnn = AddURL(0, bar, 0, "CNN",
      GURL("http://www.cnn.com"));
  ASSERT_TRUE(cnn != NULL);
  Move(0, tier1_a, bar, 1);

  // Wait for the bookmark position change to sync.
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  const BookmarkNode* porsche = AddURL(0, bar, 2, "Porsche",
      GURL("http://www.porsche.com"));
  // Rearrange stuff in tier1_a.
  ASSERT_EQ(tier1_a, tier1_a_url2->parent());
  ASSERT_EQ(tier1_a, tier1_a_url1->parent());
  Move(0, tier1_a_url2, tier1_a, 0);
  Move(0, tier1_a_url1, tier1_a, 2);

  // Wait for the rearranged hierarchy to sync.
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  ASSERT_EQ(1, tier1_a_url0->parent()->GetIndexOf(tier1_a_url0));
  Move(0, tier1_a_url0, bar, bar->child_count());
  const BookmarkNode* boa = AddURL(0, bar, bar->child_count(),
      "Bank of America", GURL("https://www.bankofamerica.com"));
  ASSERT_TRUE(boa != NULL);
  Move(0, tier1_a_url0, top, top->child_count());
  const BookmarkNode* bubble = AddURL(
      0, bar, bar->child_count(), "Seattle Bubble",
          GURL("http://seattlebubble.com"));
  ASSERT_TRUE(bubble != NULL);
  const BookmarkNode* wired = AddURL(0, bar, 2, "Wired News",
      GURL("http://www.wired.com"));
  const BookmarkNode* tier2_b = AddFolder(
      0, tier1_b, 0, "tier2_b");
  Move(0, tier1_b_url0, tier2_b, 0);
  Move(0, porsche, bar, 0);
  SetTitle(0, wired, "News Wired");
  SetTitle(0, porsche, "ICanHazPorsche?");

  // Wait for the title change to sync.
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  ASSERT_EQ(tier1_a_url0->id(), top->GetChild(top->child_count() - 1)->id());
  Remove(0, top, top->child_count() - 1);
  Move(0, wired, tier1_b, 0);
  Move(0, porsche, bar, 3);
  const BookmarkNode* tier3_b = AddFolder(0, tier2_b, 1, "tier3_b");
  const BookmarkNode* leafs = AddURL(
      0, tier1_a, 0, "Toronto Maple Leafs", GURL("http://mapleleafs.nhl.com"));
  const BookmarkNode* wynn = AddURL(0, bar, 1, "Wynn",
      GURL("http://www.wynnlasvegas.com"));

  Move(0, wynn, tier3_b, 0);
  Move(0, leafs, tier3_b, 0);

  // Wait for newly added bookmarks to sync.
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Only verify FakeServer data if FakeServer is being used.
  // TODO(pvalenzuela): Use this style of verification in more tests once it is
  // proven stable.
  if (GetFakeServer())
    VerifyBookmarkModelMatchesFakeServer(0);
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, InjectedBookmark) {
  std::string title = "Montreal Canadiens";
  fake_server::EntityBuilderFactory entity_builder_factory;
  scoped_ptr<fake_server::FakeServerEntity> entity =
      entity_builder_factory.NewBookmarkEntityBuilder(
          title, GURL("http://canadiens.nhl.com")).Build();
  fake_server_->InjectEntity(entity.Pass());

  DisableVerifier();
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(1, CountBookmarksWithTitlesMatching(0, title));
}

// Test that a client doesn't mutate the favicon data in the process
// of storing the favicon data from sync to the database or in the process
// of requesting data from the database for sync.
IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       SetFaviconHiDPIDifferentCodec) {
  // Set the supported scale factors to 1x and 2x such that
  // BookmarkModel::GetFavicon() requests both 1x and 2x.
  // 1x -> for sync, 2x -> for the UI.
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::SetSupportedScaleFactors(supported_scale_factors);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ModelMatchesVerifier(0));

  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon.ico");
  const BookmarkNode* bookmark = AddURL(0, "title", page_url);

  // Simulate receiving a favicon from sync encoded by a different PNG encoder
  // than the one native to the OS. This tests the PNG data is not decoded to
  // SkBitmap (or any other image format) then encoded back to PNG on the path
  // between sync and the database.
  gfx::Image original_favicon = Create1xFaviconFromPNGFile(
      "favicon_cocoa_png_codec.png");
  ASSERT_FALSE(original_favicon.IsEmpty());
  SetFavicon(0, bookmark, icon_url, original_favicon,
             bookmarks_helper::FROM_SYNC);

  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  scoped_refptr<base::RefCountedMemory> original_favicon_bytes =
      original_favicon.As1xPNGBytes();
  gfx::Image final_favicon = GetBookmarkModel(0)->GetFavicon(bookmark);
  scoped_refptr<base::RefCountedMemory> final_favicon_bytes =
      final_favicon.As1xPNGBytes();

  // Check that the data was not mutated from the original.
  EXPECT_TRUE(original_favicon_bytes.get());
  EXPECT_TRUE(original_favicon_bytes->Equals(final_favicon_bytes));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       BookmarkAllNodesRemovedEvent) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Starting state:
  // other_node
  //    -> folder0
  //      -> tier1_a
  //        -> http://mail.google.com
  //        -> http://www.google.com
  //      -> http://news.google.com
  //      -> http://yahoo.com
  //    -> http://www.cnn.com
  // bookmark_bar
  // -> empty_folder
  // -> folder1
  //    -> http://yahoo.com
  // -> http://gmail.com

  const BookmarkNode* folder0 = AddFolder(0, GetOtherNode(0), 0, "folder0");
  const BookmarkNode* tier1_a = AddFolder(0, folder0, 0, "tier1_a");
  ASSERT_TRUE(AddURL(0, folder0, 1, "News", GURL("http://news.google.com")));
  ASSERT_TRUE(AddURL(0, folder0, 2, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AddURL(0, tier1_a, 0, "Gmai", GURL("http://mail.google.com")));
  ASSERT_TRUE(AddURL(0, tier1_a, 1, "Google", GURL("http://www.google.com")));
  ASSERT_TRUE(
      AddURL(0, GetOtherNode(0), 1, "CNN", GURL("http://www.cnn.com")));

  ASSERT_TRUE(AddFolder(0, GetBookmarkBarNode(0), 0, "empty_folder"));
  const BookmarkNode* folder1 =
      AddFolder(0, GetBookmarkBarNode(0), 1, "folder1");
  ASSERT_TRUE(AddURL(0, folder1, 0, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(
      AddURL(0, GetBookmarkBarNode(0), 2, "Gmai", GURL("http://gmail.com")));

  // Set up sync, wait for its completion and verify that changes propagated.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Remove all bookmarks and wait for sync completion.
  RemoveAll(0);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  // Verify other node has no children now.
  EXPECT_EQ(0, GetOtherNode(0)->child_count());
  EXPECT_EQ(0, GetBookmarkBarNode(0)->child_count());
  // Verify model matches verifier.
  ASSERT_TRUE(ModelMatchesVerifier(0));
}
