// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "ui/base/layout.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;
using bookmarks_helper::AllModelsMatchVerifier;
using bookmarks_helper::ContainsDuplicateBookmarks;
using bookmarks_helper::CountBookmarksWithTitlesMatching;
using bookmarks_helper::CreateFavicon;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::GetOtherNode;
using bookmarks_helper::GetSyncedBookmarksNode;
using bookmarks_helper::GetUniqueNodeByURL;
using bookmarks_helper::HasNodeWithURL;
using bookmarks_helper::IndexedFolderName;
using bookmarks_helper::IndexedSubfolderName;
using bookmarks_helper::IndexedSubsubfolderName;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using bookmarks_helper::RemoveAll;
using bookmarks_helper::ReverseChildOrder;
using bookmarks_helper::SetFavicon;
using bookmarks_helper::SetTitle;
using bookmarks_helper::SetURL;
using bookmarks_helper::SortChildren;
using passwords_helper::SetDecryptionPassphrase;
using passwords_helper::SetEncryptionPassphrase;
using sync_integration_test_util::AwaitCommitActivityCompletion;
using sync_integration_test_util::AwaitPassphraseAccepted;
using sync_integration_test_util::AwaitPassphraseRequired;

const std::string kGenericURL = "http://www.host.ext:1234/path/filename";
const std::string kGenericURLTitle = "URL Title";
const std::string kGenericFolderName = "Folder Name";
const std::string kGenericSubfolderName = "Subfolder Name";
const std::string kGenericSubsubfolderName = "Subsubfolder Name";
const char* kValidPassphrase = "passphrase!";

class TwoClientBookmarksSyncTest : public SyncTest {
 public:
  TwoClientBookmarksSyncTest() : SyncTest(TWO_CLIENT) {}
  virtual ~TwoClientBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientBookmarksSyncTest);
};

