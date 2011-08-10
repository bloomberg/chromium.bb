// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/bookmarks_helper.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/live_sync/performance/sync_timing_helper.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;
using bookmarks_helper::Remove;
using bookmarks_helper::SetURL;

// TODO(braffert): Move kNumBenchmarkPoints and kBenchmarkPoints for all
// datatypes into a performance test base class, once it is possible to do so.
static const int kNumBookmarks = 150;
static const int kNumBenchmarkPoints = 18;
static const int kBenchmarkPoints[] = {1, 10, 20, 30, 40, 50, 75, 100, 125,
                                       150, 175, 200, 225, 250, 300, 350, 400,
                                       500};

class BookmarksSyncPerfTest : public LiveSyncTest {
 public:
  BookmarksSyncPerfTest()
      : LiveSyncTest(TWO_CLIENT),
        url_number_(0),
        url_title_number_(0) {}

  // Adds |num_urls| new unique bookmarks to the bookmark bar for |profile|.
  void AddURLs(int profile, int num_urls);

  // Updates the URL for all bookmarks in the bookmark bar for |profile|.
  void UpdateURLs(int profile);

  // Removes all bookmarks in the bookmark bar for |profile|.
  void RemoveURLs(int profile);

  // Returns the number of bookmarks stored in the bookmark bar for |profile|.
  int GetURLCount(int profile);

  // Remvoes all bookmarks in the bookmark bars for all profiles.  Called
  // between benchmark iterations.
  void Cleanup();

 private:
  // Returns a new unique bookmark URL.
  std::string NextIndexedURL();

  // Returns a new unique bookmark title.
  std::wstring NextIndexedURLTitle();

  int url_number_;
  int url_title_number_;
  DISALLOW_COPY_AND_ASSIGN(BookmarksSyncPerfTest);
};

void BookmarksSyncPerfTest::AddURLs(int profile, int num_urls) {
  for (int i = 0; i < num_urls; ++i) {
    ASSERT_TRUE(AddURL(
        profile, 0, NextIndexedURLTitle(), GURL(NextIndexedURL())) != NULL);
  }
}

void BookmarksSyncPerfTest::UpdateURLs(int profile) {
  for (int i = 0;
       i < GetBookmarkBarNode(profile)->child_count();
       ++i) {
    ASSERT_TRUE(SetURL(profile,
                       GetBookmarkBarNode(profile)->GetChild(i),
                       GURL(NextIndexedURL())));
  }
}

void BookmarksSyncPerfTest::RemoveURLs(int profile) {
  while (!GetBookmarkBarNode(profile)->empty()) {
    Remove(profile, GetBookmarkBarNode(profile), 0);
  }
}

int BookmarksSyncPerfTest::GetURLCount(int profile) {
  return GetBookmarkBarNode(profile)->child_count();
}

void BookmarksSyncPerfTest::Cleanup() {
  for (int i = 0; i < num_clients(); ++i) {
    RemoveURLs(i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(0, GetBookmarkBarNode(0)->child_count());
  ASSERT_TRUE(AllModelsMatch());
}

std::string BookmarksSyncPerfTest::NextIndexedURL() {
  return IndexedURL(url_number_++);
}

std::wstring BookmarksSyncPerfTest::NextIndexedURLTitle() {
  return IndexedURLTitle(url_title_number_++);
}

IN_PROC_BROWSER_TEST_F(BookmarksSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // TCM ID - 7556828.
  AddURLs(0, kNumBookmarks);
  base::TimeDelta dt =
      SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumBookmarks, GetURLCount(1));
  SyncTimingHelper::PrintResult("bookmarks", "add_bookmarks", dt);

  // TCM ID - 7564762.
  UpdateURLs(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumBookmarks, GetURLCount(1));
  SyncTimingHelper::PrintResult("bookmarks", "update_bookmarks", dt);

  // TCM ID - 7566626.
  RemoveURLs(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetURLCount(1));
  SyncTimingHelper::PrintResult("bookmarks", "delete_bookmarks", dt);
}

IN_PROC_BROWSER_TEST_F(BookmarksSyncPerfTest, DISABLED_Benchmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  for (int i = 0; i < kNumBenchmarkPoints; ++i) {
    int num_bookmarks = kBenchmarkPoints[i];
    AddURLs(0, num_bookmarks);
    base::TimeDelta dt_add =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_bookmarks, GetBookmarkBarNode(0)->child_count());
    ASSERT_TRUE(AllModelsMatch());
    VLOG(0) << std::endl << "Add: " << num_bookmarks << " "
            << dt_add.InSecondsF();

    UpdateURLs(0);
    base::TimeDelta dt_update =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_bookmarks, GetBookmarkBarNode(0)->child_count());
    ASSERT_TRUE(AllModelsMatch());
    VLOG(0) << std::endl << "Update: " << num_bookmarks << " "
            << dt_update.InSecondsF();

    RemoveURLs(0);
    base::TimeDelta dt_delete =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(0, GetBookmarkBarNode(0)->child_count());
    ASSERT_TRUE(AllModelsMatch());
    VLOG(0) << std::endl << "Delete: " << num_bookmarks << " "
            << dt_delete.InSecondsF();

    Cleanup();
  }
}
