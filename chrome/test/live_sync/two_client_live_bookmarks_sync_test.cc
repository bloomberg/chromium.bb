// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string>

#include "base/file_path.h"
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

using std::string;
using std::wstring;

// TODO(tejasshah): Move single client tests to separate file.

// Some Abbreviations Used:
// F -- BookmarkFolder
// BM -- Bookmark
// L -- Level (Depth of bookmark folder)
class TwoClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  TwoClientLiveBookmarksSyncTest() {
    // This makes sure browser is visible and active while running test.
    InProcessBrowserTest::set_show_window(true);
    // Set the initial timeout value to 5 min.
    InProcessBrowserTest::SetInitialTimeoutInMS(300000);
  }
  virtual ~TwoClientLiveBookmarksSyncTest() {}
  bool SetupSync() {
    profile2_.reset(MakeProfile(L"client2"));
    client1_.reset(new ProfileSyncServiceTestHarness(
        browser()->profile(), username_, password_));
    client2_.reset(new ProfileSyncServiceTestHarness(
        profile2_.get(), username_, password_));
    if (ShouldSetupSyncWithRace()) {
      return client1_->SetupSync() && client2_->SetupSync();
    } else {
      bool result_client1 = client1_->SetupSync();
      client1()->AwaitSyncCycleCompletion("Initial setup");
      return result_client1 && client2_->SetupSync();
    }
  }

  // Overwrites ShouldDeleteProfile, so profile doesn't get deleted.
  virtual bool ShouldDeleteProfile() {
    return false;
  }

  // Overload this method in inherited class and return false to avoid
  // race condition (two clients trying to sync/commit at the same time).
  // Race condition may lead to duplicate bookmarks if there is existing
  // bookmark model on both clients.
  virtual bool ShouldSetupSyncWithRace() {
    return true;
  }

  // Overload this method in your class and return true to pre-populate
  // bookmark files for client2 also.
  virtual bool ShouldCopyBookmarksToClient2() {
    return false;
  }

  // This is used to pre-populate bookmarks hierarchy file to Client1 and
  // Verifier Client.
  void PrePopulateBookmarksHierarchy(const FilePath& bookmarks_file_name) {
    // Let's create default profile directory.
    FilePath dest_user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &dest_user_data_dir);
    FilePath dest_user_data_dir_default = dest_user_data_dir.Append(
        FILE_PATH_LITERAL("Default"));
    file_util::CreateDirectory(dest_user_data_dir_default);
    // Let's create verifier profile directory.
    FilePath dest_user_data_dir_verifier = dest_user_data_dir.Append(
        FILE_PATH_LITERAL("verifier"));
    file_util::CreateDirectory(dest_user_data_dir_verifier);

    // Let's prepare sync data source file path.
    FilePath sync_data_source;
    PathService::Get(base::DIR_SOURCE_ROOT, &sync_data_source);
    sync_data_source = sync_data_source.Append(FILE_PATH_LITERAL("chrome"));
    sync_data_source = sync_data_source.Append(
        FILE_PATH_LITERAL("personalization"));
    sync_data_source = sync_data_source.Append(FILE_PATH_LITERAL("test"));
    sync_data_source = sync_data_source.Append(
        FILE_PATH_LITERAL("live_sync_data"));
    FilePath source_file = sync_data_source.Append(
        bookmarks_file_name);
    ASSERT_TRUE(file_util::PathExists(source_file));
    // Now copy pre-generated bookmark file to default profile.
    ASSERT_TRUE(file_util::CopyFile(source_file,
        dest_user_data_dir_default.Append(FILE_PATH_LITERAL("bookmarks"))));
    // Now copy pre-generated bookmark file to verifier profile.
    ASSERT_TRUE(file_util::CopyFile(source_file,
        dest_user_data_dir_verifier.Append(FILE_PATH_LITERAL("bookmarks"))));

    // Let's pre-populate bookmarks file for client2 also if we need to.
    if (ShouldCopyBookmarksToClient2()) {
      // Let's create verifier profile directory.
      FilePath dest_user_data_dir_client2 = dest_user_data_dir.Append(
          FILE_PATH_LITERAL("client2"));
      file_util::CreateDirectory(dest_user_data_dir_client2);
      // Now copy pre-generated bookmark file to verifier profile.
      ASSERT_TRUE(file_util::CopyFile(source_file,
          dest_user_data_dir_client2.Append(FILE_PATH_LITERAL("bookmarks"))));
    }
  }

  ProfileSyncServiceTestHarness* client1() { return client1_.get(); }
  ProfileSyncServiceTestHarness* client2() { return client2_.get(); }

  void set_client1(ProfileSyncServiceTestHarness* p_client1) {
    client1_.reset(p_client1);
  }

  void set_client2(ProfileSyncServiceTestHarness* p_client2) {
    client2_.reset(p_client2);
  }

  void set_profile2(Profile* p) {
    profile2_.reset(p);
  }

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

class LiveSyncTestPrePopulatedHistory1K
    : public TwoClientLiveBookmarksSyncTest {
 public:
  LiveSyncTestPrePopulatedHistory1K() {}
  virtual ~LiveSyncTestPrePopulatedHistory1K() {}

  // This is used to pre-populate history data (1K URL Visit)to Client1
  // and Verifier Client.
  void PrePopulateHistory1K() {
    // Let's copy history files to default profile.
    FilePath dest_user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &dest_user_data_dir);
    dest_user_data_dir = dest_user_data_dir.Append(
        FILE_PATH_LITERAL("Default"));
    file_util::CreateDirectory(dest_user_data_dir);
    FilePath sync_data_source;
    PathService::Get(base::DIR_SOURCE_ROOT, &sync_data_source);
    sync_data_source = sync_data_source.Append(FILE_PATH_LITERAL("chrome"));
    sync_data_source = sync_data_source.Append(
        FILE_PATH_LITERAL("personalization"));
    sync_data_source = sync_data_source.Append(FILE_PATH_LITERAL("test"));
    sync_data_source = sync_data_source.Append(
        FILE_PATH_LITERAL("live_sync_data"));
    sync_data_source = sync_data_source.Append(
        FILE_PATH_LITERAL("1K_url_visit_history"));
    sync_data_source = sync_data_source.Append(FILE_PATH_LITERAL("Default"));
    ASSERT_TRUE(file_util::PathExists(sync_data_source));
    file_util::FileEnumerator sync_data(
        sync_data_source, false, file_util::FileEnumerator::FILES);
    FilePath source_file = sync_data.Next();
    while (!source_file.empty()) {
      FilePath dest_file = dest_user_data_dir.Append(source_file.BaseName());
      ASSERT_TRUE(file_util::CopyFile(source_file, dest_file));
      source_file = sync_data.Next();
    }
  }

  virtual void SetUp() {
    PrePopulateHistory1K();
    LiveBookmarksSyncTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveSyncTestPrePopulatedHistory1K);
};

