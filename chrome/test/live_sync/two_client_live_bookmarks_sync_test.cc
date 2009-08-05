// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include <stdlib.h>

#include "base/string16.h"
#include "base/rand_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/test/live_sync/bookmark_model_verifier.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "chrome/test/live_sync/live_bookmarks_sync_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class TwoClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  TwoClientLiveBookmarksSyncTest() {
    // Set the initial timeout value to 5 min.
    InProcessBrowserTest::SetInitialTimeoutInMS(300000);
  }
  virtual ~TwoClientLiveBookmarksSyncTest() {}
  bool SetupSync() {
    profile2_.reset(MakeProfile(L"clienttwo"));
    client1_.reset(new ProfileSyncServiceTestHarness(
        browser()->profile(), username_, password_));
    client2_.reset(new ProfileSyncServiceTestHarness(
        profile2_.get(), username_, password_));
    return client1_->SetupSync() && client2_->SetupSync();
  }

  ProfileSyncServiceTestHarness* client1() { return client1_.get(); }
  ProfileSyncServiceTestHarness* client2() { return client2_.get(); }
  Profile* profile1() { return browser()->profile(); }
  Profile* profile2() { return profile2_.get(); }

  void Cleanup() {
    client2_.reset();
    profile2_.reset();
  }

 private:
  scoped_ptr<Profile> profile2_;
  scoped_ptr<ProfileSyncServiceTestHarness> client1_;
  scoped_ptr<ProfileSyncServiceTestHarness> client2_;

  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveBookmarksSyncTest);
};

// Test case Naming Convention:
// SC/MC - SingleClient / MultiClient.
// Suffix Number - Indicates test scribe testcase ID.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* bbn_two = model_two->GetBookmarkBarNode();
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  GURL google_url("http://www.google.com");
  // Start adding some bookmarks to each model.  The scope is to enforce that
  // the BookmarkNode*'s are not used after they may be invalidated by sync
  // operations that alter the models.
  {
    const BookmarkNode* google_one = verifier->AddURL(model_one, bbn_one, 0,
        L"Google", google_url);

    // To make this test deterministic, we wait here so there is no race to
    // decide which bookmark actually gets position 0.
    ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
    const BookmarkNode* yahoo_two = verifier->AddURL(model_two, bbn_two, 0,
        L"Yahoo", GURL("http://www.yahoo.com"));
  }
  ASSERT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    const BookmarkNode* new_folder_one =
        verifier->AddGroup(model_one, bbn_one, 2, L"New Folder");
    verifier->Move(model_one, GetByUniqueURL(model_one, google_url),
        new_folder_one, 0);
    verifier->SetTitle(model_one, bbn_one->GetChild(0), L"Yahoo!!");
    const BookmarkNode* cnn_one = verifier->AddURL(model_one,
        bbn_one, 1, L"CNN", GURL("http://www.cnn.com"));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    const BookmarkNode* facebook_two = verifier->AddURL(model_two,
        bbn_two, 0, L"Facebook", GURL("http://www.facebook.com"));
  }

  // AwaitMutualSyncCycleCompletion blocks the calling object before the
  // argument, so because we have made changes from client2 here we need to swap
  // the calling order.
  ASSERT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  verifier->SortChildren(model_two, bbn_two);

  ASSERT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Do something crazy and modify the same item from both clients!!
    const BookmarkNode* google_one = GetByUniqueURL(model_one, google_url);
    const BookmarkNode* google_two = GetByUniqueURL(model_two, google_url);
    model_one->SetTitle(google_one, L"Google++");
    model_two->SetTitle(google_two, L"Google--");
  }
  // The extra wait here is because both clients generated changes, and the
  // first client reaches a happy state before the second client gets a chance
  // to push, so we explicitly double check.  This shouldn't be necessary once
  // we have an easy way to verify the head version on each client.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletionWithConflict(client2()));
  BookmarkModelVerifier::ExpectModelsMatch(model_one, model_two);

  Cleanup();
}

