// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;
using bookmarks_helper::AllModelsMatchVerifier;
using bookmarks_helper::ContainsDuplicateBookmarks;
using bookmarks_helper::CountBookmarksWithTitlesMatching;
using bookmarks_helper::CreateFavicon;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::GetOtherNode;
using bookmarks_helper::GetUniqueNodeByURL;
using bookmarks_helper::IndexedFolderName;
using bookmarks_helper::IndexedSubfolderName;
using bookmarks_helper::IndexedSubsubfolderName;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using bookmarks_helper::ReverseChildOrder;
using bookmarks_helper::SetFavicon;
using bookmarks_helper::SetTitle;
using bookmarks_helper::SetURL;
using bookmarks_helper::SortChildren;

const std::string kGenericURL = "http://www.host.ext:1234/path/filename";
const std::wstring kGenericURLTitle = L"URL Title";
const std::wstring kGenericFolderName = L"Folder Name";
const std::wstring kGenericSubfolderName = L"Subfolder Name";
const std::wstring kGenericSubsubfolderName = L"Subsubfolder Name";

class TwoClientBookmarksSyncTest : public SyncTest {
 public:
  TwoClientBookmarksSyncTest() : SyncTest(TWO_CLIENT) {}
  virtual ~TwoClientBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientBookmarksSyncTest);
};