class LiveSyncTestBasicHierarchy50BM
    : public TwoClientLiveBookmarksSyncTest {
 public:
  LiveSyncTestBasicHierarchy50BM() {}
  virtual ~LiveSyncTestBasicHierarchy50BM() {}

  virtual void SetUp() {
    FilePath file_name(FILE_PATH_LITERAL("bookmarks_50BM5F3L"));
    PrePopulateBookmarksHierarchy(file_name);
    LiveBookmarksSyncTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveSyncTestBasicHierarchy50BM);
};

class LiveSyncTestBasicHierarchy50BMBothClients
    : public LiveSyncTestBasicHierarchy50BM {
 public:
  LiveSyncTestBasicHierarchy50BMBothClients() {}
  virtual ~LiveSyncTestBasicHierarchy50BMBothClients() {}
  // Overloading this method and return true to pre-populate
  // bookmark files for client2 also.
  virtual bool ShouldCopyBookmarksToClient2() {
    return true;
  }

  // Overloading to ensure there is no race condition between clients
  // while doing initial set up of sync.
  virtual bool ShouldSetupSyncWithRace() {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveSyncTestBasicHierarchy50BMBothClients);
};

class LiveSyncTestComplexHierarchy800BM
    : public TwoClientLiveBookmarksSyncTest {
 public:
  LiveSyncTestComplexHierarchy800BM() {}
  virtual ~LiveSyncTestComplexHierarchy800BM() {}
  virtual void SetUp() {
    FilePath file_name(FILE_PATH_LITERAL("bookmarks_800BM32F8L"));
    TwoClientLiveBookmarksSyncTest::PrePopulateBookmarksHierarchy(file_name);
    LiveBookmarksSyncTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveSyncTestComplexHierarchy800BM);
};

class LiveSyncTestHugeHierarchy5500BM
    : public TwoClientLiveBookmarksSyncTest {
 public:
  LiveSyncTestHugeHierarchy5500BM() {}
  virtual ~LiveSyncTestHugeHierarchy5500BM() {}
  virtual void SetUp() {
    FilePath file_name(FILE_PATH_LITERAL("bookmarks_5500BM125F25L"));
    TwoClientLiveBookmarksSyncTest::PrePopulateBookmarksHierarchy(file_name);
    LiveBookmarksSyncTest::SetUp();
  }
  virtual bool ShouldSetupSyncWithRace() {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveSyncTestHugeHierarchy5500BM);
};

