// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/test/live_sync/live_bookmarks_sync_test.h"

using std::string;
using std::wstring;

// Some Abbreviations Used:
// F -- BookmarkFolder
// BM -- Bookmark
// L -- Level (Depth of bookmark folder)

// Test case Naming Convention:
// SC/MC - SingleClient / MultiClient.
// Suffix Number - Indicates test scribe testcase ID.

// TODO(brettw) this file should be converted to string16 and use
// IntToString16 instead.
static std::wstring IntToWStringHack(int val) {
  return UTF8ToWide(base::IntToString(val));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = GetBookmarkBarNode(0);
  const BookmarkNode* bm_bar1 = GetBookmarkBarNode(1);

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  GURL google_url("http://www.google.com");
  // Start adding some bookmarks to each model.  The scope is to enforce that
  // the BookmarkNode*'s are not used after they may be invalidated by sync
  // operations that alter the models.
  {
    const BookmarkNode* google_one = v->AddURL(bm0, bm_bar0, 0,
        L"Google", google_url);
    ASSERT_TRUE(google_one != NULL);

    // To make this test deterministic, we wait here so there is no race to
    // decide which bookmark actually gets position 0.
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    const BookmarkNode* yahoo_two = v->AddURL(bm1, bm_bar1, 0,
        L"Yahoo", GURL("http://www.yahoo.com"));
    ASSERT_TRUE(yahoo_two != NULL);
  }
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    const BookmarkNode* new_folder_one =
        v->AddGroup(bm0, bm_bar0, 2, L"New Folder");
    v->Move(bm0, GetByUniqueURL(bm0, google_url),
        new_folder_one, 0);
    v->SetTitle(bm0, bm_bar0->GetChild(0), L"Yahoo!!");
    const BookmarkNode* cnn_one = v->AddURL(bm0,
        bm_bar0, 1, L"CNN", GURL("http://www.cnn.com"));
    ASSERT_TRUE(cnn_one != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    const BookmarkNode* facebook_two = v->AddURL(bm1,
        bm_bar1, 0, L"Facebook", GURL("http://www.facebook.com"));
    ASSERT_TRUE(facebook_two != NULL);
  }

  // AwaitMutualSyncCycleCompletion blocks the calling object before the
  // argument, so because we have made changes from client2 here we need to swap
  // the calling order.
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  v->SortChildren(bm1, bm_bar1);

  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Do something crazy and modify the same item from both clients!!
    const BookmarkNode* google_one = GetByUniqueURL(bm0, google_url);
    const BookmarkNode* google_two = GetByUniqueURL(bm1, google_url);
    bm0->SetTitle(google_one, ASCIIToUTF16("Google++"));
    bm1->SetTitle(google_two, ASCIIToUTF16("Google--"));
  }

  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);
}

// TODO(timsteele): There are really two tests here, one case where conflict
// resolution causes the URL to be overwritten, and one where we see duplicate
// bookmarks created due to the Remove/Add semantic for "Edit URL" and the race
// between the local edit and server edit.  The latter is bug 1956259 and is
// still under investigation, I don't have enough info to write two separate
// tests yet.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SimultaneousURLChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_bar1 = bm1->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  GURL initial_url("http://www.google.com");
  GURL second_url("http://www.google.com/abc");
  GURL third_url("http://www.google.com/def");
  string16 title = ASCIIToUTF16("Google");
  {
    const BookmarkNode* google =
        v->AddURL(bm0, bm_bar0, 0, UTF16ToWideHack(title), initial_url);
    ASSERT_TRUE(google != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  {
    const BookmarkNode* google_one = GetByUniqueURL(bm0, initial_url);
    const BookmarkNode* google_two = GetByUniqueURL(bm1, initial_url);
    bookmark_utils::ApplyEditsWithNoGroupChange(bm0, bm_bar0,
        BookmarkEditor::EditDetails(google_one), title, second_url);
    bookmark_utils::ApplyEditsWithNoGroupChange(bm1, bm_bar1,
        BookmarkEditor::EditDetails(google_two), title, third_url);
  }

  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);

  {
    const BookmarkNode* google_one = bm_bar0->GetChild(0);
    bm0->SetTitle(google_one, ASCIIToUTF16("Google1"));
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);
}