// TODO(timsteele): There are really two tests here, one case where conflict
// resolution causes the URL to be overwritten, and one where we see duplicate
// bookmarks created due to the Remove/Add semantic for "Edit URL" and the race
// between the local edit and server edit.  The latter is bug 1956259 and is
// still under investigation, I don't have enough info to write two separate
// tests yet.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SimultaneousURLChanges) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* bbn_two = model_two->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  GURL initial_url(L"http://www.google.com");
  GURL second_url(L"http://www.google.com/abc");
  GURL third_url(L"http://www.google.com/def");
  std::wstring title = L"Google";
  {
    const BookmarkNode* google = verifier->AddURL(model_one, bbn_one, 0,
        title, initial_url);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  {
    const BookmarkNode* google_one = GetByUniqueURL(model_one, initial_url);
    const BookmarkNode* google_two = GetByUniqueURL(model_two, initial_url);
    bookmark_utils::ApplyEditsWithNoGroupChange(model_one, bbn_one, google_one,
                                                title, second_url, NULL);
    bookmark_utils::ApplyEditsWithNoGroupChange(model_two, bbn_two, google_two,
                                                title, third_url, NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletionWithConflict(client2()));
  BookmarkModelVerifier::ExpectModelsMatch(model_one, model_two);

  {
    const BookmarkNode* google_one = bbn_one->GetChild(0);
    model_one->SetTitle(google_one, L"Google1");
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  BookmarkModelVerifier::ExpectModelsMatch(model_one, model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_CleanAccount_AddFirstFolder_370558) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Let's add first bookmark folder to client1
    const BookmarkNode* new_folder_one =
        verifier->AddGroup(model_one, bbn_one, 0, L"TestFolder");
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_CleanAccount_AddFirstBMWithoutFavicon_370559) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark(without favicon)
  {
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one, 0,
        L"TestBookmark", GURL("http://www.nofaviconurl.com"));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_CleanAccount_AddNonHTTPBMs_370560) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add few non-http bookmarks(without favicon)
  {
    const BookmarkNode* ftp_bm = verifier->AddURL(model_one, bbn_one, 0,
        L"FTPBookmark", GURL("ftp://ftp.testbookmark.com"));
    const BookmarkNode* file_bm = verifier->AddURL(model_one, bbn_one, 0,
        L"FileBookmark", GURL("file:///"));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_CleanAccount_AddFirstBM_UnderFolder_370561) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Let's add first bookmark folder to client1
    const BookmarkNode* new_folder_one =
        verifier->AddGroup(model_one, bbn_one, 0, L"BM TestFolder");
    // Add first bookmark to newly created folder
    const BookmarkNode* test_bm1 = verifier->AddURL(
        model_one, new_folder_one, 0,
        L"BM Test", GURL("http://www.bmtest.com"));
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_SingleClient_RenameBMName_371817) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
    // Add first bookmark
  const BookmarkNode* test_bm1 = verifier->AddURL(
      model_one, bbn_one, 0, L"Test BM", GURL("http://www.bmtest.com"));

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Rename recently added BM
    verifier->SetTitle(model_one, test_bm1, L"New Test BM");
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMFolder_371824) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark folder to client1
  const BookmarkNode* new_folder_one = verifier->AddGroup(model_one, bbn_one, 0,
      L"TestBMFolder");

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Rename recently added Bookmark folder
  verifier->SetTitle(model_one, new_folder_one, L"New TestBMFolder");

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteBM_EmptyAccountAfterThisDelete_371832) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark(without favicon)
  {
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one, 0,
        L"TestBookmark", GURL("http://www.nofaviconurl.com"));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Delete this newly created bookmark
    verifier->Remove(model_one, bbn_one, 0);
  }
  client1()->AwaitMutualSyncCycleCompletionWithConflict(client2());
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteBM_NonEmptyAccountAfterThisDelete_371833) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add some bookmarks(without favicon)
  for (int index = 0; index < 20; index++) {
    string16 title(L"TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(
        model_one, bbn_one, index,
        title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Delete this newly created bookmark
    verifier->Remove(model_one, bbn_one, 0);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RepositioningBM_ab_To_ba_371931) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  const BookmarkNode* bm_a = verifier->AddURL(
      model_one, bbn_one, 0, L"Bookmark A",
      GURL("http://www.nofaviconurla.com"));
  const BookmarkNode* bm_b = verifier->AddURL(
      model_one, bbn_one, 1, L"Bookmark B",
      GURL("http://www.nofaviconurlb.com"));

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Move bm_a to new position
    verifier->Move(model_one, bm_a, bbn_one, 2);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_Repositioning_NonEmptyBMFolder_ab_To_ba_372026) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  const BookmarkNode* bm_folder_a =
    verifier->AddGroup(model_one, bbn_one, 0, L"TestBMFolderA");
  const BookmarkNode* bm_folder_b =
    verifier->AddGroup(model_one, bbn_one, 1, L"TestBMFolderB");
  for (int index = 0; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 60% of time add bookmarks
    if (random_int > 40) {
        string16 title(L"Folder A - ChildTestBookmark");
        string16 url(L"http://www.nofaviconurl");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        url.append(index_str);
        url.append(L".com");
        const BookmarkNode* nofavicon_bm =
            verifier->AddURL(model_one, bm_folder_a, index, title, GURL(url));
    } else {
        // Remaining % of time - Add Bookmark folders
        string16 title(L"Folder A - ChildTestBMFolder");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        const BookmarkNode* bm_folder =
            verifier->AddGroup(model_one, bm_folder_a, index, title);
    }
  }

  for (int index = 0; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 60% of time add bookmarks
    if (random_int > 40) {
        string16 title(L"Folder B - ChildTestBookmark");
        string16 url(L"http://www.nofaviconurl");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        url.append(index_str);
        url.append(L".com");
        const BookmarkNode* nofavicon_bm =
            verifier->AddURL(model_one, bm_folder_b, index, title, GURL(url));
    } else {
        // Remaining % of time - Add Bookmark folders
        string16 title(L"Folder B - ChildTestBMFolder");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        const BookmarkNode* bm_folder =
            verifier->AddGroup(model_one, bm_folder_b, index, title);
    }
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  {
    // Move bm_a to new position
    verifier->Move(model_one, bm_folder_a, bbn_one, 2);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_CleanAccount_AddSeveralBMs_UnderBMBarAndOtherBM_370562) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add some bookmarks(without favicon)
  for (int index = 0; index < 20; index++) {
    string16 title(L"TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bbn_one, index, title, GURL(url));
  }
  for (int index = 0; index < 10; index++) {
    string16 title(L"TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, other_bm_one, index, title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_CleanAccount_AddSeveralBMs_And_SeveralFolders_370563) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add some bookmarks(without favicon)
  for (int index = 0; index < 15; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
        string16 title(L"BB - TestBookmark");
        string16 url(L"http://www.nofaviconurl");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        url.append(index_str);
        url.append(L".com");
        const BookmarkNode* nofavicon_bm =
            verifier->AddURL(model_one, bbn_one, index, title, GURL(url));
    } else {
        // Remaining % of time - Add Bookmark folders
        string16 title(L"BB - TestBMFolder");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        const BookmarkNode* bm_folder = verifier->AddGroup(model_one, bbn_one,
            index, title);
        int random_int2 = base::RandInt(1, 100);
        // 60% of time we will add bookmarks to added folder
        if (random_int2 > 40) {
            for (int index = 0; index < 20; index++) {
              string16 url(L"http://www.nofaviconurl");
              string16 index_str = IntToString16(index);
              string16 child_title(title);
              child_title.append(L" - ChildTestBM");
              child_title.append(index_str);
              url.append(index_str);
              url.append(L".com");
              const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
                    bm_folder, index, child_title, GURL(url));
            }
        }
    }
  }
  LOG(INFO) << "Adding several bookmarks under other bookmarks";
  for (int index = 0; index < 10; index++) {
    string16 title(L"Other - TestBookmark");
    string16 url(L"http://www.nofaviconurl-other");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, other_bm_one, index, title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMURL_371822) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add first bookmark(without favicon)
  const BookmarkNode* nofavicon_bm =
      verifier->AddURL(model_one, bbn_one, 0, L"Google",
          GURL("http://www.google.com"));

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's rename/change URL
  verifier->SetURL(model_one, nofavicon_bm, GURL("http://www.cnn.com"));
  // Wait for changes to sync and then verify
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameEmptyBMFolder_371825) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 0, L"TestFolder");

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's rename newly added bookmark folder
  verifier->SetTitle(model_one, bm_folder_one, L"New TestFolder");
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteFirstBM_Under_BMFolder_NonEmptyFolderAfterDelete_371835) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 0, L"TestFolder");
  // Let's add some bookmarks(without favicon) to this folder
  for (int index = 0; index < 10; index++) {
    string16 title(L"TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bm_folder_one, index, title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Delete first bookmark under this folder
    verifier->Remove(model_one, bm_folder_one, 0);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteLastBM_Under_BMFolder_NonEmptyFolderAfterDelete_371836) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one = verifier->AddGroup(model_one, bbn_one,
      0, L"TestFolder");
  // Let's add some bookmarks(without favicon) to this folder
  for (int index = 0; index < 10; index++) {
    string16 title(L"TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(
        model_one, bm_folder_one,
        index, title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Delete last bookmark under this folder
    verifier->Remove(model_one, bm_folder_one,
        (bm_folder_one->GetChildCount() - 1));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelMiddleBM_Under_BMFold_NonEmptyFoldAfterDel_371856) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
    verifier->AddGroup(model_one, bbn_one, 0, L"TestFolder");
  // Let's add some bookmarks(without favicon) to this folder
  for (int index = 0; index < 10; index++) {
    string16 title(L"TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bm_folder_one, index, title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    // Delete middle bookmark under this folder
    verifier->Remove(model_one, bm_folder_one, 4);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteBMs_Under_BMFolder_EmptyFolderAfterDelete_371857) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
    verifier->AddGroup(model_one, bbn_one, 0, L"TestFolder");
  // Let's add some bookmarks(without favicon) to this folder
  for (int index = 0; index < 10; index++) {
    string16 title(L"TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bm_folder_one, index, title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  int child_count = bm_folder_one->GetChildCount();
  // Let's delete all the bookmarks added under this new folder
  for (int index = 0; index < child_count; index++) {
    verifier->Remove(model_one, bm_folder_one, 0);
    ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
    verifier->ExpectMatch(model_one);
    verifier->ExpectMatch(model_two);
  }
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteEmptyBMFolder_EmptyAccountAfterDelete_371858) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
    verifier->AddGroup(model_one, bbn_one, 0, L"TestFolder");

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's delete this empty bookmark folder
  verifier->Remove(model_one, bbn_one, 0);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteEmptyBMFolder_NonEmptyAccountAfterDelete_371869) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 0, L"TestFolder");
  // Let's add some bookmarks(without favicon)
  for (int index = 1; index < 15; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
        string16 title(L"BB - TestBookmark");
        string16 url(L"http://www.nofaviconurl");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        url.append(index_str);
        url.append(L".com");
        const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one,
            index, title, GURL(url));
    } else {
        // Remaining % of time - Add Bookmark folders
        string16 title(L"BB - TestBMFolder");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        const BookmarkNode* bm_folder = verifier->AddGroup(model_one, bbn_one,
            index, title);
     }
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's delete the first empty bookmark folder
  verifier->Remove(model_one, bbn_one, 0);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMFoldWithBMsAndBMFolds_NonEmptyACAfterDelete_371880) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = verifier->AddURL(model_one, bbn_one, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 1, L"TestFolder");
  // Let's add some bookmarks(without favicon) and folders to
  // bookmark bar
  for (int index = 2; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
        string16 title(L"BB - TestBookmark");
        string16 url(L"http://www.nofaviconurl");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        url.append(index_str);
        url.append(L".com");
        const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one,
            index, title, GURL(url));
    } else {
        // Remaining % of time - Add Bookmark folders
        string16 title(L"BB - TestBMFolder");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        const BookmarkNode* bm_folder = verifier->AddGroup(model_one, bbn_one,
            index, title);
     }
  }

  // Let's add some bookmarks(without favicon) and folders to
  // bm_folder_one ('TestFolder')
  for (int index = 0; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
      string16 title(L"Level2 - TestBookmark");
      string16 url(L"http://www.nofaviconurl");
      string16 index_str = IntToString16(index);
      title.append(index_str);
      url.append(index_str);
      url.append(L".com");
      const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
          bm_folder_one, index, title, GURL(url));
    } else {
        // Remaining % of time - Add Bookmark folders
        string16 title(L"Level2 - TestBMFolder");
        string16 index_str = IntToString16(index);
        title.append(index_str);
        const BookmarkNode* l2_bm_folder = verifier->AddGroup(model_one,
            bm_folder_one, index, title);
        int random_int2 = base::RandInt(1, 100);
        // 70% of time - Let's add more levels of bookmarks and folders to
        // l2_bm_folder
        if (random_int2 > 30) {
          for (int index2 = 0; index2 < 10; index2++) {
            int random_int3 = base::RandInt(1, 100);
            // To create randomness in order, 40% of time add bookmarks
            if (random_int3 > 60) {
              string16 title(L"Level3 - TestBookmark");
              string16 url(L"http://www.nofaviconurl");
              string16 index_str = IntToString16(index2);
              title.append(index_str);
              url.append(index_str);
              url.append(L".com");
              const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
                  l2_bm_folder, index2, title, GURL(url));
            } else {
                // Remaining % of time - Add Bookmark folders
                string16 title(L"Level3 - TestBMFolder");
                string16 index_str = IntToString16(index);
                title.append(index_str);
                const BookmarkNode* l3_bm_folder =
                    verifier->AddGroup(model_one, l2_bm_folder, index2, title);
             }
          }  // end inner for loop
        }
     }
    }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's delete the bookmark folder (bm_folder_one)
  verifier->Remove(model_one, bm_folder_one, 0);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_Moving_BMsFromBookmarkBar_To_BMFolder_371954) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = verifier->AddURL(model_one, bbn_one, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 1, L"TestFolder");
  // Let's add some bookmarks(without favicon) to bookmark bar
  for (int index = 2; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    string16 title(L"BB - TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one,
        index, title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move bookmarks from bookmark bar to BMfolder (bm_folder_one)
  int child_count_to_move = bbn_one->GetChildCount() - 2;
  for (int index = 0; index < child_count_to_move; index++) {
    verifier->Move(model_one, bbn_one->GetChild(2),
        bm_folder_one, index);
    ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
    verifier->ExpectMatch(model_one);
    verifier->ExpectMatch(model_two);
  }
  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_Moving_BMsFromBMFolder_To_BookmarkBar_371957) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = verifier->AddURL(model_one, bbn_one, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 1, L"TestFolder");
  // Let's add some bookmarks(without favicon) to bm_folder_one
  for (int index = 0; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    string16 title(L"BB - TestBookmark");
    string16 url(L"http://www.nofaviconurl");
    string16 index_str = IntToString16(index);
    title.append(index_str);
    url.append(index_str);
    url.append(L".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
        bm_folder_one, index, title, GURL(url));
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move bookmarks from BMfolder(bm_folder_one) to bookmark bar
  int child_count_to_move = bm_folder_one->GetChildCount();
  for (int index = 0; index < child_count_to_move; index++) {
    verifier->Move(model_one, bm_folder_one->GetChild(0),
        bbn_one, bbn_one->GetChildCount());
    ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
    verifier->ExpectMatch(model_one);
    verifier->ExpectMatch(model_two);
  }
  Cleanup();
}

#endif  // CHROME_PERSONALIZATION