class LegacyTwoClientBookmarksSyncTest : public SyncTest {
 public:
  LegacyTwoClientBookmarksSyncTest() : SyncTest(TWO_CLIENT_LEGACY) {}
  virtual ~LegacyTwoClientBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyTwoClientBookmarksSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL google_url("http://www.google.com");
  ASSERT_TRUE(AddURL(0, "Google", google_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AddURL(1, "Yahoo", GURL("http://www.yahoo.com")) != NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* new_folder = AddFolder(0, 2, "New Folder");
  Move(0, GetUniqueNodeByURL(0, google_url), new_folder, 0);
  SetTitle(0, GetBookmarkBarNode(0)->GetChild(0), "Yahoo!!");
  ASSERT_TRUE(AddURL(0, GetBookmarkBarNode(0), 1, "CNN",
      GURL("http://www.cnn.com")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(1, "Facebook", GURL("http://www.facebook.com")) != NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  SortChildren(1, GetBookmarkBarNode(1));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  DisableVerifier();
  SetTitle(0, GetUniqueNodeByURL(0, google_url), "Google++");
  SetTitle(1, GetUniqueNodeByURL(1, google_url), "Google--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SimultaneousURLChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL initial_url("http://www.google.com");
  GURL second_url("http://www.google.com/abc");
  GURL third_url("http://www.google.com/def");
  std::string title = "Google";

  ASSERT_TRUE(AddURL(0, title, initial_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  DisableVerifier();
  ASSERT_TRUE(SetURL(
      0, GetUniqueNodeByURL(0, initial_url), second_url) != NULL);
  ASSERT_TRUE(SetURL(
      1, GetUniqueNodeByURL(1, initial_url), third_url) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  SetTitle(0, GetBookmarkBarNode(0)->GetChild(0), "Google1");
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
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddFirstBMWithFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const GURL page_url(kGenericURL);
  const GURL icon_url("http://www.google.com/favicon.ico");

  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, page_url);

  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  SetFavicon(0, bookmark, icon_url, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test that the history service logic for not losing the hidpi versions of
// favicons as a result of sync does not result in dropping sync updates.
// In particular, the synced 16x16 favicon bitmap should overwrite 16x16
// favicon bitmaps on all clients. (Though non-16x16 favicon bitmaps
// are unchanged).
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_SetFaviconHiDPI) {
  // Set the supported scale factors to include 2x such that CreateFavicon()
  // creates a favicon with hidpi representations and that methods in the
  // FaviconService request hidpi favicons.
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::SetSupportedScaleFactors(supported_scale_factors);

  const GURL page_url(kGenericURL);
  const GURL icon_url1("http://www.google.com/favicon1.ico");
  const GURL icon_url2("http://www.google.com/favicon2.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* bookmark0 = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_TRUE(bookmark0 != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  SetFavicon(0, bookmark0, icon_url1, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* bookmark1 = GetUniqueNodeByURL(1, page_url);
  SetFavicon(1, bookmark1, icon_url1, CreateFavicon(SK_ColorBLUE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  SetFavicon(0, bookmark0, icon_url2, CreateFavicon(SK_ColorGREEN),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370560.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddNonHTTPBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(
      0, "FTP UR", GURL("ftp://user:password@host:1234/path")) != NULL);
  ASSERT_TRUE(AddURL(0, "File UR", GURL("file://host/path")) != NULL);
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
    std::string title = IndexedURLTitle(i);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      const BookmarkNode* folder = AddFolder(0, i, title);
      ASSERT_TRUE(folder != NULL);
      if (base::RandDouble() > 0.4) {
        for (int i = 0; i < 20; ++i) {
          std::string title = IndexedURLTitle(i);
          GURL url = GURL(IndexedURL(i));
          ASSERT_TRUE(
              AddURL(0, folder, i, title, url) != NULL);
        }
      }
    }
  }
  for (int i = 0; i < 10; i++) {
    std::string title = IndexedURLTitle(i);
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

  std::string title0 = IndexedURLTitle(0);
  std::string title1 = IndexedURLTitle(1);
  ASSERT_TRUE(AddURL(0, title0, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(0, title1, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371817.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, GURL(kGenericURL));
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedURLTitle(2);
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

  std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, GURL(kGenericURL));
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedURLTitle(2);
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

  std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_TRUE(AddURL(
      0, folder, 0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371825.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameEmptyBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371826.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_RenameBMFolderWithLongHierarchy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    } else {
      std::string title = IndexedSubfolderName(i);
      ASSERT_TRUE(AddFolder(0, folder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedFolderName(2);
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
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }
  std::string title = IndexedSubfolderName(1);
  const BookmarkNode* subfolder = AddFolder(0, folder, 0, title);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, subfolder, i, title, url) != NULL);
    } else {
      std::string title = IndexedSubsubfolderName(i);
      ASSERT_TRUE(AddFolder(0, subfolder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedSubfolderName(2);
  SetTitle(0, subfolder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371828.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMNameAndURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, url);
  ASSERT_TRUE(bookmark != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL new_url = GURL(IndexedURL(2));
  std::string new_title = IndexedURLTitle(2);
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
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  for (int i = 0; i < 15; ++i) {
    std::string title = IndexedURLTitle(i);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  for (int i = 0; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    } else {
      std::string title = IndexedSubfolderName(i);
      const BookmarkNode* subfolder =
          AddFolder(0, folder, i, title);
      ASSERT_TRUE(subfolder != NULL);
      if (base::RandDouble() > 0.3) {
        for (int j = 0; j < 10; ++j) {
          if (base::RandDouble() > 0.6) {
            std::string title = IndexedURLTitle(j);
            GURL url = GURL(IndexedURL(j));
            ASSERT_TRUE(AddURL(
                0, subfolder, j, title, url) != NULL);
          } else {
            std::string title = IndexedSubsubfolderName(j);
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
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 0, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 30; ++i) {
    if (base::RandDouble() > 0.2) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, subfolder, i, title, url) != NULL);
    } else {
      std::string title = IndexedSubsubfolderName(i);
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
  std::string title0 = IndexedURLTitle(0);
  std::string title1 = IndexedURLTitle(1);
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
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i + 3);
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
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 5; ++i) {
    std::string title = IndexedURLTitle(i + 3);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 0) folder_L0 = folder;
    if (level == 10) folder_L10 = folder;
  }
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i + 10);
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
// Flaky. http://crbug.com/107744.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_SinkBMs10LevelDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L0 = NULL;
  const BookmarkNode* folder_L10 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 0) folder_L0 = folder;
    if (level == 10) folder_L10 = folder;
  }
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i + 10);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5) folder_L5 = folder;
  }
  folder = AddFolder(
      0, GetBookmarkBarNode(0)->child_count(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(
        0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5) folder_L5 = folder;
  }
  folder = AddFolder(
      0, folder_L5, folder_L5->child_count(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedFolderName(i);
    const BookmarkNode* folder = AddFolder(0, i, title);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 10; ++j) {
      std::string title = IndexedURLTitle(j);
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
    std::string title = IndexedFolderName(i);
    const BookmarkNode* folder = AddFolder(0, i, title);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 10; ++j) {
      std::string title = IndexedURLTitle(1000 * i + j);
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
    std::string title0 = IndexedURLTitle(2*i);
    GURL url0 = GURL(IndexedURL(2*i));
    ASSERT_TRUE(AddURL(0, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(2*i+1);
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
    std::string title = IndexedURLTitle(i);
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

  const BookmarkNode* folder0 = AddFolder(0, "Folder");
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddURL(0, folder0, 0, "Bookmark 0", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(0, folder0, 1, "Bookmark 1", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(0, folder0, 2, "Bookmark 2", GURL(kGenericURL)) != NULL);

  const BookmarkNode* folder1 = AddFolder(1, "fOlDeR");
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddURL(1, folder1, 0, "bOoKmArK 0", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(1, folder1, 1, "BooKMarK 1", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddURL(1, folder1, 2, "bOOKMARK 2", GURL(kGenericURL)) != NULL);

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
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    ASSERT_TRUE(AddURL(1, i, title, url) != NULL);
  }

  for (int i = 3; i < 10; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(i+7);
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
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
    ASSERT_TRUE(AddURL(1, i, title, url) != NULL);
  }

  for (int i = 3; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
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
    std::string title0 = IndexedURLTitle(2*i);
    GURL url0 = GURL(IndexedURL(2*i));
    ASSERT_TRUE(AddURL(0, folder0, i, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(2*i+1);
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
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(i+50);
    GURL url1 = GURL(IndexedURL(i+50));
    ASSERT_TRUE(AddURL(1, i, title1, url1) != NULL);
  }
  for (int i = 25; i < 30; ++i) {
    std::string title0 = IndexedFolderName(i);
    const BookmarkNode* folder0 = AddFolder(0, i, title0);
    ASSERT_TRUE(folder0 != NULL);
    std::string title1 = IndexedFolderName(i+50);
    const BookmarkNode* folder1 = AddFolder(1, i, title1);
    ASSERT_TRUE(folder1 != NULL);
    for (int j = 0; j < 5; ++j) {
      std::string title0 = IndexedURLTitle(i+5*j);
      GURL url0 = GURL(IndexedURL(i+5*j));
      ASSERT_TRUE(AddURL(0, folder0, j, title0, url0) != NULL);
      std::string title1 = IndexedURLTitle(i+5*j+50);
      GURL url1 = GURL(IndexedURL(i+5*j+50));
      ASSERT_TRUE(AddURL(1, folder1, j, title1, url1) != NULL);
    }
  }
  for (int i = 100; i < 125; ++i) {
    std::string title = IndexedURLTitle(i);
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
        std::string title = IndexedURLTitle(j);
        GURL url = GURL(IndexedURL(j));
        ASSERT_TRUE(AddURL(i, folder, j, title, url) != NULL);
      } else {
        std::string title = IndexedFolderName(j);
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
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }

  for (int j = 0; j < 2; ++j) {
    std::string title = IndexedURLTitle(j);
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
    std::string title = IndexedURLTitle(i+1);
    GURL url = GURL(IndexedURL(i+1));
    ASSERT_TRUE(AddURL(0, i, title, url) != NULL);
  }

  for (int j = 0; j < 3; ++j) {
    std::string title = IndexedURLTitle(j+4);
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
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddURL(0, node_index, title, url) != NULL);
      ++node_index;
    }
  }
  // Let's add a set of bookmarks {1,2,3,4} to client1.
  for (int i = 0; i < 4; ++i) {
    std::string title = IndexedURLTitle(i+1);
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

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::BOOKMARKS));
  ASSERT_TRUE(AddFolder(1, kGenericFolderName) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::BOOKMARKS));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// TCM ID - 7343544.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  ASSERT_TRUE(AddFolder(0, IndexedFolderName(0)) != NULL);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
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
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder0, i, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(i+5);
    GURL url1 = GURL(IndexedURL(i+5));
    ASSERT_TRUE(AddURL(1, folder1, i, title1, url1) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// This test fails when run with FakeServer and FakeServerInvalidationService.
IN_PROC_BROWSER_TEST_F(LegacyTwoClientBookmarksSyncTest, MC_DeleteBookmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::BOOKMARKS));

  const GURL bar_url("http://example.com/bar");
  const GURL other_url("http://example.com/other");

  ASSERT_TRUE(AddURL(0, GetBookmarkBarNode(0), 0, "bar", bar_url) != NULL);
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 0, "other", other_url) != NULL);

  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  ASSERT_TRUE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_FALSE(HasNodeWithURL(1, other_url));

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::BOOKMARKS));
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_TRUE(HasNodeWithURL(1, other_url));
}

// TCM ID - 3719307 - Test a scenario of updating the name of the same bookmark
// from two clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BookmarkNameChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folder0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  DisableVerifier();
  GURL url(IndexedURL(0));
  SetTitle(0, GetUniqueNodeByURL(0, url), "Title++");
  SetTitle(1, GetUniqueNodeByURL(1, url), "Title--");

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
    std::string title = IndexedURLTitle(i);
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
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddURL(0, folderB[0], i, title, url) != NULL);
  }

  // Create folder C with bookmarks and subfolders on both clients.
  folderC[0] = AddFolder(0, IndexedFolderName(2));
  ASSERT_TRUE(folderC[0] != NULL);
  folderC[1] = AddFolder(1, IndexedFolderName(2));
  ASSERT_TRUE(folderC[1] != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string folder_name = IndexedSubfolderName(i);
    const BookmarkNode* subfolder = AddFolder(0, folderC[0], i, folder_name);
    ASSERT_TRUE(subfolder != NULL);
    for (int j = 0; j < 3; ++j) {
      std::string title = IndexedURLTitle(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddURL(0, subfolder, j, title, url) != NULL);
    }
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder A on both clients.
  SetTitle(0, folderA[0], "Folder A++");
  SetTitle(1, folderA[1], "Folder A--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder B on both clients.
  SetTitle(0, folderB[0], "Folder B++");
  SetTitle(1, folderB[1], "Folder B--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder C on both clients.
  SetTitle(0, folderC[0], "Folder C++");
  SetTitle(1, folderC[1], "Folder C--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(EnableEncryption(1));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SingleClientEnabledEncryptionBothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SingleClientEnabledEncryptionAndChangedMultipleTimes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddURL(0, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AddFolder(0, IndexedFolderName(0)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       FirstClientEnablesEncryptionWithPassSecondChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Add initial bookmarks.
  ASSERT_TRUE(AddURL(0, 0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(AddURL(0, 1, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AddURL(0, 2, IndexedURLTitle(2), GURL(IndexedURL(2))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Set a passphrase and enable encryption on Client 0. Client 1 will not
  // understand the bookmark updates.
  SetEncryptionPassphrase(0, kValidPassphrase, ProfileSyncService::EXPLICIT);
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService((0))));
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(GetSyncService((1))->IsPassphraseRequired());

  // Client 1 adds bookmarks between the first two and between the second two.
  ASSERT_TRUE(AddURL(0, 1, IndexedURLTitle(3), GURL(IndexedURL(3))) != NULL);
  ASSERT_TRUE(AddURL(0, 3, IndexedURLTitle(4), GURL(IndexedURL(4))) != NULL);
  EXPECT_FALSE(AllModelsMatchVerifier());
  EXPECT_FALSE(AllModelsMatch());

  // Set the passphrase. Everything should resolve.
  ASSERT_TRUE(AwaitPassphraseRequired(GetSyncService((1))));
  ASSERT_TRUE(SetDecryptionPassphrase(1, kValidPassphrase));
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService((1))));
  ASSERT_TRUE(AwaitQuiescence());
  EXPECT_TRUE(AllModelsMatch());
  ASSERT_EQ(0,
            GetClient(1)->GetLastSessionSnapshot().num_encryption_conflicts());

  // Ensure everything is syncing normally by appending a final bookmark.
  ASSERT_TRUE(AddURL(1, 5, IndexedURLTitle(5), GURL(IndexedURL(5))) != NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  EXPECT_TRUE(AllModelsMatch());
  ASSERT_EQ(0,
            GetClient(1)->GetLastSessionSnapshot().num_encryption_conflicts());
}

// Deliberately racy rearranging of bookmarks to test that our conflict resolver
// code results in a consistent view across machines (no matter what the final
// order is).
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, RacyPositionChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Add initial bookmarks.
  size_t num_bookmarks = 5;
  for (size_t i = 0; i < num_bookmarks; ++i) {
    ASSERT_TRUE(AddURL(0, i, IndexedURLTitle(i), GURL(IndexedURL(i))) != NULL);
  }

  // Once we make diverging changes the verifer is helpless.
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
  DisableVerifier();

  // Make changes on client 0.
  for (size_t i = 0; i < num_bookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_bookmarks-1);
    DVLOG(1) << "Moving client 0's bookmark " << i << " to position "
             << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  // Make changes on client 1.
  for (size_t i = 0; i < num_bookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_bookmarks-1);
    DVLOG(1) << "Moving client 1's bookmark " << i << " to position "
             << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  // Now make changes to client 1 first.
  for (size_t i = 0; i < num_bookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_bookmarks-1);
    DVLOG(1) << "Moving client 1's bookmark " << i << " to position "
             << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  // Make changes on client 0.
  for (size_t i = 0; i < num_bookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_bookmarks-1);
    DVLOG(1) << "Moving client 0's bookmark " << i << " to position "
             << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// Trigger the server side creation of Synced Bookmarks. Ensure both clients
// remain syncing afterwards. Add bookmarks to the synced bookmarks folder
// and ensure both clients receive the boomkmark.
IN_PROC_BROWSER_TEST_F(LegacyTwoClientBookmarksSyncTest,
                       CreateSyncedBookmarks) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  TriggerCreateSyncedBookmarks();

  // Add a bookmark on Client 0 and ensure it syncs over. This will also trigger
  // both clients downloading the new Synced Bookmarks folder.
  ASSERT_TRUE(AddURL(0, "Google", GURL("http://www.google.com")));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  // Now add a bookmark within the Synced Bookmarks folder and ensure it syncs
  // over.
  const BookmarkNode* synced_bookmarks = GetSyncedBookmarksNode(0);
  ASSERT_TRUE(synced_bookmarks);
  ASSERT_TRUE(AddURL(0, synced_bookmarks, 0, "Google2",
                     GURL("http://www.google2.com")));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       BookmarkAllNodesRemovedEvent) {

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

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

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  // Remove all
  RemoveAll(0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  // Verify other node has no children now.
  EXPECT_EQ(0, GetOtherNode(0)->child_count());
  EXPECT_EQ(0, GetBookmarkBarNode(0)->child_count());
  ASSERT_TRUE(AllModelsMatch());
}