// Test Scribe ID - 370558.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddFirstFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Let's add first bookmark folder to client0
    const BookmarkNode* new_folder_one =
        v->AddGroup(bm0, bm_bar0, 0, L"TestFolder");
    ASSERT_TRUE(new_folder_one != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 370559.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddFirstBMWithoutFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add first bookmark(without favicon)
  {
    const BookmarkNode* nofavicon_bm = v->AddURL(bm0, bm_bar0, 0,
        L"TestBookmark", GURL("http://www.nofaviconurl.com"));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 370560.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddNonHTTPBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add few non-http GetBookmarkModel(without favicon)
  {
    const BookmarkNode* ftp_bm = v->AddURL(bm0, bm_bar0, 0,
        L"FTPBookmark", GURL("ftp://ftp.testbookmark.com"));
    ASSERT_TRUE(ftp_bm != NULL);
    const BookmarkNode* file_bm = v->AddURL(bm0, bm_bar0, 1,
        L"FileBookmark", GURL("file:///"));
    ASSERT_TRUE(file_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 370561.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddFirstBMUnderFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Let's add first bookmark folder to client1
    const BookmarkNode* new_folder_one =
        v->AddGroup(bm0, bm_bar0, 0, L"BM TestFolder");
    // Add first bookmark to newly created folder
    const BookmarkNode* test_bm1 = v->AddURL(
        bm0, new_folder_one, 0,
        L"BM Test", GURL("http://www.bmtest.com"));
    ASSERT_TRUE(test_bm1 != NULL);
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 370562.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddSeveralBMsUnderBMBarAndOtherBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_other0 = bm0->other_node();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add some GetBookmarkModel(without favicon)
  for (int index = 0; index < 20; index++) {
    wstring title(L"TestBookmark");
        title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, bm_bar0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  for (int index = 0; index < 10; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, bm_other0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 370563.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddSeveralBMsAndFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_other0 = bm0->other_node();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add some GetBookmarkModel(without favicon)
  for (int index = 0; index < 15; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
        wstring title(L"BB - TestBookmark");
        title.append(IntToWStringHack(index));
        string url("http://www.nofaviconurl");
        url.append(base::IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm =
            v->AddURL(bm0, bm_bar0, index, title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"BB - TestBMFolder");
        title.append(IntToWStringHack(index));
        const BookmarkNode* bm_folder = v->AddGroup(bm0, bm_bar0,
            index, title);
        int random_int2 = base::RandInt(1, 100);
        // 60% of time we will add bookmarks to added folder
        if (random_int2 > 40) {
            for (int index = 0; index < 20; index++) {
              wstring child_title(title);
              child_title.append(L" - ChildTestBM");
              child_title.append(IntToWStringHack(index));
              string url("http://www.nofaviconurl");
              url.append(base::IntToString(index));
              url.append(".com");
              const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
                    bm_folder, index, child_title, GURL(url));
              ASSERT_TRUE(nofavicon_bm != NULL);
            }
        }
    }
  }
  LOG(INFO) << "Adding several bookmarks under other bookmarks";
  for (int index = 0; index < 10; index++) {
    wstring title(L"Other - TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl-other");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, bm_other0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 370641.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DuplicateBMWithDifferentURLSameName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add two bookmarks with different URL but same name
  {
    const BookmarkNode* google_bm = v->AddURL(bm0, bm_bar0, 0,
        L"Google", GURL("http://www.google.com"));
    ASSERT_TRUE(google_bm != NULL);
    const BookmarkNode* google_news_bm = v->AddURL(bm0, bm_bar0, 1,
        L"Google", GURL("http://www.google.com/news"));
    ASSERT_TRUE(google_news_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371817.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
    // Add first bookmark
  const BookmarkNode* test_bm1 = v->AddURL(
      bm0, bm_bar0, 0, L"Test BM", GURL("http://www.bmtest.com"));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Rename recently added BM
    v->SetTitle(bm0, test_bm1, L"New Test BM");
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371822.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add first bookmark(without favicon)
  const BookmarkNode* nofavicon_bm =
      v->AddURL(bm0, bm_bar0, 0, L"Google",
          GURL("http://www.google.com"));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's rename/change URL
  nofavicon_bm = v->SetURL(bm0, nofavicon_bm,
      GURL("http://www.cnn.com"));
  // Wait for changes to sync and then verify
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371824.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add first bookmark folder to client1
  const BookmarkNode* new_folder_one = v->AddGroup(bm0, bm_bar0, 0,
      L"TestBMFolder");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Rename recently added Bookmark folder
  v->SetTitle(bm0, new_folder_one, L"New TestBMFolder");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371825.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameEmptyBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
      v->AddGroup(bm0, bm_bar0, 0, L"TestFolder");

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's rename newly added bookmark folder
  v->SetTitle(bm0, bm_folder_one, L"New TestFolder");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371826.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMFolderWithLongHierarchy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add first bookmark folder to under bookmark_bar.
  const BookmarkNode* test_bm_folder =
      v->AddGroup(bm0, bm_bar0, 0, L"Test BMFolder");

  // Let's add lots of bookmarks and folders underneath test_bm_folder.
  for (int index = 0; index < 120; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 85% of time add bookmarks
    if (random_int > 15) {
        wstring title(L"Test BMFolder - ChildTestBookmark");
        title.append(IntToWStringHack(index));
        string url("http://www.nofaviconurl");
        url.append(base::IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm =
            v->AddURL(bm0, test_bm_folder, index,
            title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"Test BMFolder - ChildTestBMFolder");
        title.append(IntToWStringHack(index));
        const BookmarkNode* bm_folder =
            v->AddGroup(bm0, test_bm_folder, index, title);
        ASSERT_TRUE(bm_folder != NULL);
    }
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's rename test_bm_folder.
  v->SetTitle(bm0, test_bm_folder, L"New TestBMFolder");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371827.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMFolderThatHasParentAndChildren) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add first bookmark folder to under bookmark_bar.
  const BookmarkNode* parent_bm_folder =
      v->AddGroup(bm0, bm_bar0, 0, L"Parent TestBMFolder");

  // Let's add few bookmarks under bookmark_bar.
  for (int index = 1; index < 15; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, bm_bar0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  // Let's add first bookmark folder under parent_bm_folder.
  const BookmarkNode* test_bm_folder =
      v->AddGroup(bm0, parent_bm_folder, 0, L"Test BMFolder");
  // Let's add lots of bookmarks and folders underneath test_bm_folder.
  for (int index = 0; index < 120; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 85% of time add bookmarks
    if (random_int > 15) {
        wstring title(L"Test BMFolder - ChildTestBookmark");
        title.append(IntToWStringHack(index));
        string url("http://www.nofaviconurl");
        url.append(base::IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm =
            v->AddURL(bm0, test_bm_folder, index,
            title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"Test BMFolder - ChildTestBMFolder");
        title.append(IntToWStringHack(index));
        const BookmarkNode* bm_folder =
            v->AddGroup(bm0, test_bm_folder, index, title);
        ASSERT_TRUE(bm_folder != NULL);
    }
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's rename test_bm_folder.
  v->SetTitle(bm0, test_bm_folder, L"New TestBMFolder");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371828.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMNameAndURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add first bookmark(without favicon)
  const BookmarkNode* nofavicon_bm =
      v->AddURL(bm0, bm_bar0, 0, L"Google",
      GURL("http://www.google.com"));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's change the URL.
  nofavicon_bm = v->SetURL(bm0, nofavicon_bm,
      GURL("http://www.cnn.com"));
  // Let's change the Name.
  v->SetTitle(bm0, nofavicon_bm, L"CNN");
  // Wait for changes to sync and then verify
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371832.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteBMEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add first bookmark(without favicon)
  {
    const BookmarkNode* nofavicon_bm = v->AddURL(bm0, bm_bar0, 0,
        L"TestBookmark", GURL("http://www.nofaviconurl.com"));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Delete this newly created bookmark
    v->Remove(bm0, bm_bar0, 0);
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  // Make sure that client2 has pushed all of it's changes as well.
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371833.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add some GetBookmarkModel(without favicon)
  for (int index = 0; index < 20; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = v->AddURL(
        bm0, bm_bar0, index,
        title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Delete this newly created bookmark
    v->Remove(bm0, bm_bar0, 0);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}


// Test Scribe ID - 371835.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelFirstBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
      v->AddGroup(bm0, bm_bar0, 0, L"TestFolder");
  // Let's add some GetBookmarkModel(without favicon) to this folder
  for (int index = 0; index < 10; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Delete first bookmark under this folder
    v->Remove(bm0, bm_folder_one, 0);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}


// Test Scribe ID - 371836.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelLastBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one = v->AddGroup(bm0, bm_bar0,
      0, L"TestFolder");
  // Let's add some GetBookmarkModel(without favicon) to this folder
  for (int index = 0; index < 10; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = v->AddURL(
        bm0, bm_folder_one,
        index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Delete last bookmark under this folder
    v->Remove(bm0, bm_folder_one,
        (bm_folder_one->GetChildCount() - 1));
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}


// Test Scribe ID - 371856.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelMiddleBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
    v->AddGroup(bm0, bm_bar0, 0, L"TestFolder");
  // Let's add some GetBookmarkModel(without favicon) to this folder
  for (int index = 0; index < 10; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Delete middle bookmark under this folder
    v->Remove(bm0, bm_folder_one, 4);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}


// Test Scribe ID - 371857.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMsUnderBMFoldEmptyFolderAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
    v->AddGroup(bm0, bm_bar0, 0, L"TestFolder");
  // Let's add some GetBookmarkModel(without favicon) to this folder
  for (int index = 0; index < 10; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  int child_count = bm_folder_one->GetChildCount();
  // Let's delete all the bookmarks added under this new folder
  for (int index = 0; index < child_count; index++) {
    v->Remove(bm0, bm_folder_one, 0);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    v->ExpectMatch(bm0);
    v->ExpectMatch(bm1);
  }
}

// Test Scribe ID - 371858.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelEmptyBMFoldEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
    v->AddGroup(bm0, bm_bar0, 0, L"TestFolder");
  ASSERT_TRUE(bm_folder_one != NULL);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's delete this empty bookmark folder
  v->Remove(bm0, bm_bar0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}


// Test Scribe ID - 371869.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelEmptyBMFoldNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_other0 = bm0->other_node();
  ASSERT_TRUE(bm_other0 != NULL);

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
      v->AddGroup(bm0, bm_bar0, 0, L"TestFolder");
  ASSERT_TRUE(bm_folder_one != NULL);
  // Let's add some GetBookmarkModel(without favicon)
  for (int index = 1; index < 15; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
      wstring title(L"BB - TestBookmark");
      title.append(IntToWStringHack(index));
      string url("http://www.nofaviconurl");
      url.append(base::IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = v->AddURL(bm0, bm_bar0,
          index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
      // Remaining % of time - Add Bookmark folders
      wstring title(L"BB - TestBMFolder");
      title.append(IntToWStringHack(index));
      const BookmarkNode* bm_folder = v->AddGroup(bm0, bm_bar0,
          index, title);
      ASSERT_TRUE(bm_folder != NULL);
     }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's delete the first empty bookmark folder
  v->Remove(bm0, bm_bar0, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371879.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMFoldWithBMsNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  ASSERT_TRUE(bm_bar0 != NULL);
  const BookmarkNode* bm_other0 = bm0->other_node();
  ASSERT_TRUE(bm_other0 != NULL);

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = v->AddURL(bm0, bm_bar0, 0,
      L"Yahoo!", GURL("http://www.yahoo.com"));
  ASSERT_TRUE(yahoo != NULL);
  const BookmarkNode* bm_folder_one =
      v->AddGroup(bm0, bm_bar0, 1, L"TestFolder");
  // Let's add some GetBookmarkModel(without favicon) and folders to
  // bookmark bar
  for (int index = 2; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
        wstring title(L"BB - TestBookmark");
        title.append(IntToWStringHack(index));
        string url("http://www.nofaviconurl");
        url.append(base::IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm = v->AddURL(bm0, bm_bar0,
            index, title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"BB - TestBMFolder");
        title.append(IntToWStringHack(index));
        const BookmarkNode* bm_folder = v->AddGroup(bm0, bm_bar0,
            index, title);
        ASSERT_TRUE(bm_folder != NULL);
     }
  }

  // Let's add some bookmarks(without favicon) to bm_folder_one ('TestFolder')
  for (int index = 0; index < 15; index++) {
    wstring title(L"Level2 - TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
        bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);

  // Let's delete the bookmark folder (bm_folder_one)
  v->Remove(bm0, bm_bar0, 1);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);
}

// Test Scribe ID - 371880.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMFoldWithBMsAndBMFoldsNonEmptyACAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_other0 = bm0->other_node();
  ASSERT_TRUE(bm_other0 != NULL);

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = v->AddURL(bm0, bm_bar0, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  ASSERT_TRUE(yahoo != NULL);
  const BookmarkNode* bm_folder_one =
      v->AddGroup(bm0, bm_bar0, 1, L"TestFolder");
  // Let's add some GetBookmarkModel(without favicon) and folders to
  // bookmark bar
  for (int index = 2; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
        wstring title(L"BB - TestBookmark");
        title.append(IntToWStringHack(index));
        string url("http://www.nofaviconurl");
        url.append(base::IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm = v->AddURL(bm0, bm_bar0,
            index, title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"BB - TestBMFolder");
        title.append(IntToWStringHack(index));
        const BookmarkNode* bm_folder = v->AddGroup(bm0, bm_bar0,
            index, title);
        ASSERT_TRUE(bm_folder != NULL);
     }
  }

  // Let's add some GetBookmarkModel(without favicon) and folders to
  // bm_folder_one ('TestFolder')
  for (int index = 0; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
      wstring title(L"Level2 - TestBookmark");
      title.append(IntToWStringHack(index));
      string url("http://www.nofaviconurl");
      url.append(base::IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
          bm_folder_one, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"Level2 - TestBMFolder");
        title.append(IntToWStringHack(index));
        const BookmarkNode* l2_bm_folder = v->AddGroup(bm0,
            bm_folder_one, index, title);
        int random_int2 = base::RandInt(1, 100);
        // 70% of time - Let's add more levels of bookmarks and folders to
        // l2_bm_folder
        if (random_int2 > 30) {
          for (int index2 = 0; index2 < 10; index2++) {
            int random_int3 = base::RandInt(1, 100);
            // To create randomness in order, 40% of time add bookmarks
            if (random_int3 > 60) {
              wstring title(L"Level3 - TestBookmark");
              title.append(IntToWStringHack(index));
              string url("http://www.nofaviconurl");
              url.append(base::IntToString(index));
              url.append(".com");
              const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
                  l2_bm_folder, index2, title, GURL(url));
              ASSERT_TRUE(nofavicon_bm != NULL);
            } else {
                // Remaining % of time - Add Bookmark folders
                wstring title(L"Level3 - TestBMFolder");
                title.append(IntToWStringHack(index));
                const BookmarkNode* l3_bm_folder =
                    v->AddGroup(bm0, l2_bm_folder, index2, title);
                ASSERT_TRUE(l3_bm_folder != NULL);
             }
          }  // end inner for loop
        }
     }
    }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);

  // Let's delete the bookmark folder (bm_folder_one)
  v->Remove(bm0, bm_bar0, 1);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);
}

// Test Scribe ID - 371882.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMFoldWithParentAndChildrenBMsAndBMFolds) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add first bookmark folder to under bookmark_bar.
  const BookmarkNode* parent_bm_folder =
      v->AddGroup(bm0, bm_bar0, 0, L"Parent TestBMFolder");

  // Let's add few bookmarks under bookmark_bar.
  for (int index = 1; index < 11; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, bm_bar0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  // Let's add first bookmark folder under parent_bm_folder.
  const BookmarkNode* test_bm_folder =
      v->AddGroup(bm0, parent_bm_folder, 0, L"Test BMFolder");
  // Let's add lots of bookmarks and folders underneath test_bm_folder.
  for (int index = 0; index < 30; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 80% of time add bookmarks
    if (random_int > 20) {
        wstring title(L"Test BMFolder - ChildTestBookmark");
        title.append(IntToWStringHack(index));
        string url("http://www.nofaviconurl");
        url.append(base::IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm =
            v->AddURL(bm0, test_bm_folder, index, title,
            GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"Test BMFolder - ChildTestBMFolder");
        title.append(IntToWStringHack(index));
        const BookmarkNode* bm_folder =
            v->AddGroup(bm0, test_bm_folder, index, title);
        ASSERT_TRUE(bm_folder != NULL);
    }
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);

  // Let's delete test_bm_folder
  v->Remove(bm0, parent_bm_folder, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);
}

// Test Scribe ID - 371931.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_ReverseTheOrderOfTwoBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  const BookmarkNode* bm_a = v->AddURL(
      bm0, bm_bar0, 0, L"Bookmark A",
      GURL("http://www.nofaviconurla.com"));
  ASSERT_TRUE(bm_a != NULL);
  const BookmarkNode* bm_b = v->AddURL(
      bm0, bm_bar0, 1, L"Bookmark B",
      GURL("http://www.nofaviconurlb.com"));
  ASSERT_TRUE(bm_b != NULL);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  {
    // Move bm_a to new position
    v->Move(bm0, bm_a, bm_bar0, 2);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371933.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_ReverseTheOrderOf10BMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add 10 bookmarks like 0123456789
  for (int index = 0; index < 10; index++) {
    wstring title(L"BM-");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl-");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
        bm_bar0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Shuffle bookmarks to make it look like 9876543210
  v->ReverseChildOrder(bm0, bm_bar0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371954.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_MovingBMsFromBMBarToBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_other0 = bm0->other_node();
  ASSERT_TRUE(bm_other0 != NULL);

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = v->AddURL(bm0, bm_bar0, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  ASSERT_TRUE(yahoo != NULL);
  const BookmarkNode* bm_folder_one =
      v->AddGroup(bm0, bm_bar0, 1, L"TestFolder");
  // Let's add some GetBookmarkModel(without favicon) to bookmark bar
  for (int index = 2; index < 10; index++) {
    wstring title(L"BB - TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = v->AddURL(bm0, bm_bar0,
        index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move bookmarks from bookmark bar to BMfolder (bm_folder_one)
  int child_count_to_move = bm_bar0->GetChildCount() - 2;
  for (int index = 0; index < child_count_to_move; index++) {
    v->Move(bm0, bm_bar0->GetChild(2),
        bm_folder_one, index);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    v->ExpectMatch(bm0);
    v->ExpectMatch(bm1);
  }
}

// Test Scribe ID - 371957.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_MovingBMsFromBMFoldToBMBar) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_other0 = bm0->other_node();
  ASSERT_TRUE(bm_other0 != NULL);

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = v->AddURL(bm0, bm_bar0, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  ASSERT_TRUE(yahoo != NULL);
  const BookmarkNode* bm_folder_one =
      v->AddGroup(bm0, bm_bar0, 1, L"TestFolder");
  // Let's add some GetBookmarkModel(without favicon) to bm_folder_one
  for (int index = 0; index < 10; index++) {
    wstring title(L"BB - TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
        bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move bookmarks from BMfolder(bm_folder_one) to bookmark bar
  int child_count_to_move = bm_folder_one->GetChildCount();
  for (int index = 0; index < child_count_to_move; index++) {
    v->Move(bm0, bm_folder_one->GetChild(0),
        bm_bar0, bm_bar0->GetChildCount());
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    v->ExpectMatch(bm0);
    v->ExpectMatch(bm1);
  }
}

// Test Scribe ID - 371961.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_MovingBMsFromParentBMFoldToChildBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  const BookmarkNode* parent_folder =
      v->AddGroup(bm0, bm_bar0, 0, L"Test Parent BMFolder");

  // Let's add bookmarks a,b,c to parent_folder.
  const BookmarkNode* bm_a = v->AddURL(
      bm0, parent_folder, 0, L"Bookmark A",
      GURL("http://www.nofaviconurl-a.com"));
  const BookmarkNode* bm_b = v->AddURL(
      bm0, parent_folder, 1, L"Bookmark B",
      GURL("http://www.nofaviconurl-b.com"));
  const BookmarkNode* bm_c = v->AddURL(
      bm0, parent_folder, 2, L"Bookmark C",
      GURL("http://www.nofaviconurl-c.com"));
  const BookmarkNode* child_folder =
      v->AddGroup(bm0, parent_folder, 3, L"Test Child BMFolder");

  // Let's add few bookmarks under child_folder.
  for (int index = 0; index < 10; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWStringHack(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        v->AddURL(bm0, child_folder, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move bookmark a,b,c to child_folder.
  v->Move(bm0, bm_a, child_folder, 10);
  v->Move(bm0, bm_b, child_folder, 11);
  v->Move(bm0, bm_c, child_folder, 12);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}


// Test Scribe ID - 371964.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_MovingBMsFromChildBMFoldToParentBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  const BookmarkNode* parent_folder =
      v->AddGroup(bm0, bm_bar0, 0, L"Test Parent BMFolder");

  // Let's add bookmarks a,b,c to parent_folder.
  const BookmarkNode* bm_a = v->AddURL(
      bm0, parent_folder, 0, L"Bookmark A",
      GURL("http://www.nofaviconurl-a.com"));
  ASSERT_TRUE(bm_a != NULL);
  const BookmarkNode* bm_b = v->AddURL(
      bm0, parent_folder, 1, L"Bookmark B",
      GURL("http://www.nofaviconurl-b.com"));
  ASSERT_TRUE(bm_b != NULL);
  const BookmarkNode* bm_c = v->AddURL(
      bm0, parent_folder, 2, L"Bookmark C",
      GURL("http://www.nofaviconurl-c.com"));
  ASSERT_TRUE(bm_c != NULL);
  const BookmarkNode* child_folder =
      v->AddGroup(bm0, parent_folder, 3, L"Test Child BMFolder");

  // Let's add bookmarks d,e,f,g,h to child_folder.
  const BookmarkNode* bm_d = v->AddURL(
      bm0, child_folder, 0, L"Bookmark D",
      GURL("http://www.nofaviconurl-d.com"));
  ASSERT_TRUE(bm_d != NULL);
  const BookmarkNode* bm_e = v->AddURL(
      bm0, child_folder, 1, L"Bookmark E",
      GURL("http://www.nofaviconurl-e.com"));
  ASSERT_TRUE(bm_e != NULL);
  const BookmarkNode* bm_f = v->AddURL(
      bm0, child_folder, 2, L"Bookmark F",
      GURL("http://www.nofaviconurl-f.com"));
  ASSERT_TRUE(bm_f != NULL);
  const BookmarkNode* bm_g = v->AddURL(
      bm0, child_folder, 3, L"Bookmark G",
      GURL("http://www.nofaviconurl-g.com"));
  ASSERT_TRUE(bm_g != NULL);
  const BookmarkNode* bm_h = v->AddURL(
      bm0, child_folder, 4, L"Bookmark H",
      GURL("http://www.nofaviconurl-h.com"));
  ASSERT_TRUE(bm_h != NULL);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move bookmark d,e,h to parent_folder.
  v->Move(bm0, bm_d, parent_folder, 4);
  v->Move(bm0, bm_e, parent_folder, 3);
  v->Move(bm0, bm_h, parent_folder, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}


// Test Scribe ID - 371967.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_HoistBMs10LevelUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  const BookmarkNode* bm_folder = bm_bar0;
  const BookmarkNode* bm_folder_L10 = NULL;
  const BookmarkNode* bm_folder_L0 = NULL;
  for (int level = 0; level < 15; level++) {
    // Let's add some GetBookmarkModel(without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(UTF16ToWideHack(bm_folder->GetTitle()));
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWStringHack(index));
      url.append(base::IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWStringHack(level));

    bm_folder = v->AddGroup(bm0,
        bm_folder, bm_folder->GetChildCount(), title);
    // Let's remember first bm folder for later use.
    if (level == 0) {
      bm_folder_L0 = bm_folder;
    }
    // Let's remember 10th level bm folder for later use.
    if (level == 9) {
      bm_folder_L10 = bm_folder;
    }
  }
  const BookmarkNode* bm_a = v->AddURL(bm0,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-A",
      GURL("http://www.bm-a.com"));
  const BookmarkNode* bm_b = v->AddURL(bm0,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-B",
      GURL("http://www.bm-b.com"));
  const BookmarkNode* bm_c = v->AddURL(bm0,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-C",
      GURL("http://www.bm-c.com"));
  // Let's wait till all the changes populate to another client.
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move bookmark a from bm_folder_L10 to first bookmark folder- at end.
  v->Move(bm0, bm_a, bm_folder_L0, bm_folder_L0->GetChildCount());
  // Let's move bookmark b to first bookmark folder- at the beginning.
  v->Move(bm0, bm_b, bm_folder_L0, 0);
  // Let's move bookmark c to first bookmark folder- in the middle.
  v->Move(bm0, bm_c, bm_folder_L0, 1);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371968.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_SinkBMs10LevelDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
  const BookmarkNode* bm_folder = bm_bar0;
  const BookmarkNode* bm_folder_L10 = NULL;
  const BookmarkNode* bm_folder_L0 = NULL;
  for (int level = 0; level < 15; level++) {
    // Let's add some GetBookmarkModel(without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(UTF16ToWideHack(bm_folder->GetTitle()));
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWStringHack(index));
      url.append(base::IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWStringHack(level));

    bm_folder = v->AddGroup(bm0,
        bm_folder, bm_folder->GetChildCount(), title);
    // Let's remember first bm folder for later use.
    if (level == 0) {
      bm_folder_L0 = bm_folder;
    }
    // Let's remember 10th level bm folder for later use.
    if (level == 9) {
      bm_folder_L10 = bm_folder;
    }
  }
  const BookmarkNode* bm_a = v->AddURL(bm0,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-A",
      GURL("http://www.bm-a.com"));
  const BookmarkNode* bm_b = v->AddURL(bm0,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-B",
      GURL("http://www.bm-b.com"));
  const BookmarkNode* bm_c = v->AddURL(bm0,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-C",
      GURL("http://www.bm-c.com"));
  // Let's wait till all the changes populate to another client.
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move bookmark a from bm_folder_L10 to first bookmark
  // folder- at end.
  v->Move(bm0, bm_a, bm_folder_L10,
      bm_folder_L10->GetChildCount());
  // Let's move bookmark b to first bookmark folder- at the beginning.
  v->Move(bm0, bm_b, bm_folder_L10, 0);
  // Let's move bookmark c to first bookmark folder- in the middle.
  v->Move(bm0, bm_c, bm_folder_L10, 1);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}


// Test Scribe ID - 371980.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_SinkEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  const BookmarkNode* bm_folder = bm_bar0;
  const BookmarkNode* bm_folder_L5 = NULL;
  for (int level = 0; level < 6; level++) {
    // Let's add some GetBookmarkModel(without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(UTF16ToWideHack(bm_folder->GetTitle()));
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWStringHack(index));
      url.append(base::IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWStringHack(level));

    bm_folder = v->AddGroup(bm0,
        bm_folder, bm_folder->GetChildCount(), title);
    // Let's remember 5th level bm folder for later use.
    if (level == 5) {
      bm_folder_L5 = bm_folder;
    }
  }
  const BookmarkNode* empty_bm_folder = v->AddGroup(bm0,
      bm_bar0, bm_bar0->GetChildCount(), L"EmptyTest BMFolder");

  // Let's wait until all the changes populate to another client.
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move empty_bm_folder from bookmark bar to bm_folder_L5 (at the end).
  v->Move(bm0, empty_bm_folder, bm_folder_L5,
      bm_folder_L5->GetChildCount());

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 371997.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_SinkNonEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  const BookmarkNode* bm_folder = bm_bar0;
  const BookmarkNode* bm_folder_L5 = NULL;
  for (int level = 0; level < 6; level++) {
    // Let's add some bookmarks (without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(UTF16ToWideHack(bm_folder->GetTitle()));
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWStringHack(index));
      url.append(base::IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWStringHack(level));

    bm_folder = v->AddGroup(bm0,
        bm_folder, bm_folder->GetChildCount(), title);
    // Let's remember 5th level bm folder for later use.
    if (level == 5) {
      bm_folder_L5 = bm_folder;
    }
  }
  const BookmarkNode* my_bm_folder = v->AddGroup(bm0,
      bm_bar0, bm_bar0->GetChildCount(), L"MyTest BMFolder");
  // Let's add few bookmarks to my_bm_folder.
  for (int index = 0; index < 10; index++) {
    wstring title(UTF16ToWideHack(bm_folder->GetTitle()));
    title.append(L"-BM");
    string url("http://www.nofaviconurl-");
    title.append(IntToWStringHack(index));
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
        my_bm_folder, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  // Let's wait until all the changes populate to another client.
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move my_bm_folder from bookmark bar to bm_folder_L5 (at the end).
  v->Move(bm0, my_bm_folder, bm_folder_L5,
      bm_folder_L5->GetChildCount());

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 372006.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_HoistFolder5LevelsUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  const BookmarkNode* bm_folder = bm_bar0;
  const BookmarkNode* bm_folder_L5 = NULL;
  for (int level = 0; level < 6; level++) {
    // Let's add some GetBookmarkModel(without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(UTF16ToWideHack(bm_folder->GetTitle()));
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWStringHack(index));
      url.append(base::IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWStringHack(level));

    bm_folder = v->AddGroup(bm0,
        bm_folder, bm_folder->GetChildCount(), title);
    // Let's remember 5th level bm folder for later use.
    if (level == 5) {
      bm_folder_L5 = bm_folder;
    }
  }
  const BookmarkNode* my_bm_folder = v->AddGroup(bm0,
      bm_folder_L5, bm_folder_L5->GetChildCount(), L"MyTest BMFolder");
  // Let's add few bookmarks to my_bm_folder.
  for (int index = 0; index < 10; index++) {
    wstring title(UTF16ToWideHack(bm_folder->GetTitle()));
    title.append(L"-BM");
    string url("http://www.nofaviconurl-");
    title.append(IntToWStringHack(index));
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = v->AddURL(bm0,
        my_bm_folder, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  // Let's wait until all the changes populate to another client.
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's move my_bm_folder from  bm_folder_L5 to bookmark bar- at end.
  v->Move(bm0, my_bm_folder, bm_bar0, bm_bar0->GetChildCount());

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}



// Test Scribe ID - 372026.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_ReverseTheOrderOfTwoBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();

  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  const BookmarkNode* bm_folder_a =
    v->AddNonEmptyGroup(bm0, bm_bar0, 0, L"TestBMFolderA", 10);
  ASSERT_TRUE(bm_folder_a != NULL);
  const BookmarkNode* bm_folder_b =
    v->AddNonEmptyGroup(bm0, bm_bar0, 1, L"TestBMFolderB", 10);
  ASSERT_TRUE(bm_folder_b != NULL);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's change positions of bookmark folders so it is more like ba.
  v->ReverseChildOrder(bm0, bm_bar0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 372028.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_ReverseTheOrderOfTenBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Let's add 10 non-empty bookmark folders like 0123456789
  for (int index = 0; index < 10; index++) {
    wstring title(L"BM Folder");
    title.append(IntToWStringHack(index));
    const BookmarkNode* child_bm_folder = v->AddNonEmptyGroup(
        bm0, bm_bar0, index, title, 10);
    ASSERT_TRUE(child_bm_folder != NULL);
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);

  // Shuffle bookmark folders to make it look like 9876543210
  v->ReverseChildOrder(bm0, bm_bar0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 373379.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_BiDirectionalPushAddingBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_bar1 = bm1->GetBookmarkBarNode();

  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);

  // Let's add 2 bookmarks (without favicon) on each client.
  {
    const BookmarkNode* bm_foo1 = bm0->AddURL(
        bm_bar0, 0, ASCIIToUTF16("Foo1"), GURL("http://www.foo1.com"));
    ASSERT_TRUE(bm_foo1 != NULL);
    const BookmarkNode* bm_foo3 = bm1->AddURL(
        bm_bar1, 0, ASCIIToUTF16("Foo3"), GURL("http://www.foo3.com"));
    ASSERT_TRUE(bm_foo3 != NULL);

    const BookmarkNode* bm_foo2 = bm0->AddURL(
        bm_bar0, 1, ASCIIToUTF16("Foo2"), GURL("http://www.foo2.com"));
    ASSERT_TRUE(bm_foo2 != NULL);
    const BookmarkNode* bm_foo4 = bm1->AddURL(
        bm_bar1, 1, ASCIIToUTF16("Foo4"), GURL("http://www.foo4.com"));
    ASSERT_TRUE(bm_foo4 != NULL);
  }

  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  BookmarkModelVerifier::ExpectModelsMatch(bm0, bm1);
  BookmarkModelVerifier::VerifyNoDuplicates(bm0);
}

// Test Scribe ID - 373503.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
                       MC_BiDirectionalPush_AddingSameBMs) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  BookmarkModel* profile0_bookmark_model = GetBookmarkModel(0);
  BookmarkModel* profile1_bookmark_model = GetBookmarkModel(1);

  const BookmarkNode* profile0_bookmark_bar =
      profile0_bookmark_model->GetBookmarkBarNode();
  const BookmarkNode* profile1_bookmark_bar =
      profile1_bookmark_model->GetBookmarkBarNode();

  {
    const BookmarkNode* profile0_bookmark1 = profile0_bookmark_model->AddURL(
        profile0_bookmark_bar, 0, ASCIIToUTF16("Bookmark1_name"),
        GURL("http://www.bookmark1name.com"));
    ASSERT_TRUE(profile0_bookmark1 != NULL);
    const BookmarkNode* profile0_bookmark2 = profile0_bookmark_model->AddURL(
        profile0_bookmark_bar, 1, ASCIIToUTF16("Bookmark2_name"),
        GURL("http://www.bookmark2name.com"));
    ASSERT_TRUE(profile0_bookmark2 != NULL);

    const BookmarkNode* profile1_bookmark1 = profile1_bookmark_model->AddURL(
        profile1_bookmark_bar, 0, ASCIIToUTF16("Bookmark2_name"),
        GURL("http://www.bookmark2name.com"));
    ASSERT_TRUE(profile1_bookmark1 != NULL);
    const BookmarkNode* profile1_bookmark2 = profile1_bookmark_model->AddURL(
        profile1_bookmark_bar, 1, ASCIIToUTF16("Bookmark1_name"),
        GURL("http://www.bookmark1name.com"));
    ASSERT_TRUE(profile1_bookmark2 != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  BookmarkModelVerifier::ExpectModelsMatch(profile0_bookmark_model,
      profile1_bookmark_model);
  BookmarkModelVerifier::VerifyNoDuplicates(profile0_bookmark_model);
}

// Test Scribe ID - 373506.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_BootStrapEmptyStateEverywhere) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  BookmarkModelVerifier* v = verifier_helper();
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  v->ExpectMatch(bm0);
  v->ExpectMatch(bm1);
}

// Test Scribe ID - 373505.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
                       MC_Merge_CaseInsensitivity_InNames) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  BookmarkModel* profile0_bookmark_model = GetBookmarkModel(0);
  BookmarkModel* profile1_bookmark_model = GetBookmarkModel(1);

  const BookmarkNode* profile0_bookmark_bar =
      profile0_bookmark_model->GetBookmarkBarNode();

  const BookmarkNode* profile1_bookmark_bar =
      profile1_bookmark_model->GetBookmarkBarNode();

  {
    const BookmarkNode* profile0_folder = profile0_bookmark_model->AddGroup(
        profile0_bookmark_bar, 0, ASCIIToUTF16("TeSt BMFolder"));
    ASSERT_TRUE(profile0_folder != NULL);
    const BookmarkNode* profile0_bookmark1 = profile0_bookmark_model->AddURL(
        profile0_folder, 0, ASCIIToUTF16("bookmark1_name"),
        GURL("http://www.bookmark1name.com"));
    ASSERT_TRUE(profile0_bookmark1 != NULL);
    const BookmarkNode* profile0_bookmark2 = profile0_bookmark_model->AddURL(
        profile0_folder, 1, ASCIIToUTF16("bookmark2_name"),
        GURL("http://www.bookmark2name.com"));
    ASSERT_TRUE(profile0_bookmark2 != NULL);
    const BookmarkNode* profile0_bookmark3 = profile0_bookmark_model->AddURL(
        profile0_folder, 2, ASCIIToUTF16("BOOKMARK3_NAME"),
        GURL("http://www.bookmark3name.com"));
    ASSERT_TRUE(profile0_bookmark3 != NULL);

    const BookmarkNode* profile1_folder = profile1_bookmark_model->AddGroup(
        profile1_bookmark_bar, 0, ASCIIToUTF16("test bMFolder"));
    ASSERT_TRUE(profile1_folder != NULL);
    const BookmarkNode* profile1_bookmark1 = profile1_bookmark_model->AddURL(
        profile1_folder, 0, ASCIIToUTF16("bookmark3_name"),
        GURL("http://www.bookmark3name.com"));
    ASSERT_TRUE(profile1_bookmark1 != NULL);
    const BookmarkNode* profile1_bookmark2 = profile1_bookmark_model->AddURL(
        profile1_folder, 1, ASCIIToUTF16("bookMARK2_Name"),
        GURL("http://www.bookmark2name.com"));
    ASSERT_TRUE(profile1_bookmark2 != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  BookmarkModelVerifier::ExpectModelsMatch(profile0_bookmark_model,
      profile1_bookmark_model);
  BookmarkModelVerifier::VerifyNoDuplicates(profile0_bookmark_model);
  BookmarkModelVerifier::VerifyNoDuplicates(profile1_bookmark_model);
}

// Test Scribe ID - 373508.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_SimpleMergeOfDifferentBMModels) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);

  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_bar1 = bm1->GetBookmarkBarNode();

  // Let's add same bookmarks (without favicon) to both clients.
  for (int index = 0; index < 3; index++) {
    string16 title(ASCIIToUTF16("TestBookmark"));
    title.append(base::IntToString16(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client0 =
        bm0->AddURL(bm_bar0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client0 != NULL);
    const BookmarkNode* nofavicon_bm_client1 =
        bm1->AddURL(bm_bar1, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
  }

  // Let's add some different bookmarks (without favicon) to client1.
  for (int index = 3; index < 11 ; index++) {
    string16 title(ASCIIToUTF16("Client1-TestBookmark"));
    title.append(base::IntToString16(index));
    string url("http://www.client1-nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client0 =
        bm0->AddURL(bm_bar0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client0 != NULL);
  }

   // Let's add some different bookmarks (without favicon) to client2.
  for (int index = 3; index < 11 ; index++) {
    string16 title(ASCIIToUTF16("Client2-TestBookmark"));
    title.append(base::IntToString16(index));
    string url("http://www.Client2-nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client1 =
        bm1->AddURL(bm_bar1, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
  }

  // Set up sync on both clients.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Wait for changes to propagate.
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  // Let's make sure there aren't any duplicates after sync.
  BookmarkModelVerifier::VerifyNoDuplicates(bm0);
  // Let's compare and make sure both bookmark models are same after sync.
  BookmarkModelVerifier::ExpectModelsMatchIncludingFavicon(
      bm0, bm1, false);
}

// Test Scribe ID - 386586.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_MergeSimpleBMHierarchyUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  ui_test_utils::WaitForBookmarkModelToLoad(bm1);

  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_bar1 = bm1->GetBookmarkBarNode();

  // Let's add same bookmarks (without favicon) to both clients.
  for (int index = 0; index < 3 ; index++) {
    string16 title(ASCIIToUTF16("TestBookmark"));
    title.append(base::IntToString16(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client0 =
        bm0->AddURL(bm_bar0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client0 != NULL);
    const BookmarkNode* nofavicon_bm_client1 =
        bm1->AddURL(bm_bar1, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
  }

  // Let's add some different bookmarks (without favicon) to client2.
  for (int index = 3; index < 5 ; index++) {
    string16 title(ASCIIToUTF16("Client2-TestBookmark"));
    title.append(base::IntToString16(index));
    string url("http://www.client2-nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client1 =
        bm1->AddURL(bm_bar1, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
  }

  // Set up sync on both clients.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Wait for changes to propagate.
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  // Let's make sure there aren't any duplicates after sync.
  BookmarkModelVerifier::VerifyNoDuplicates(bm0);
  // Let's compare and make sure both bookmark models are same after sync.
  BookmarkModelVerifier::ExpectModelsMatchIncludingFavicon(
      bm0, bm1, false);
}

// Test Scribe ID - 386589.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_MergeSimpleBMHierarchyEqualSetsUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  BookmarkModel* bm0 = GetBookmarkModel(0);
  BookmarkModel* bm1 = GetBookmarkModel(1);
  ui_test_utils::WaitForBookmarkModelToLoad(bm1);

  const BookmarkNode* bm_bar0 = bm0->GetBookmarkBarNode();
  const BookmarkNode* bm_bar1 = bm1->GetBookmarkBarNode();

  // Let's add same bookmarks (without favicon) to both clients.
  for (int index = 0; index < 3 ; index++) {
    string16 title(ASCIIToUTF16("TestBookmark"));
    title.append(base::IntToString16(index));
    string url("http://www.nofaviconurl");
    url.append(base::IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client0 =
        bm0->AddURL(bm_bar0, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client0 != NULL);
    const BookmarkNode* nofavicon_bm_client1 =
        bm1->AddURL(bm_bar1, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
  }

  // Set up sync on both clients.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // Wait for changes to propagate.
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  // Let's make sure there aren't any duplicates after sync.
  BookmarkModelVerifier::VerifyNoDuplicates(bm0);
  // Let's compare and make sure both bookmark models are same after sync.
  BookmarkModelVerifier::ExpectModelsMatchIncludingFavicon(
      bm0, bm1, false);
}

// Test Scribe ID - 373504.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_MergeBMFoldersWithDifferentBMs) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  BookmarkModel* profile0_bookmark_model = GetBookmarkModel(0);
  BookmarkModel* profile1_bookmark_model = GetBookmarkModel(1);

  const BookmarkNode* profile0_bookmark_bar =
      profile0_bookmark_model->GetBookmarkBarNode();
  const BookmarkNode* profile1_bookmark_bar =
      profile1_bookmark_model->GetBookmarkBarNode();

  // Let's add first bookmark folder and 2 bookmarks in profile0.
  const BookmarkNode* profile0_folder = profile0_bookmark_model->AddGroup(
      profile0_bookmark_bar, 0, ASCIIToUTF16("Folder_name"));
  ASSERT_TRUE(profile0_folder != NULL);
  const BookmarkNode* profile0_bookmark1 = profile0_bookmark_model->AddURL(
      profile0_folder, 0, ASCIIToUTF16("bookmark1_name"),
      GURL("http://www.bookmark1-url.com"));
  ASSERT_TRUE(profile0_bookmark1 != NULL);
  const BookmarkNode* profile0_bookmark3 = profile0_bookmark_model->AddURL(
      profile0_folder, 1, ASCIIToUTF16("bookmark3_name"),
      GURL("http://www.bookmark3-url.com"));
  ASSERT_TRUE(profile0_bookmark3 != NULL);

  // Let's add first bookmark folder and 2 bookmarks in profile1.
  const BookmarkNode* profile1_folder = profile1_bookmark_model->AddGroup(
      profile1_bookmark_bar, 0, ASCIIToUTF16("Folder_name"));
  ASSERT_TRUE(profile1_folder != NULL);
  const BookmarkNode* profile1_bookmark2 = profile1_bookmark_model->AddURL(
      profile1_folder, 0, ASCIIToUTF16("bookmark2_name"),
      GURL("http://www.bookmark2-url.com"));
  ASSERT_TRUE(profile1_bookmark2 != NULL);
  const BookmarkNode* profile1_bookmark4 = profile1_bookmark_model->AddURL(
      profile1_folder, 1, ASCIIToUTF16("bookmark4_name"),
      GURL("http://www.bookmark4-url.com"));
  ASSERT_TRUE(profile1_bookmark4 != NULL);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  BookmarkModelVerifier::VerifyNoDuplicates(profile0_bookmark_model);
  BookmarkModelVerifier::ExpectModelsMatch(profile0_bookmark_model,
                                           profile1_bookmark_model);
}

// Test Scribe ID - 373509.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_MergeDifferentBMModelsModeratelyComplex) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  BookmarkModel* profile0_bookmark_model = GetBookmarkModel(0);
  BookmarkModel* profile1_bookmark_model = GetBookmarkModel(1);

  const BookmarkNode* profile0_bookmark_bar =
      profile0_bookmark_model->GetBookmarkBarNode();
  const BookmarkNode* profile1_bookmark_bar =
      profile1_bookmark_model->GetBookmarkBarNode();

  // Let's add some bookmarks in profile0.
  for (int index = 0; index < 20; index++) {
    string16 title = ASCIIToUTF16(StringPrintf("profile0_bookmark%d-name",
                                               index));
    string url = StringPrintf("http://www.profile0-bookmark%d-url.com", index);
    const BookmarkNode* profile0_bookmark0 = profile0_bookmark_model->AddURL(
        profile0_bookmark_bar, index, title, GURL(url));
    ASSERT_TRUE(profile0_bookmark0 != NULL);
  }

  // Let's add some bookmarks within the folders in profile0.
  for (int index = 20; index < 25; index++) {
    string16 title = ASCIIToUTF16(StringPrintf("Profile0-Folder%d_name",
                                               index));
    const BookmarkNode* profile0_folder0 = profile0_bookmark_model->AddGroup(
        profile0_bookmark_bar, index, title);
    ASSERT_TRUE(profile0_folder0 != NULL);
    for (int index = 0; index < 5; index++) {
      string16 child_title = title +
          ASCIIToUTF16(StringPrintf("-bookmark%d_name", index));
      string url = StringPrintf(
          "http://www.profile0-folder0-bookmark%d-url.com", index);
      const BookmarkNode* profile0_bookmark1 = profile0_bookmark_model->AddURL(
          profile0_folder0, index, child_title, GURL(url));
      ASSERT_TRUE(profile0_bookmark1 != NULL);
    }
  }

  // Let's add some bookmarks in profile1.
  for (int index = 0; index < 20; index++) {
    string16 title = ASCIIToUTF16(StringPrintf("profile1_bookmark%d_name",
                                               index));
    string url = StringPrintf("http://www.profile1-bookmark%d-ulr.com", index);
    const BookmarkNode* profile1_bookmark0 = profile1_bookmark_model->AddURL(
        profile1_bookmark_bar, index, title, GURL(url));
    ASSERT_TRUE(profile1_bookmark0 != NULL);
  }

  // Let's add some bookmarks within the folders in profile1.
  for (int index = 20; index < 25; index++) {
    string16 title = ASCIIToUTF16(StringPrintf("Profile1-Folder%d_name",
                                               index));
    const BookmarkNode* profile1_folder0 = profile1_bookmark_model->AddGroup(
        profile1_bookmark_bar, index, title);
    ASSERT_TRUE(profile1_folder0 != NULL);
    for (int index = 0; index < 5; index++) {
      string16 child_title = title +
          ASCIIToUTF16(StringPrintf("-bookmark%d_name", index));
      string url = StringPrintf(
          "http://www.profile1-folder0-bookmark%d-url.com", index);
      const BookmarkNode* profile1_bookmark1 = profile1_bookmark_model->AddURL(
          profile1_folder0, index, child_title, GURL(url));
      ASSERT_TRUE(profile1_bookmark1 != NULL);
    }
  }

  // Let's add same bookmarks in both the profiles.
  for (int index = 25; index < 45; index++) {
    string16 title = ASCIIToUTF16(StringPrintf("bookmark%d_name", index));
    string url = StringPrintf("http://www.bookmark%d-url.com", index);
    const BookmarkNode* profile0_bookmark = profile0_bookmark_model->AddURL(
        profile0_bookmark_bar, index, title, GURL(url));
    ASSERT_TRUE(profile0_bookmark != NULL);
    const BookmarkNode* profile1_bookmark = profile1_bookmark_model->AddURL(
        profile1_bookmark_bar, index, title, GURL(url));
    ASSERT_TRUE(profile1_bookmark != NULL);
  }

  // Let's add same bookmark folders and bookmarks in those on both clients.
  for (int index = 45; index < 50; index++) {
    string16 title = ASCIIToUTF16(StringPrintf("Folder%d_name", index));
    const BookmarkNode* profile0_folder = profile0_bookmark_model->AddGroup(
        profile0_bookmark_bar, index, title);
    ASSERT_TRUE(profile0_folder != NULL);
    const BookmarkNode* profile1_folder = profile1_bookmark_model->AddGroup(
        profile1_bookmark_bar, index, title);
    ASSERT_TRUE(profile1_folder != NULL);
    // Let's add same bookmarks in the folders created above.
    for (int index = 0; index < 5; index++) {
      string16 child_title = title +
          ASCIIToUTF16(StringPrintf("-bookmark%d_name", index));
      string url = StringPrintf("http://www.folder-bookmark%d-url.com", index);
      const BookmarkNode* profile0_bookmark2 = profile0_bookmark_model->AddURL(
          profile0_folder, index, child_title, GURL(url));
      ASSERT_TRUE(profile0_bookmark2 != NULL);
      const BookmarkNode* profile1_bookmark2 = profile1_bookmark_model->AddURL(
          profile1_folder, index, child_title, GURL(url));
      ASSERT_TRUE(profile1_bookmark2 != NULL);
    }
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  BookmarkModelVerifier::VerifyNoDuplicates(profile0_bookmark_model);
  BookmarkModelVerifier::ExpectModelsMatch(profile0_bookmark_model,
                                           profile1_bookmark_model);
}

// Test Scribe ID - 386591.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_MergeSimpleBMHierarchySubsetUnderBMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  BookmarkModel* profile0_bookmark_model = GetBookmarkModel(0);
  BookmarkModel* profile1_bookmark_model = GetBookmarkModel(1);

  const BookmarkNode* profile0_bookmark_bar =
      profile0_bookmark_model->GetBookmarkBarNode();
  const BookmarkNode* profile1_bookmark_bar =
      profile1_bookmark_model->GetBookmarkBarNode();

  {
    // Let's create the hierarchy in profile0.
    const BookmarkNode* profile0_folder = profile0_bookmark_model->AddGroup(
        profile0_bookmark_bar, 0, ASCIIToUTF16("Folder_name"));
    ASSERT_TRUE(profile0_folder != NULL);
    const BookmarkNode* profile0_folder0 = profile0_bookmark_model->AddGroup(
        profile0_folder, 0, ASCIIToUTF16("Folder0_name"));
    ASSERT_TRUE(profile0_folder0 != NULL);
    const BookmarkNode* profile0_bookmark1 = profile0_bookmark_model->AddURL(
        profile0_folder0, 0, ASCIIToUTF16("bookmark1_name"),
        GURL("http://www.bookmark1-url.com"));
    ASSERT_TRUE(profile0_bookmark1 != NULL);
    const BookmarkNode* profile0_folder2 = profile0_bookmark_model->AddGroup(
        profile0_folder, 1, ASCIIToUTF16("Folder2_name"));
    ASSERT_TRUE(profile0_folder2 != NULL);
    const BookmarkNode* profile0_bookmark3 = profile0_bookmark_model->AddURL(
        profile0_folder2, 0, ASCIIToUTF16("bookmark3_name"),
        GURL("http://www.bookmark3-url.com"));
    ASSERT_TRUE(profile0_bookmark3 != NULL);
    const BookmarkNode* profile0_folder4 = profile0_bookmark_model->AddGroup(
        profile0_folder, 2, ASCIIToUTF16("Folder4_name"));
    ASSERT_TRUE(profile0_folder4 != NULL);
    const BookmarkNode* profile0_bookmark5 = profile0_bookmark_model->AddURL(
        profile0_folder4, 0, ASCIIToUTF16("bookmark1_name"),
        GURL("http://www.bookmark1-url.com"));
    ASSERT_TRUE(profile0_bookmark5 != NULL);

    // Let's create the hierarchy for profile1.
    const BookmarkNode* profile1_folder = profile1_bookmark_model->AddGroup(
        profile1_bookmark_bar, 0, ASCIIToUTF16("Folder_name"));
    ASSERT_TRUE(profile1_folder != NULL);
    const BookmarkNode* profile1_folder0 = profile1_bookmark_model->AddGroup(
        profile1_folder, 0, ASCIIToUTF16("Folder0_name"));
    ASSERT_TRUE(profile1_folder0 != NULL);
    const BookmarkNode* profile1_folder2 = profile1_bookmark_model->AddGroup(
        profile1_folder, 1, ASCIIToUTF16("Folder2_name"));
    ASSERT_TRUE(profile1_folder2 != NULL);
    const BookmarkNode* profile1_bookmark3 = profile1_bookmark_model->AddURL(
        profile1_folder2, 0, ASCIIToUTF16("bookmark3_name"),
        GURL("http://www.bookmark3-url.com"));
    ASSERT_TRUE(profile1_bookmark3 != NULL);
    const BookmarkNode* profile1_folder4 = profile1_bookmark_model->AddGroup(
        profile1_folder, 2, ASCIIToUTF16("Folder4_name"));
    ASSERT_TRUE(profile1_folder4 != NULL);
    const BookmarkNode* profile1_bookmark5 = profile1_bookmark_model->AddURL(
        profile1_folder4, 0, ASCIIToUTF16("bookmark1_name"),
        GURL("http://www.bookmark1-url.com"));
    ASSERT_TRUE(profile1_bookmark5 != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));
  BookmarkModelVerifier::ExpectModelsMatch(profile0_bookmark_model,
                                           profile1_bookmark_model);
}