class LiveSyncTestDefaultIEFavorites
    : public TwoClientLiveBookmarksSyncTest {
 public:
  LiveSyncTestDefaultIEFavorites() {}
  virtual ~LiveSyncTestDefaultIEFavorites() {}

  virtual void SetUp() {
    const FilePath file_name(
        FILE_PATH_LITERAL("bookmarks_default_IE_favorites"));
    TwoClientLiveBookmarksSyncTest::PrePopulateBookmarksHierarchy(file_name);
    LiveBookmarksSyncTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveSyncTestDefaultIEFavorites);
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
    ASSERT_TRUE(google_one != NULL);

    // To make this test deterministic, we wait here so there is no race to
    // decide which bookmark actually gets position 0.
    ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
    const BookmarkNode* yahoo_two = verifier->AddURL(model_two, bbn_two, 0,
        L"Yahoo", GURL("http://www.yahoo.com"));
    ASSERT_TRUE(yahoo_two != NULL);
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
    ASSERT_TRUE(cnn_one != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  {
    const BookmarkNode* facebook_two = verifier->AddURL(model_two,
        bbn_two, 0, L"Facebook", GURL("http://www.facebook.com"));
    ASSERT_TRUE(facebook_two != NULL);
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
  GURL initial_url("http://www.google.com");
  GURL second_url("http://www.google.com/abc");
  GURL third_url("http://www.google.com/def");
  wstring title = L"Google";
  {
    const BookmarkNode* google = verifier->AddURL(model_one, bbn_one, 0,
        title, initial_url);
    ASSERT_TRUE(google != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  {
    const BookmarkNode* google_one = GetByUniqueURL(model_one, initial_url);
    const BookmarkNode* google_two = GetByUniqueURL(model_two, initial_url);
    bookmark_utils::ApplyEditsWithNoGroupChange(model_one, bbn_one,
        BookmarkEditor::EditDetails(google_one), title, second_url, NULL);
    bookmark_utils::ApplyEditsWithNoGroupChange(model_two, bbn_two,
        BookmarkEditor::EditDetails(google_two), title, third_url, NULL);
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

// Test Scribe ID - 370439.
IN_PROC_BROWSER_TEST_F(LiveSyncTestDefaultIEFavorites,
    SC_BootStrapWithDefaultIEFavorites) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();

  // Wait for changes to propagate.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  // Let's compare and make sure both bookmark models are same after sync.
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 370441.
IN_PROC_BROWSER_TEST_F(LiveSyncTestComplexHierarchy800BM,
    SC_BootStrapWithComplexBMHierarchy) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();

  // Wait for changes to propagate.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  // Let's compare and make sure both bookmark models are same after sync.
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 370442.
IN_PROC_BROWSER_TEST_F(LiveSyncTestHugeHierarchy5500BM,
    SC_BootStrapWithHugeBMs) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();

  // Wait for changes to propagate.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  // Let's compare and make sure both bookmark models are same after sync.
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 370489.
IN_PROC_BROWSER_TEST_F(LiveSyncTestPrePopulatedHistory1K,
    SC_AddFirstBMWithFavicon) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark(with favicon)
  {
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one, 0,
        L"Welcome to Facebook! | Facebook", GURL("http://www.facebook.com"));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  BookmarkModelVerifier::ExpectModelsMatchIncludingFavicon(model_one,
      model_two, true);
  Cleanup();
}

// Test Scribe ID - 370558.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddFirstFolder) {

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
    ASSERT_TRUE(new_folder_one != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 370559.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddFirstBMWithoutFavicon) {

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
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 370560.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddNonHTTPBMs) {
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
    ASSERT_TRUE(ftp_bm != NULL);
    const BookmarkNode* file_bm = verifier->AddURL(model_one, bbn_one, 1,
        L"FileBookmark", GURL("file:///"));
    ASSERT_TRUE(file_bm != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 370561.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddFirstBMUnderFolder) {

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
    ASSERT_TRUE(test_bm1 != NULL);
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 370562.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddSeveralBMsUnderBMBarAndOtherBM) {

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
    wstring title(L"TestBookmark");
        title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bbn_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  for (int index = 0; index < 10; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, other_bm_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 370563.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_AddSeveralBMsAndFolders) {

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
        wstring title(L"BB - TestBookmark");
        title.append(IntToWString(index));
        string url("http://www.nofaviconurl");
        url.append(IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm =
            verifier->AddURL(model_one, bbn_one, index, title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"BB - TestBMFolder");
        title.append(IntToWString(index));
        const BookmarkNode* bm_folder = verifier->AddGroup(model_one, bbn_one,
            index, title);
        int random_int2 = base::RandInt(1, 100);
        // 60% of time we will add bookmarks to added folder
        if (random_int2 > 40) {
            for (int index = 0; index < 20; index++) {
              wstring child_title(title);
              child_title.append(L" - ChildTestBM");
              child_title.append(IntToWString(index));
              string url("http://www.nofaviconurl");
              url.append(IntToString(index));
              url.append(".com");
              const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
                    bm_folder, index, child_title, GURL(url));
              ASSERT_TRUE(nofavicon_bm != NULL);
            }
        }
    }
  }
  LOG(INFO) << "Adding several bookmarks under other bookmarks";
  for (int index = 0; index < 10; index++) {
    wstring title(L"Other - TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl-other");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, other_bm_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 370641.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DuplicateBMWithDifferentURLSameName) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add two bookmarks with different URL but same name
  {
    const BookmarkNode* google_bm = verifier->AddURL(model_one, bbn_one, 0,
        L"Google", GURL("http://www.google.com"));
    ASSERT_TRUE(google_bm != NULL);
    const BookmarkNode* google_news_bm = verifier->AddURL(model_one, bbn_one, 1,
        L"Google", GURL("http://www.google.com/news"));
    ASSERT_TRUE(google_news_bm != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 371817.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMName) {

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

// Test Scribe ID - 371822.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMURL) {

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
  nofavicon_bm = verifier->SetURL(model_one, nofavicon_bm,
      GURL("http://www.cnn.com"));
  // Wait for changes to sync and then verify
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 371824.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMFolder) {

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

// Test Scribe ID - 371825.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameEmptyBMFolder) {
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

// Test Scribe ID - 371826.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMFolderWithLongHierarchy) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark folder to under bookmark_bar.
  const BookmarkNode* test_bm_folder =
      verifier->AddGroup(model_one, bbn_one, 0, L"Test BMFolder");

  // Let's add lots of bookmarks and folders underneath test_bm_folder.
  for (int index = 0; index < 120; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 85% of time add bookmarks
    if (random_int > 15) {
        wstring title(L"Test BMFolder - ChildTestBookmark");
        title.append(IntToWString(index));
        string url("http://www.nofaviconurl");
        url.append(IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm =
            verifier->AddURL(model_one, test_bm_folder, index,
            title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"Test BMFolder - ChildTestBMFolder");
        title.append(IntToWString(index));
        const BookmarkNode* bm_folder =
            verifier->AddGroup(model_one, test_bm_folder, index, title);
        ASSERT_TRUE(bm_folder != NULL);
    }
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's rename test_bm_folder.
  verifier->SetTitle(model_one, test_bm_folder, L"New TestBMFolder");
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 371827.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMFolderThatHasParentAndChildren) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark folder to under bookmark_bar.
  const BookmarkNode* parent_bm_folder =
      verifier->AddGroup(model_one, bbn_one, 0, L"Parent TestBMFolder");

  // Let's add few bookmarks under bookmark_bar.
  for (int index = 1; index < 15; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bbn_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  // Let's add first bookmark folder under parent_bm_folder.
  const BookmarkNode* test_bm_folder =
      verifier->AddGroup(model_one, parent_bm_folder, 0, L"Test BMFolder");
  // Let's add lots of bookmarks and folders underneath test_bm_folder.
  for (int index = 0; index < 120; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 85% of time add bookmarks
    if (random_int > 15) {
        wstring title(L"Test BMFolder - ChildTestBookmark");
        title.append(IntToWString(index));
        string url("http://www.nofaviconurl");
        url.append(IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm =
            verifier->AddURL(model_one, test_bm_folder, index,
            title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"Test BMFolder - ChildTestBMFolder");
        title.append(IntToWString(index));
        const BookmarkNode* bm_folder =
            verifier->AddGroup(model_one, test_bm_folder, index, title);
        ASSERT_TRUE(bm_folder != NULL);
    }
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's rename test_bm_folder.
  verifier->SetTitle(model_one, test_bm_folder, L"New TestBMFolder");
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 371828.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_RenameBMNameAndURL) {
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

  // Let's change the URL.
  nofavicon_bm = verifier->SetURL(model_one, nofavicon_bm,
      GURL("http://www.cnn.com"));
  // Let's change the Name.
  verifier->SetTitle(model_one, nofavicon_bm, L"CNN");
  // Wait for changes to sync and then verify
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 371832.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DeleteBMEmptyAccountAfterwards) {
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
    ASSERT_TRUE(nofavicon_bm != NULL);
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

// Test Scribe ID - 371833.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add some bookmarks(without favicon)
  for (int index = 0; index < 20; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(
        model_one, bbn_one, index,
        title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
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


// Test Scribe ID - 371835.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelFirstBMUnderBMFoldNonEmptyFoldAfterwards) {
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
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
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


// Test Scribe ID - 371836.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelLastBMUnderBMFoldNonEmptyFoldAfterwards) {
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
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(
        model_one, bm_folder_one,
        index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
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


// Test Scribe ID - 371856.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelMiddleBMUnderBMFoldNonEmptyFoldAfterwards) {
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
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
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


// Test Scribe ID - 371857.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMsUnderBMFoldEmptyFolderAfterwards) {
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
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
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

// Test Scribe ID - 371858.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelEmptyBMFoldEmptyAccountAfterwards) {
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
  ASSERT_TRUE(bm_folder_one != NULL);

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


// Test Scribe ID - 371869.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelEmptyBMFoldNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();
  ASSERT_TRUE(other_bm_one != NULL);

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add first bookmark folder to client1
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 0, L"TestFolder");
  ASSERT_TRUE(bm_folder_one != NULL);
  // Let's add some bookmarks(without favicon)
  for (int index = 1; index < 15; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
      wstring title(L"BB - TestBookmark");
      title.append(IntToWString(index));
      string url("http://www.nofaviconurl");
      url.append(IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one,
          index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
      // Remaining % of time - Add Bookmark folders
      wstring title(L"BB - TestBMFolder");
      title.append(IntToWString(index));
      const BookmarkNode* bm_folder = verifier->AddGroup(model_one, bbn_one,
          index, title);
      ASSERT_TRUE(bm_folder != NULL);
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

// Test Scribe ID - 371879.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMFoldWithBMsNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  ASSERT_TRUE(bbn_one != NULL);
  const BookmarkNode* other_bm_one = model_one->other_node();
  ASSERT_TRUE(other_bm_one != NULL);

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = verifier->AddURL(model_one, bbn_one, 0,
      L"Yahoo!", GURL("http://www.yahoo.com"));
  ASSERT_TRUE(yahoo != NULL);
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 1, L"TestFolder");
  // Let's add some bookmarks(without favicon) and folders to
  // bookmark bar
  for (int index = 2; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
        wstring title(L"BB - TestBookmark");
        title.append(IntToWString(index));
        string url("http://www.nofaviconurl");
        url.append(IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one,
            index, title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"BB - TestBMFolder");
        title.append(IntToWString(index));
        const BookmarkNode* bm_folder = verifier->AddGroup(model_one, bbn_one,
            index, title);
        ASSERT_TRUE(bm_folder != NULL);
     }
  }

  // Let's add some bookmarks(without favicon) to bm_folder_one ('TestFolder')
  for (int index = 0; index < 15; index++) {
    wstring title(L"Level2 - TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
        bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's delete the bookmark folder (bm_folder_one)
  verifier->Remove(model_one, bbn_one, 1);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


// Test Scribe ID - 371880.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMFoldWithBMsAndBMFoldsNonEmptyACAfterwards) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();
  ASSERT_TRUE(other_bm_one != NULL);

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = verifier->AddURL(model_one, bbn_one, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  ASSERT_TRUE(yahoo != NULL);
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 1, L"TestFolder");
  // Let's add some bookmarks(without favicon) and folders to
  // bookmark bar
  for (int index = 2; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
        wstring title(L"BB - TestBookmark");
        title.append(IntToWString(index));
        string url("http://www.nofaviconurl");
        url.append(IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one,
            index, title, GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"BB - TestBMFolder");
        title.append(IntToWString(index));
        const BookmarkNode* bm_folder = verifier->AddGroup(model_one, bbn_one,
            index, title);
        ASSERT_TRUE(bm_folder != NULL);
     }
  }

  // Let's add some bookmarks(without favicon) and folders to
  // bm_folder_one ('TestFolder')
  for (int index = 0; index < 10; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 40% of time add bookmarks
    if (random_int > 60) {
      wstring title(L"Level2 - TestBookmark");
      title.append(IntToWString(index));
      string url("http://www.nofaviconurl");
      url.append(IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
          bm_folder_one, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"Level2 - TestBMFolder");
        title.append(IntToWString(index));
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
              wstring title(L"Level3 - TestBookmark");
              title.append(IntToWString(index));
              string url("http://www.nofaviconurl");
              url.append(IntToString(index));
              url.append(".com");
              const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
                  l2_bm_folder, index2, title, GURL(url));
              ASSERT_TRUE(nofavicon_bm != NULL);
            } else {
                // Remaining % of time - Add Bookmark folders
                wstring title(L"Level3 - TestBMFolder");
                title.append(IntToWString(index));
                const BookmarkNode* l3_bm_folder =
                    verifier->AddGroup(model_one, l2_bm_folder, index2, title);
                ASSERT_TRUE(l3_bm_folder != NULL);
             }
          }  // end inner for loop
        }
     }
    }
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's delete the bookmark folder (bm_folder_one)
  verifier->Remove(model_one, bbn_one, 1);


  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 371882.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_DelBMFoldWithParentAndChildrenBMsAndBMFolds) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add first bookmark folder to under bookmark_bar.
  const BookmarkNode* parent_bm_folder =
      verifier->AddGroup(model_one, bbn_one, 0, L"Parent TestBMFolder");

  // Let's add few bookmarks under bookmark_bar.
  for (int index = 1; index < 11; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, bbn_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  // Let's add first bookmark folder under parent_bm_folder.
  const BookmarkNode* test_bm_folder =
      verifier->AddGroup(model_one, parent_bm_folder, 0, L"Test BMFolder");
  // Let's add lots of bookmarks and folders underneath test_bm_folder.
  for (int index = 0; index < 30; index++) {
    int random_int = base::RandInt(1, 100);
    // To create randomness in order, 80% of time add bookmarks
    if (random_int > 20) {
        wstring title(L"Test BMFolder - ChildTestBookmark");
        title.append(IntToWString(index));
        string url("http://www.nofaviconurl");
        url.append(IntToString(index));
        url.append(".com");
        const BookmarkNode* nofavicon_bm =
            verifier->AddURL(model_one, test_bm_folder, index, title,
            GURL(url));
        ASSERT_TRUE(nofavicon_bm != NULL);
    } else {
        // Remaining % of time - Add Bookmark folders
        wstring title(L"Test BMFolder - ChildTestBMFolder");
        title.append(IntToWString(index));
        const BookmarkNode* bm_folder =
            verifier->AddGroup(model_one, test_bm_folder, index, title);
        ASSERT_TRUE(bm_folder != NULL);
    }
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's delete test_bm_folder
  verifier->Remove(model_one, parent_bm_folder, 0);
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


// Test Scribe ID - 371931.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_ReverseTheOrderOfTwoBMs) {

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
  ASSERT_TRUE(bm_a != NULL);
  const BookmarkNode* bm_b = verifier->AddURL(
      model_one, bbn_one, 1, L"Bookmark B",
      GURL("http://www.nofaviconurlb.com"));
  ASSERT_TRUE(bm_b != NULL);

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

// Test Scribe ID - 371933.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_ReverseTheOrderOf10BMs) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add 10 bookmarks like 0123456789
  for (int index = 0; index < 10; index++) {
    wstring title(L"BM-");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl-");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
        bbn_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Shuffle bookmarks to make it look like 9876543210
  verifier->ReverseChildOrder(model_one, bbn_one);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 371954.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_MovingBMsFromBMBarToBMFolder) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();
  ASSERT_TRUE(other_bm_one != NULL);

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = verifier->AddURL(model_one, bbn_one, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  ASSERT_TRUE(yahoo != NULL);
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 1, L"TestFolder");
  // Let's add some bookmarks(without favicon) to bookmark bar
  for (int index = 2; index < 10; index++) {
    wstring title(L"BB - TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one, bbn_one,
        index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
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

// Test Scribe ID - 371957.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_MovingBMsFromBMFoldToBMBar) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* other_bm_one = model_one->other_node();
  ASSERT_TRUE(other_bm_one != NULL);

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  // Let's add bookmark and bookmark folder to client1
  const BookmarkNode* yahoo = verifier->AddURL(model_one, bbn_one, 0,
      L"Yahoo", GURL("http://www.yahoo.com"));
  ASSERT_TRUE(yahoo != NULL);
  const BookmarkNode* bm_folder_one =
      verifier->AddGroup(model_one, bbn_one, 1, L"TestFolder");
  // Let's add some bookmarks(without favicon) to bm_folder_one
  for (int index = 0; index < 10; index++) {
    wstring title(L"BB - TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
        bm_folder_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
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

// Test Scribe ID - 371961.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_MovingBMsFromParentBMFoldToChildBMFold) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  const BookmarkNode* parent_folder =
      verifier->AddGroup(model_one, bbn_one, 0, L"Test Parent BMFolder");

  // Let's add bookmarks a,b,c to parent_folder.
  const BookmarkNode* bm_a = verifier->AddURL(
      model_one, parent_folder, 0, L"Bookmark A",
      GURL("http://www.nofaviconurl-a.com"));
  const BookmarkNode* bm_b = verifier->AddURL(
      model_one, parent_folder, 1, L"Bookmark B",
      GURL("http://www.nofaviconurl-b.com"));
  const BookmarkNode* bm_c = verifier->AddURL(
      model_one, parent_folder, 2, L"Bookmark C",
      GURL("http://www.nofaviconurl-c.com"));
  const BookmarkNode* child_folder =
      verifier->AddGroup(model_one, parent_folder, 3, L"Test Child BMFolder");

  // Let's add few bookmarks under child_folder.
  for (int index = 0; index < 10; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm =
        verifier->AddURL(model_one, child_folder, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move bookmark a,b,c to child_folder.
  verifier->Move(model_one, bm_a, child_folder, 10);
  verifier->Move(model_one, bm_b, child_folder, 11);
  verifier->Move(model_one, bm_c, child_folder, 12);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


// Test Scribe ID - 371964.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_MovingBMsFromChildBMFoldToParentBMFold) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  const BookmarkNode* parent_folder =
      verifier->AddGroup(model_one, bbn_one, 0, L"Test Parent BMFolder");

  // Let's add bookmarks a,b,c to parent_folder.
  const BookmarkNode* bm_a = verifier->AddURL(
      model_one, parent_folder, 0, L"Bookmark A",
      GURL("http://www.nofaviconurl-a.com"));
  ASSERT_TRUE(bm_a != NULL);
  const BookmarkNode* bm_b = verifier->AddURL(
      model_one, parent_folder, 1, L"Bookmark B",
      GURL("http://www.nofaviconurl-b.com"));
  ASSERT_TRUE(bm_b != NULL);
  const BookmarkNode* bm_c = verifier->AddURL(
      model_one, parent_folder, 2, L"Bookmark C",
      GURL("http://www.nofaviconurl-c.com"));
  ASSERT_TRUE(bm_c != NULL);
  const BookmarkNode* child_folder =
      verifier->AddGroup(model_one, parent_folder, 3, L"Test Child BMFolder");

  // Let's add bookmarks d,e,f,g,h to child_folder.
  const BookmarkNode* bm_d = verifier->AddURL(
      model_one, child_folder, 0, L"Bookmark D",
      GURL("http://www.nofaviconurl-d.com"));
  ASSERT_TRUE(bm_d != NULL);
  const BookmarkNode* bm_e = verifier->AddURL(
      model_one, child_folder, 1, L"Bookmark E",
      GURL("http://www.nofaviconurl-e.com"));
  ASSERT_TRUE(bm_e != NULL);
  const BookmarkNode* bm_f = verifier->AddURL(
      model_one, child_folder, 2, L"Bookmark F",
      GURL("http://www.nofaviconurl-f.com"));
  ASSERT_TRUE(bm_f != NULL);
  const BookmarkNode* bm_g = verifier->AddURL(
      model_one, child_folder, 3, L"Bookmark G",
      GURL("http://www.nofaviconurl-g.com"));
  ASSERT_TRUE(bm_g != NULL);
  const BookmarkNode* bm_h = verifier->AddURL(
      model_one, child_folder, 4, L"Bookmark H",
      GURL("http://www.nofaviconurl-h.com"));
  ASSERT_TRUE(bm_h != NULL);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move bookmark d,e,h to parent_folder.
  verifier->Move(model_one, bm_d, parent_folder, 4);
  verifier->Move(model_one, bm_e, parent_folder, 3);
  verifier->Move(model_one, bm_h, parent_folder, 0);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


// Test Scribe ID - 371967.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_HoistBMs10LevelUp) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  const BookmarkNode* bm_folder = bbn_one;
  const BookmarkNode* bm_folder_L10;
  const BookmarkNode* bm_folder_L0;
  for (int level = 0; level < 15; level++) {
    // Let's add some bookmarks(without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(bm_folder->GetTitle());
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWString(index));
      url.append(IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWString(level));

    bm_folder = verifier->AddGroup(model_one,
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
  const BookmarkNode* bm_a = verifier->AddURL(model_one,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-A",
      GURL("http://www.bm-a.com"));
  const BookmarkNode* bm_b = verifier->AddURL(model_one,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-B",
      GURL("http://www.bm-b.com"));
  const BookmarkNode* bm_c = verifier->AddURL(model_one,
      bm_folder_L10, bm_folder_L10->GetChildCount(), L"BM-C",
      GURL("http://www.bm-c.com"));
  // Let's wait till all the changes populate to another client.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move bookmark a from bm_folder_L10 to first bookmark folder- at end.
  verifier->Move(model_one, bm_a, bm_folder_L0, bm_folder_L0->GetChildCount());
  // Let's move bookmark b to first bookmark folder- at the beginning.
  verifier->Move(model_one, bm_b, bm_folder_L0, 0);
  // Let's move bookmark c to first bookmark folder- in the middle.
  verifier->Move(model_one, bm_c, bm_folder_L0, 1);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 371968.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_SinkBMs10LevelDown) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  const BookmarkNode* bm_folder = bbn_one;
  const BookmarkNode* bm_folder_L10;
  const BookmarkNode* bm_folder_L0;
  for (int level = 0; level < 15; level++) {
    // Let's add some bookmarks(without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(bm_folder->GetTitle());
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWString(index));
      url.append(IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWString(level));

    bm_folder = verifier->AddGroup(model_one,
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
  const BookmarkNode* bm_a = verifier->AddURL(model_one,
      bm_folder_L10, bm_folder_L0->GetChildCount(), L"BM-A",
      GURL("http://www.bm-a.com"));
  const BookmarkNode* bm_b = verifier->AddURL(model_one,
      bm_folder_L10, bm_folder_L0->GetChildCount(), L"BM-B",
      GURL("http://www.bm-b.com"));
  const BookmarkNode* bm_c = verifier->AddURL(model_one,
      bm_folder_L10, bm_folder_L0->GetChildCount(), L"BM-C",
      GURL("http://www.bm-c.com"));
  // Let's wait till all the changes populate to another client.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move bookmark a from bm_folder_L10 to first bookmark
  // folder- at end.
  verifier->Move(model_one, bm_a, bm_folder_L10,
      bm_folder_L10->GetChildCount());
  // Let's move bookmark b to first bookmark folder- at the beginning.
  verifier->Move(model_one, bm_b, bm_folder_L10, 0);
  // Let's move bookmark c to first bookmark folder- in the middle.
  verifier->Move(model_one, bm_c, bm_folder_L10, 1);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}


// Test Scribe ID - 371980.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_SinkEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  const BookmarkNode* bm_folder = bbn_one;
  const BookmarkNode* bm_folder_L5 = NULL;
  for (int level = 0; level < 6; level++) {
    // Let's add some bookmarks(without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(bm_folder->GetTitle());
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWString(index));
      url.append(IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWString(level));

    bm_folder = verifier->AddGroup(model_one,
        bm_folder, bm_folder->GetChildCount(), title);
    // Let's remember 5th level bm folder for later use.
    if (level == 5) {
      bm_folder_L5 = bm_folder;
    }
  }
  const BookmarkNode* empty_bm_folder = verifier->AddGroup(model_one,
      bbn_one, bbn_one->GetChildCount(), L"EmptyTest BMFolder");

  // Let's wait until all the changes populate to another client.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move empty_bm_folder from bookmark bar to bm_folder_L5 (at the end).
  verifier->Move(model_one, empty_bm_folder, bm_folder_L5,
      bm_folder_L5->GetChildCount());

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 371997.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_SinkNonEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  const BookmarkNode* bm_folder = bbn_one;
  const BookmarkNode* bm_folder_L5 = NULL;
  for (int level = 0; level < 6; level++) {
    // Let's add some bookmarks (without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(bm_folder->GetTitle());
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWString(index));
      url.append(IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWString(level));

    bm_folder = verifier->AddGroup(model_one,
        bm_folder, bm_folder->GetChildCount(), title);
    // Let's remember 5th level bm folder for later use.
    if (level == 5) {
      bm_folder_L5 = bm_folder;
    }
  }
  const BookmarkNode* my_bm_folder = verifier->AddGroup(model_one,
      bbn_one, bbn_one->GetChildCount(), L"MyTest BMFolder");
  // Let's add few bookmarks to my_bm_folder.
  for (int index = 0; index < 10; index++) {
    wstring title(bm_folder->GetTitle());
    title.append(L"-BM");
    string url("http://www.nofaviconurl-");
    title.append(IntToWString(index));
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
        my_bm_folder, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  // Let's wait until all the changes populate to another client.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move my_bm_folder from bookmark bar to bm_folder_L5 (at the end).
  verifier->Move(model_one, my_bm_folder, bm_folder_L5,
      bm_folder_L5->GetChildCount());

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 372006.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_HoistFolder5LevelsUp) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  const BookmarkNode* bm_folder = bbn_one;
  const BookmarkNode* bm_folder_L5 = NULL;
  for (int level = 0; level < 6; level++) {
    // Let's add some bookmarks(without favicon) to bm_folder.
    int child_count = base::RandInt(0, 10);
    for (int index = 0; index < child_count; index++) {
      wstring title(bm_folder->GetTitle());
      title.append(L"-BM");
      string url("http://www.nofaviconurl-");
      title.append(IntToWString(index));
      url.append(IntToString(index));
      url.append(".com");
      const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
          bm_folder, index, title, GURL(url));
      ASSERT_TRUE(nofavicon_bm != NULL);
    }
    wstring title(L"Test BMFolder-");
    title.append(IntToWString(level));

    bm_folder = verifier->AddGroup(model_one,
        bm_folder, bm_folder->GetChildCount(), title);
    // Let's remember 5th level bm folder for later use.
    if (level == 5) {
      bm_folder_L5 = bm_folder;
    }
  }
  const BookmarkNode* my_bm_folder = verifier->AddGroup(model_one,
      bm_folder_L5, bm_folder_L5->GetChildCount(), L"MyTest BMFolder");
  // Let's add few bookmarks to my_bm_folder.
  for (int index = 0; index < 10; index++) {
    wstring title(bm_folder->GetTitle());
    title.append(L"-BM");
    string url("http://www.nofaviconurl-");
    title.append(IntToWString(index));
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm = verifier->AddURL(model_one,
        my_bm_folder, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm != NULL);
  }

  // Let's wait until all the changes populate to another client.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's move my_bm_folder from  bm_folder_L5 to bookmark bar- at end.
  verifier->Move(model_one, my_bm_folder, bbn_one, bbn_one->GetChildCount());

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}



// Test Scribe ID - 372026.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_ReverseTheOrderOfTwoBMFolders) {

  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();

  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  const BookmarkNode* bm_folder_a =
    verifier->AddNonEmptyGroup(model_one, bbn_one, 0, L"TestBMFolderA", 10);
  ASSERT_TRUE(bm_folder_a != NULL);
  const BookmarkNode* bm_folder_b =
    verifier->AddNonEmptyGroup(model_one, bbn_one, 1, L"TestBMFolderB", 10);
  ASSERT_TRUE(bm_folder_b != NULL);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's change positions of bookmark folders so it is more like ba.
  verifier->ReverseChildOrder(model_one, bbn_one);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);
  Cleanup();
}

// Test Scribe ID - 372028.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    SC_ReverseTheOrderOfTenBMFolders) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Let's add 10 non-empty bookmark folders like 0123456789
  for (int index = 0; index < 10; index++) {
    wstring title(L"BM Folder");
    title.append(IntToWString(index));
    const BookmarkNode* child_bm_folder = verifier->AddNonEmptyGroup(
        model_one, bbn_one, index, title, 10);
    ASSERT_TRUE(child_bm_folder != NULL);
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  // Shuffle bookmark folders to make it look like 9876543210
  verifier->ReverseChildOrder(model_one, bbn_one);

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 373378.
IN_PROC_BROWSER_TEST_F(LiveSyncTestBasicHierarchy50BM,
    MC_PushExistingBMsToSecondClient) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();

  // Wait for changes to propagate.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  // Let's compare and make sure both bookmark models are same after sync.
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 373379.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_BiDirectionalPushAddingBM) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* bbn_two = model_one->GetBookmarkBarNode();

  BookmarkModelVerifier::ExpectModelsMatch(model_one, model_two);

  // Let's add 2 bookmarks (without favicon) on each client.
  {
    const BookmarkNode* bm_foo1 = model_one->AddURL(
        bbn_one, 0, L"Foo1", GURL("http://www.foo1.com"));
    ASSERT_TRUE(bm_foo1 != NULL);
    const BookmarkNode* bm_foo3 = model_two->AddURL(
        bbn_two, 0, L"Foo3", GURL("http://www.foo3.com"));
    ASSERT_TRUE(bm_foo3 != NULL);

    const BookmarkNode* bm_foo2 = model_one->AddURL(
        bbn_one, 1, L"Foo2", GURL("http://www.foo2.com"));
    ASSERT_TRUE(bm_foo2 != NULL);
    const BookmarkNode* bm_foo4 = model_two->AddURL(
        bbn_two, 1, L"Foo4", GURL("http://www.foo4.com"));
    ASSERT_TRUE(bm_foo4 != NULL);
  }

  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletionWithConflict(client2()));
  BookmarkModelVerifier::ExpectModelsMatch(model_one, model_two);
  Cleanup();
}

// Test Scribe ID - 373506.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_BootStrapEmptyStateEverywhere) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();

  // Wait for changes to propagate.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  // Let's compare and make sure both bookmark models are same after sync.
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 373507.
IN_PROC_BROWSER_TEST_F(LiveSyncTestBasicHierarchy50BMBothClients,
    MC_FirstUseExistingSameBMModelBothClients) {
  ASSERT_TRUE(SetupSync()) << "Failed to SetupSync";
  scoped_ptr<BookmarkModelVerifier> verifier(BookmarkModelVerifier::Create());
  BookmarkModel* model_one = profile1()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();

  // Wait for changes to propagate.
  ASSERT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));
  // Let's compare and make sure both bookmark models are same after sync.
  verifier->ExpectMatch(model_one);
  verifier->ExpectMatch(model_two);

  Cleanup();
}

// Test Scribe ID - 373508.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_SimpleMergeOfDifferentBMModels) {
  set_profile2(MakeProfile(L"client2"));
  BookmarkModel* model_one = browser()->profile()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  LiveBookmarksSyncTest::BlockUntilLoaded(model_two);

  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* bbn_two = model_two->GetBookmarkBarNode();

  // Let's add same bookmarks (without favicon) to both clients.
  for (int index = 0; index < 3; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client1 =
        model_one->AddURL(bbn_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
    const BookmarkNode* nofavicon_bm_client2 =
        model_two->AddURL(bbn_two, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client2 != NULL);
  }

  // Let's add some different bookmarks (without favicon) to client1.
  for (int index = 3; index < 11 ; index++) {
    wstring title(L"Client1-TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.client1-nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client1 =
        model_one->AddURL(bbn_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
  }

   // Let's add some different bookmarks (without favicon) to client2.
  for (int index = 3; index < 11 ; index++) {
    wstring title(L"Client2-TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.Client2-nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client2 =
        model_two->AddURL(bbn_two, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client2 != NULL);
  }

  // Set up sync on both clients.
  set_client1(new ProfileSyncServiceTestHarness(
      browser()->profile(), username_, password_));
  set_client2(new ProfileSyncServiceTestHarness(
      profile2(), username_, password_));
  ASSERT_TRUE(client1()->SetupSync()) << "Failed to SetupSync on Client1";
  ASSERT_TRUE(client2()->SetupSync()) << "Failed to SetupSync on Client2";

  // Wait for changes to propagate.
  ASSERT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));
  // Let's make sure there aren't any duplicates after sync.
  BookmarkModelVerifier::VerifyNoDuplicates(model_one);
  // Let's compare and make sure both bookmark models are same after sync.
  BookmarkModelVerifier::ExpectModelsMatchIncludingFavicon(
      model_one, model_two, false);

  Cleanup();
}

// Test Scribe ID - 386586.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_MergeSimpleBMHierarchyUnderBMBar) {
  set_profile2(MakeProfile(L"client2"));
  BookmarkModel* model_one = browser()->profile()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  LiveBookmarksSyncTest::BlockUntilLoaded(model_two);

  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* bbn_two = model_two->GetBookmarkBarNode();

  // Let's add same bookmarks (without favicon) to both clients.
  for (int index = 0; index < 3 ; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client1 =
        model_one->AddURL(bbn_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
    const BookmarkNode* nofavicon_bm_client2 =
        model_two->AddURL(bbn_two, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client2 != NULL);
  }

  // Let's add some different bookmarks (without favicon) to client2.
  for (int index = 3; index < 5 ; index++) {
    wstring title(L"Client2-TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.client2-nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client2 =
        model_two->AddURL(bbn_two, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client2 != NULL);
  }

  // Set up sync on both clients.
  set_client1(new ProfileSyncServiceTestHarness(
      browser()->profile(), username_, password_));
  set_client2(new ProfileSyncServiceTestHarness(
      profile2(), username_, password_));
  ASSERT_TRUE(client1()->SetupSync()) << "Failed to SetupSync on Client1";
  ASSERT_TRUE(client2()->SetupSync()) << "Failed to SetupSync on Client2";

  // Wait for changes to propagate.
  ASSERT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));
  // Let's make sure there aren't any duplicates after sync.
  BookmarkModelVerifier::VerifyNoDuplicates(model_one);
  // Let's compare and make sure both bookmark models are same after sync.
  BookmarkModelVerifier::ExpectModelsMatchIncludingFavicon(
      model_one, model_two, false);

  Cleanup();
}

// Test Scribe ID - 386589.
IN_PROC_BROWSER_TEST_F(TwoClientLiveBookmarksSyncTest,
    MC_MergeSimpleBMHierarchyEqualSetsUnderBMBar) {
  set_profile2(MakeProfile(L"client2"));
  BookmarkModel* model_one = browser()->profile()->GetBookmarkModel();
  BookmarkModel* model_two = profile2()->GetBookmarkModel();
  LiveBookmarksSyncTest::BlockUntilLoaded(model_two);

  const BookmarkNode* bbn_one = model_one->GetBookmarkBarNode();
  const BookmarkNode* bbn_two = model_two->GetBookmarkBarNode();

  // Let's add same bookmarks (without favicon) to both clients.
  for (int index = 0; index < 3 ; index++) {
    wstring title(L"TestBookmark");
    title.append(IntToWString(index));
    string url("http://www.nofaviconurl");
    url.append(IntToString(index));
    url.append(".com");
    const BookmarkNode* nofavicon_bm_client1 =
        model_one->AddURL(bbn_one, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client1 != NULL);
    const BookmarkNode* nofavicon_bm_client2 =
        model_two->AddURL(bbn_two, index, title, GURL(url));
    ASSERT_TRUE(nofavicon_bm_client2 != NULL);
  }

  // Set up sync on both clients.
  set_client1(new ProfileSyncServiceTestHarness(
      browser()->profile(), username_, password_));
  set_client2(new ProfileSyncServiceTestHarness(
      profile2(), username_, password_));
  ASSERT_TRUE(client1()->SetupSync()) << "Failed to SetupSync on Client1";
  ASSERT_TRUE(client2()->SetupSync()) << "Failed to SetupSync on Client2";

  // Wait for changes to propagate.
  ASSERT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));
  // Let's make sure there aren't any duplicates after sync.
  BookmarkModelVerifier::VerifyNoDuplicates(model_one);
  // Let's compare and make sure both bookmark models are same after sync.
  BookmarkModelVerifier::ExpectModelsMatchIncludingFavicon(
      model_one, model_two, false);

  Cleanup();
}