const std::vector<unsigned char> GenericFavicon() {
  return CreateFavicon(254);
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL google_url("http://www.google.com");
  ASSERT_TRUE(AddURL(0, L"Google", google_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AddURL(1, L"Yahoo", GURL("http://www.yahoo.com")) != NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* new_folder = AddFolder(0, 2, L"New Folder");
  Move(0, GetUniqueNodeByURL(0, google_url), new_folder, 0);
  SetTitle(0, GetBookmarkBarNode(0)->GetChild(0), L"Yahoo!!");
  ASSERT_TRUE(AddURL(0, GetBookmarkBarNode(0), 1, L"CNN",
      GURL("http://www.cnn.com")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(1, L"Facebook", GURL("http://www.facebook.com")) != NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  SortChildren(1, GetBookmarkBarNode(1));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  DisableVerifier();
  SetTitle(0, GetUniqueNodeByURL(0, google_url), L"Google++");
  SetTitle(1, GetUniqueNodeByURL(1, google_url), L"Google--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SimultaneousURLChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL initial_url("http://www.google.com");
  GURL second_url("http://www.google.com/abc");
  GURL third_url("http://www.google.com/def");
  std::wstring title = L"Google";

  ASSERT_TRUE(AddURL(0, title, initial_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  DisableVerifier();
  ASSERT_TRUE(SetURL(
      0, GetUniqueNodeByURL(0, initial_url), second_url) != NULL);
  ASSERT_TRUE(SetURL(
      1, GetUniqueNodeByURL(1, initial_url), third_url) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  SetTitle(0, GetBookmarkBarNode(0)->GetChild(0), L"Google1");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370558.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddFirstFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370559.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_AddFirstBMWithoutFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370489.
// TODO(rsimha): Enable after http://crbug.com/94941 is fixed.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       FAILS_SC_AddFirstBMWithFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, GURL(kGenericURL));
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  SetFavicon(0, bookmark, GenericFavicon());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370560.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddNonHTTPBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(
      0, L"FTP URL", GURL("ftp://user:password@host:1234/path")) != NULL);
  ASSERT_TRUE(AddURL(0, L"File URL", GURL("file://host/path")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370561.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_AddFirstBMUnderFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  ASSERT_TRUE(AddURL(
      0, folder, 0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370562.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_AddSeveralBMsUnderBMBarAndOtherBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 20; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    ASSERT_TRUE(AddURL(
        0, GetOtherNode(0), i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370563.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_AddSeveralBMsAndFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    } else {
      std::wstring title = IndexedFolderName(i);
      const BookmarkNode* folder = AddFolder(0, i, title);
      ASSERT_TRUE(folder != NULL);
      if (base::RandDouble() > 0.4) {
        for (int i = 0; i < 20; ++i) {
          std::wstring title = IndexedURLTitle(i);
          GURL url = GURL(IndexedURL(i));
          ASSERT_TRUE(
              AddURL(0, folder, i, title, url) != NULL);
        }
      }
    }
  }
  for (int i = 0; i < 10; i++) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, GetOtherNode(0), i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370641.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DuplicateBMWithDifferentURLSameName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url0 = GURL(IndexedURL(0));
  GURL url1 = GURL(IndexedURL(1));
  ASSERT_TRUE(AddURL(0, kGenericURLTitle, url0) != NULL);
  ASSERT_TRUE(AddURL(0, kGenericURLTitle, url1) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370639 - Add bookmarks with different name and same URL.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DuplicateBookmarksWithSameURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring title0 = IndexedURLTitle(0);
  std::wstring title1 = IndexedURLTitle(1);
  ASSERT_TRUE(AddURL(0, title0, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(0, title1, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371817.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, GURL(kGenericURL));
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring new_title = IndexedURLTitle(2);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371822.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, url);
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL new_url = GURL(IndexedURL(2));
  ASSERT_TRUE(SetURL(0, bookmark, new_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}


// Test Scribe ID - 371818 - Renaming the same bookmark name twice.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_TwiceRenamingBookmarkName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, GURL(kGenericURL));
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring new_title = IndexedURLTitle(2);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  SetTitle(0, bookmark, title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371823 - Renaming the same bookmark URL twice.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_TwiceRenamingBookmarkURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, url);
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL new_url = GURL(IndexedURL(2));
  ASSERT_TRUE(SetURL(0, bookmark, new_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(SetURL(0, bookmark, url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371824.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_TRUE(AddURL(
      0, folder, 0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371825.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameEmptyBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371826.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_RenameBMFolderWithLongHierarchy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    } else {
      std::wstring title = IndexedSubfolderName(i);
      ASSERT_TRUE(AddFolder(0, folder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371827.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_RenameBMFolderThatHasParentAndChildren) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 1; i < 15; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }
  std::wstring title = IndexedSubfolderName(1);
  const BookmarkNode* subfolder = AddFolder(0, folder, 0, title);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, subfolder, i, title, url) != NULL);
    } else {
      std::wstring title = IndexedSubsubfolderName(i);
      ASSERT_TRUE(AddFolder(0, subfolder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::wstring new_title = IndexedSubfolderName(2);
  SetTitle(0, subfolder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371828.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMNameAndURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  std::wstring title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, url);
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL new_url = GURL(IndexedURL(2));
  std::wstring new_title = IndexedURLTitle(2);
  bookmark = SetURL(0, bookmark, new_url);
  ASSERT_TRUE(bookmark != NULL);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371832.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DeleteBMEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(
      0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371833.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 20; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371835.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelFirstBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, folder, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371836.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelLastBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, folder, folder->child_count() - 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371856.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelMiddleBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, folder, 4);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371857.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMsUnderBMFoldEmptyFolderAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  int child_count = folder->child_count();
  for (int i = 0; i < child_count; ++i) {
    Remove(0, folder, 0);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371858.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelEmptyBMFoldEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371869.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelEmptyBMFoldNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  for (int i = 1; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    } else {
      std::wstring title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371879.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMFoldWithBMsNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    } else {
      std::wstring title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  for (int i = 0; i < 15; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetBookmarkBarNode(0), 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371880.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMFoldWithBMsAndBMFoldsNonEmptyACAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    } else {
      std::wstring title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  for (int i = 0; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    } else {
      std::wstring title = IndexedSubfolderName(i);
      const BookmarkNode* subfolder =
          AddFolder(0, folder, i, title);
      ASSERT_TRUE(subfolder != NULL);
      if (base::RandDouble() > 0.3) {
        for (int j = 0; j < 10; ++j) {
          if (base::RandDouble() > 0.6) {
            std::wstring title = IndexedURLTitle(j);
            GURL url = GURL(IndexedURL(j));
            ASSERT_TRUE(AddURL(
                0, subfolder, j, title, url) != NULL);
          } else {
            std::wstring title = IndexedSubsubfolderName(j);
            ASSERT_TRUE(AddFolder(
                0, subfolder, j, title) != NULL);
          }
        }
      }
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetBookmarkBarNode(0), 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371882.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMFoldWithParentAndChildrenBMsAndBMFolds) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 1; i < 11; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 0, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 30; ++i) {
    if (base::RandDouble() > 0.2) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, subfolder, i, title, url) != NULL);
    } else {
      std::wstring title = IndexedSubsubfolderName(i);
      ASSERT_TRUE(AddFolder(0, subfolder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, folder, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371931.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_ReverseTheOrderOfTwoBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url0 = GURL(IndexedURL(0));
  GURL url1 = GURL(IndexedURL(1));
  std::wstring title0 = IndexedURLTitle(0);
  std::wstring title1 = IndexedURLTitle(1);
  const BookmarkNode* bookmark0 = AddURL(0, 0, title0, url0);
  const BookmarkNode* bookmark1 = AddURL(0, 1, title1, url1);
  ASSERT_TRUE(bookmark0 != NULL);
  ASSERT_TRUE(bookmark1 != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Move(0, bookmark0, GetBookmarkBarNode(0), 2);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371933.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_ReverseTheOrderOf10BMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371954.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_MovingBMsFromBMBarToBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  int num_bookmarks_to_move =
      GetBookmarkBarNode(0)->child_count() - 2;
  for (int i = 0; i < num_bookmarks_to_move; ++i) {
    Move(
        0, GetBookmarkBarNode(0)->GetChild(2), folder, i);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(AllModelsMatchVerifier());
  }
}

// Test Scribe ID - 371957.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_MovingBMsFromBMFoldToBMBar) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  int num_bookmarks_to_move = folder->child_count() - 2;
  for (int i = 0; i < num_bookmarks_to_move; ++i) {
    Move(0, folder->GetChild(0), GetBookmarkBarNode(0), i);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(AllModelsMatchVerifier());
  }
}

// Test Scribe ID - 371961.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_MovingBMsFromParentBMFoldToChildBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i + 3);
    GURL url = GURL(IndexedURL(i + 3));
    ASSERT_TRUE(AddURL(0, subfolder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 3; ++i) {
    GURL url = GURL(IndexedURL(i));
    Move(0, GetUniqueNodeByURL(0, url), subfolder, i + 10);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371964.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_MovingBMsFromChildBMFoldToParentBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 5; ++i) {
    std::wstring title = IndexedURLTitle(i + 3);
    GURL url = GURL(IndexedURL(i + 3));
    ASSERT_TRUE(AddURL(0, subfolder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 3; ++i) {
    GURL url = GURL(IndexedURL(i + 3));
    Move(0, GetUniqueNodeByURL(0, url), folder, i + 4);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371967.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_HoistBMs10LevelUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L0 = NULL;
  const BookmarkNode* folder_L10 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::wstring title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 0) folder_L0 = folder;
    if (level == 10) folder_L10 = folder;
  }
  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i + 10);
    GURL url = GURL(IndexedURL(i + 10));
    ASSERT_TRUE(AddURL(0, folder_L10, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(
      0, url10), folder_L0, folder_L0->child_count());
  GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L0, 0);
  GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L0, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371968.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_SinkBMs10LevelDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L0 = NULL;
  const BookmarkNode* folder_L10 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::wstring title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 0) folder_L0 = folder;
    if (level == 10) folder_L10 = folder;
  }
  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i + 10);
    GURL url = GURL(IndexedURL(i + 10));
    ASSERT_TRUE(AddURL(0, folder_L0, 0, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(0, url10), folder_L10, folder_L10->child_count());
  GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L10, 0);
  GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L10, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371980.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_SinkEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::wstring title = IndexedFolderName(level);
    folder = AddFolder(
        0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5) folder_L5 = folder;
  }
  folder = AddFolder(
      0, GetBookmarkBarNode(0)->child_count(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Move(0, folder, folder_L5, folder_L5->child_count());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371997.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_SinkNonEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = NULL;
  for (int level = 0; level < 6; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::wstring title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5) folder_L5 = folder;
  }
  folder = AddFolder(
      0, GetBookmarkBarNode(0)->child_count(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Move(0, folder, folder_L5, folder_L5->child_count());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 372006.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_HoistFolder5LevelsUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = NULL;
  for (int level = 0; level < 6; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::wstring title = IndexedFolderName(level);
    folder = AddFolder(
        0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5) folder_L5 = folder;
  }
  folder = AddFolder(
      0, folder_L5, folder_L5->child_count(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Move(0, folder, GetBookmarkBarNode(0), GetBookmarkBarNode(0)->child_count());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 372026.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_ReverseTheOrderOfTwoBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 2; ++i) {
    std::wstring title = IndexedFolderName(i);
    const BookmarkNode* folder = AddFolder(0, i, title);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 10; ++j) {
      std::wstring title = IndexedURLTitle(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddURL(0, folder, j, title, url) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 372028.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_ReverseTheOrderOfTenBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 10; ++i) {
    std::wstring title = IndexedFolderName(i);
    const BookmarkNode* folder = AddFolder(0, i, title);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 10; ++j) {
      std::wstring title = IndexedURLTitle(1000 * i + j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddURL(0, folder, j, title, url) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 373379.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BiDirectionalPushAddingBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  DisableVerifier();
  for (int i = 0; i < 2; ++i) {
    std::wstring title0 = IndexedURLTitle(2*i);
    GURL url0 = GURL(IndexedURL(2*i));
    ASSERT_TRUE(AddURL(0, title0, url0) != NULL);
    std::wstring title1 = IndexedURLTitle(2*i+1);
    GURL url1 = GURL(IndexedURL(2*i+1));
    ASSERT_TRUE(AddURL(1, title1, url1) != NULL);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test Scribe ID - 373503.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BiDirectionalPush_AddingSameBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Note: When a racy commit is done with identical bookmarks, it is possible
  // for duplicates to exist after sync completes. See http://crbug.com/19769.
  DisableVerifier();
  for (int i = 0; i < 2; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, title, url) != NULL);
    ASSERT_TRUE(AddURL(1, title, url) != NULL);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 373506.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BootStrapEmptyStateEverywhere) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 373505.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_Merge_CaseInsensitivity_InNames) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const BookmarkNode* folder0 = AddFolder(0, L"Folder");
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddURL(0, folder0, 0, L"Bookmark 0", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(0, folder0, 1, L"Bookmark 1", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(0, folder0, 2, L"Bookmark 2", GURL(kGenericURL)) != NULL);

  const BookmarkNode* folder1 = AddFolder(1, L"fOlDeR");
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddURL(1, folder1, 0, L"bOoKmArK 0", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(1, folder1, 1, L"BooKMarK 1", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(1, folder1, 2, L"bOOKMARK 2", GURL(kGenericURL)) != NULL);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test Scribe ID - 373508.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_SimpleMergeOfDifferentBMModels) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    ASSERT_TRUE(AddURL(1, i, title, url) != NULL);
  }

  for (int i = 3; i < 10; ++i) {
    std::wstring title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title0, url0) != NULL);
    std::wstring title1 = IndexedURLTitle(i+7);
    GURL url1 = GURL(IndexedURL(i+7));
    ASSERT_TRUE(AddURL(1, i, title1, url1) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test Scribe ID - 386586.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeSimpleBMHierarchyUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    ASSERT_TRUE(AddURL(1, i, title, url) != NULL);
  }

  for (int i = 3; i < 10; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(1, i, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test Scribe ID - 386589.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeSimpleBMHierarchyEqualSetsUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    ASSERT_TRUE(AddURL(1, i, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test Scribe ID - 373504 - Merge bookmark folders with different bookmarks.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeBMFoldersWithDifferentBMs) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  const BookmarkNode* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  for (int i = 0; i < 2; ++i) {
    std::wstring title0 = IndexedURLTitle(2*i);
    GURL url0 = GURL(IndexedURL(2*i));
    ASSERT_TRUE(AddURL(0, folder0, i, title0, url0) != NULL);
    std::wstring title1 = IndexedURLTitle(2*i+1);
    GURL url1 = GURL(IndexedURL(2*i+1));
    ASSERT_TRUE(AddURL(1, folder1, i, title1, url1) != NULL);
  }
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test Scribe ID - 373509 - Merge moderately complex bookmark models.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeDifferentBMModelsModeratelyComplex) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 25; ++i) {
    std::wstring title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title0, url0) != NULL);
    std::wstring title1 = IndexedURLTitle(i+50);
    GURL url1 = GURL(IndexedURL(i+50));
    ASSERT_TRUE(AddURL(1, i, title1, url1) != NULL);
  }
  for (int i = 25; i < 30; ++i) {
    std::wstring title0 = IndexedFolderName(i);
    const BookmarkNode* folder0 = AddFolder(0, i, title0);
    ASSERT_TRUE(folder0 != NULL);
    std::wstring title1 = IndexedFolderName(i+50);
    const BookmarkNode* folder1 = AddFolder(1, i, title1);
    ASSERT_TRUE(folder1 != NULL);
    for (int j = 0; j < 5; ++j) {
      std::wstring title0 = IndexedURLTitle(i+5*j);
      GURL url0 = GURL(IndexedURL(i+5*j));
      ASSERT_TRUE(AddURL(0, folder0, j, title0, url0) != NULL);
      std::wstring title1 = IndexedURLTitle(i+5*j+50);
      GURL url1 = GURL(IndexedURL(i+5*j+50));
      ASSERT_TRUE(AddURL(1, folder1, j, title1, url1) != NULL);
    }
  }
  for (int i = 100; i < 125; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, title, url) != NULL);
    ASSERT_TRUE(AddURL(1, title, url) != NULL);
  }
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// TCM ID - 3675271 - Merge simple bookmark subset under bookmark folder.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeSimpleBMHierarchySubsetUnderBMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 2; ++i) {
    const BookmarkNode* folder = AddFolder(i, kGenericFolderName);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 4; ++j) {
      if (base::RandDouble() < 0.5) {
        std::wstring title = IndexedURLTitle(j);
        GURL url = GURL(IndexedURL(j));
        ASSERT_TRUE(AddURL(i, folder, j, title, url) != NULL);
      } else {
        std::wstring title = IndexedFolderName(j);
        ASSERT_TRUE(AddFolder(i, folder, j, title) != NULL);
      }
    }
  }
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// TCM ID - 3727284 - Merge subsets of bookmark under bookmark bar.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeSimpleBMHierarchySubsetUnderBookmarkBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 4; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }

  for (int j = 0; j < 2; ++j) {
    std::wstring title = IndexedURLTitle(j);
    GURL url = GURL(IndexedURL(j));
    ASSERT_TRUE(AddURL(1, j, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
  ASSERT_FALSE(ContainsDuplicateBookmarks(1));
}

// TCM ID - 3659294 - Merge simple bookmark hierarchy under bookmark folder.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_Merge_SimpleBMHierarchy_Under_BMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const BookmarkNode* folder0 = AddFolder(0, 0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddURL(
      0, folder0, 0, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddURL(
      0, folder0, 2, IndexedURLTitle(3), GURL(IndexedURL(3))) != NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 3, IndexedSubfolderName(4)) != NULL);

  const BookmarkNode* folder1 = AddFolder(1, 0, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 0, IndexedSubfolderName(0)) != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddURL(
      1, folder1, 2, IndexedURLTitle(3), GURL(IndexedURL(3))) != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 3, IndexedSubfolderName(5)) != NULL);
  ASSERT_TRUE(AddURL(
      1, folder1, 4, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// TCM ID - 3711273 - Merge disjoint sets of bookmark hierarchy under bookmark
// folder.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_Merge_SimpleBMHierarchy_DisjointSets_Under_BMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const BookmarkNode* folder0 =
      AddFolder(0, 0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddURL(
      0, folder0, 0, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddURL(
      0, folder0, 2, IndexedURLTitle(3), GURL(IndexedURL(3))) != NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 3, IndexedSubfolderName(4)) != NULL);

  const BookmarkNode* folder1 = AddFolder(1, 0, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 0, IndexedSubfolderName(5)) != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 1, IndexedSubfolderName(6)) != NULL);
  ASSERT_TRUE(AddURL(
      1, folder1, 2, IndexedURLTitle(7), GURL(IndexedURL(7))) != NULL);
  ASSERT_TRUE(AddURL(
      1, folder1, 3, IndexedURLTitle(8), GURL(IndexedURL(8))) != NULL);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// TCM ID - 3639296 - Merge disjoint sets of bookmark hierarchy under bookmark
// bar.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
    MC_Merge_SimpleBMHierarchy_DisjointSets_Under_BookmarkBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i+1);
    GURL url = GURL(IndexedURL(i+1));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }

  for (int j = 0; j < 3; ++j) {
    std::wstring title = IndexedURLTitle(j+4);
    GURL url = GURL(IndexedURL(j+4));
    ASSERT_TRUE(AddURL(0, j, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// TCM ID - 3616282 - Merge sets of duplicate bookmarks under bookmark bar.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_Merge_SimpleBMHierarchy_DuplicateBMs_Under_BMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  // Let's add duplicate set of bookmark {1,2,2,3,3,3,4,4,4,4} to client0.
  int node_index = 0;
  for (int i = 1; i < 5 ; ++i) {
    for (int j = 0; j < i; ++j) {
      std::wstring title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, node_index, title, url) != NULL);
      ++node_index;
    }
  }
  // Let's add a set of bookmarks {1,2,3,4} to client1.
  for (int i = 0; i < 4; ++i) {
    std::wstring title = IndexedURLTitle(i+1);
    GURL url = GURL(IndexedURL(i+1));
    ASSERT_TRUE(AddURL(1, i, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 1; i < 5 ; ++i) {
    ASSERT_TRUE(CountBookmarksWithTitlesMatching(1, IndexedURLTitle(i)) == i);
  }
}

// TCM ID - 6593872.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, DisableBookmarks) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncable::BOOKMARKS));
  ASSERT_TRUE(AddFolder(1, kGenericFolderName) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncable::BOOKMARKS));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// TCM ID - 7343544.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  ASSERT_TRUE(AddFolder(0, IndexedFolderName(0)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Added a folder."));
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(AddFolder(1, IndexedFolderName(1)) != NULL);
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// TCM ID - 3662298 - Test adding duplicate folder - Both with different BMs
// underneath.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, MC_DuplicateFolders) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  const BookmarkNode* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  for (int i = 0; i < 5; ++i) {
    std::wstring title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder0, i, title0, url0) != NULL);
    std::wstring title1 = IndexedURLTitle(i+5);
    GURL url1 = GURL(IndexedURL(i+5));
    ASSERT_TRUE(AddURL(1, folder1, i, title1, url1) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// TCM ID - 3719307 - Test a scenario of updating the name of the same bookmark
// from two clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BookmarkNameChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  DisableVerifier();
  GURL url(IndexedURL(0));
  SetTitle(0, GetUniqueNodeByURL(0, url), L"Title++");
  SetTitle(1, GetUniqueNodeByURL(1, url), L"Title--");

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// TCM ID - 3672299 - Test a scenario of updating the URL of the same bookmark
// from two clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BookmarkURLChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  DisableVerifier();
  GURL url(IndexedURL(0));
  ASSERT_TRUE(SetURL(
      0, GetUniqueNodeByURL(0, url), GURL("http://www.google.com/00")));
  ASSERT_TRUE(SetURL(
      1, GetUniqueNodeByURL(1, url), GURL("http://www.google.com/11")));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// TCM ID - 3699290 - Test a scenario of updating the BM Folder name from two
// clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_FolderNameChangeConflict) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const BookmarkNode* folderA[2];
  const BookmarkNode* folderB[2];
  const BookmarkNode* folderC[2];

  // Create empty folder A on both clients.
  folderA[0] = AddFolder(0, IndexedFolderName(0));
  ASSERT_TRUE(folderA[0] != NULL);
  folderA[1] = AddFolder(1, IndexedFolderName(0));
  ASSERT_TRUE(folderA[1] != NULL);

  // Create folder B with bookmarks on both clients.
  folderB[0] = AddFolder(0, IndexedFolderName(1));
  ASSERT_TRUE(folderB[0] != NULL);
  folderB[1] = AddFolder(1, IndexedFolderName(1));
  ASSERT_TRUE(folderB[1] != NULL);
  for (int i = 0; i < 3; ++i) {
    std::wstring title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folderB[0], i, title, url) != NULL);
  }

  // Create folder C with bookmarks and subfolders on both clients.
  folderC[0] = AddFolder(0, IndexedFolderName(2));
  ASSERT_TRUE(folderC[0] != NULL);
  folderC[1] = AddFolder(1, IndexedFolderName(2));
  ASSERT_TRUE(folderC[1] != NULL);
  for (int i = 0; i < 3; ++i) {
    std::wstring folder_name = IndexedSubfolderName(i);
    const BookmarkNode* subfolder = AddFolder(0, folderC[0], i, folder_name);
    ASSERT_TRUE(subfolder != NULL);
    for (int j = 0; j < 3; ++j) {
      std::wstring title = IndexedURLTitle(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddURL(0, subfolder, j, title, url) != NULL);
    }
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder A on both clients.
  SetTitle(0, folderA[0], L"Folder A++");
  SetTitle(1, folderA[1], L"Folder A--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder B on both clients.
  SetTitle(0, folderB[0], L"Folder B++");
  SetTitle(1, folderB[1], L"Folder B--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder C on both clients.
  SetTitle(0, folderC[0], L"Folder C++");
  SetTitle(1, folderC[1], L"Folder C--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0, syncable::BOOKMARKS));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncrypted(0, syncable::BOOKMARKS));
  ASSERT_TRUE(IsEncrypted(1, syncable::BOOKMARKS));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0, syncable::BOOKMARKS));
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncrypted(0, syncable::BOOKMARKS));
  ASSERT_TRUE(IsEncrypted(1, syncable::BOOKMARKS));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0, syncable::BOOKMARKS));
  ASSERT_TRUE(EnableEncryption(1, syncable::BOOKMARKS));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncrypted(0, syncable::BOOKMARKS));
  ASSERT_TRUE(IsEncrypted(1, syncable::BOOKMARKS));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SingleClientEnabledEncryptionBothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0, syncable::BOOKMARKS));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncrypted(0, syncable::BOOKMARKS));
  ASSERT_TRUE(IsEncrypted(1, syncable::BOOKMARKS));
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_TRUE(IsEncrypted(0, syncable::BOOKMARKS));
  ASSERT_TRUE(IsEncrypted(1, syncable::BOOKMARKS));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SingleClientEnabledEncryptionAndChangedMultipleTimes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(EnableEncryption(0, syncable::BOOKMARKS));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncrypted(0, syncable::BOOKMARKS));
  ASSERT_TRUE(IsEncrypted(1, syncable::BOOKMARKS));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AddFolder(0, IndexedFolderName(0)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// TODO(zea): Add first time sync functionality testing. In particular, when
// there are encrypted types in first time sync we need to ensure we don't
// duplicate bookmarks.
