// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_bookmarks_sync_test.h"
#include "chrome/test/live_sync/live_sync_timing_helper.h"

static const int kNumBookmarks = 150;

// TODO(braffert): Consider the range / resolution of these test points.
static const int kNumBenchmarkPoints = 18;
static const int kBenchmarkPoints[] = {1, 10, 20, 30, 40, 50, 75, 100, 125,
                                       150, 175, 200, 225, 250, 300, 350, 400,
                                       500};

// TODO(braffert): Move this class into its own .h/.cc files.  What should the
// class files be named as opposed to the file containing the tests themselves?
class PerformanceLiveBookmarksSyncTest
    : public TwoClientLiveBookmarksSyncTest {
 public:
  PerformanceLiveBookmarksSyncTest() : url_number(0), url_title_number(0) {}

  // Adds |num_urls| new unique bookmarks to the bookmark bar for |profile|.
  void AddURLs(int profile, int num_urls);

  // Updates the URL for all bookmarks in the bookmark bar for |profile|.
  void UpdateURLs(int profile);

  // Removes all bookmarks in the bookmark bar for |profile|.
  void RemoveURLs(int profile);

  // Remvoes all bookmarks in the bookmark bars for all profiles.  Called
  // between benchmark iterations.
  void Cleanup();

 private:
  // Returns a new unique bookmark URL.
  std::string NextIndexedURL();

  // Returns a new unique bookmark title.
  std::wstring NextIndexedURLTitle();

  int url_number;
  int url_title_number;
  DISALLOW_COPY_AND_ASSIGN(PerformanceLiveBookmarksSyncTest);
};

void PerformanceLiveBookmarksSyncTest::AddURLs(int profile, int num_urls) {
  for (int i = 0; i < num_urls; ++i) {
    ASSERT_TRUE(AddURL(
        profile, 0, NextIndexedURLTitle(), GURL(NextIndexedURL())) != NULL);
  }
}

void PerformanceLiveBookmarksSyncTest::UpdateURLs(int profile) {
  for (int i = 0; i < GetBookmarkBarNode(profile)->child_count(); ++i) {
    ASSERT_TRUE(SetURL(profile, GetBookmarkBarNode(profile)->GetChild(i),
                       GURL(NextIndexedURL())));
  }
}

void PerformanceLiveBookmarksSyncTest::RemoveURLs(int profile) {
  while (GetBookmarkBarNode(profile)->child_count()) {
    Remove(profile, GetBookmarkBarNode(profile), 0);
  }
}

void PerformanceLiveBookmarksSyncTest::Cleanup() {
  for (int i = 0; i < num_clients(); ++i) {
    RemoveURLs(i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(0, GetBookmarkBarNode(0)->child_count());
  ASSERT_TRUE(AllModelsMatch());
}

std::string PerformanceLiveBookmarksSyncTest::NextIndexedURL() {
  return IndexedURL(url_number++);
}

std::wstring PerformanceLiveBookmarksSyncTest::NextIndexedURLTitle() {
  return IndexedURLTitle(url_title_number++);
}

// TODO(braffert): Possibly split each of these into separate up / down test
// cases?
// TCM ID - 7556828.
IN_PROC_BROWSER_TEST_F(PerformanceLiveBookmarksSyncTest, Add) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  DisableNetwork(GetProfile(1));
  AddURLs(0, kNumBookmarks);
  base::TimeDelta dt_up = LiveSyncTimingHelper::TimeSyncCycle(GetClient(0));
  ASSERT_EQ(kNumBookmarks, GetBookmarkBarNode(0)->child_count());
  ASSERT_EQ(0, GetBookmarkBarNode(1)->child_count());

  EnableNetwork(GetProfile(1));
  base::TimeDelta dt_down =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumBookmarks, GetBookmarkBarNode(0)->child_count());
  ASSERT_TRUE(AllModelsMatch());

  // TODO(braffert): Compare timings against some target value.
}

// TCM ID - 7564762.
IN_PROC_BROWSER_TEST_F(PerformanceLiveBookmarksSyncTest, Update) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  AddURLs(0, kNumBookmarks);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  DisableNetwork(GetProfile(1));
  UpdateURLs(0);
  base::TimeDelta dt_up = LiveSyncTimingHelper::TimeSyncCycle(GetClient(0));
  ASSERT_EQ(kNumBookmarks, GetBookmarkBarNode(0)->child_count());
  ASSERT_EQ(kNumBookmarks, GetBookmarkBarNode(1)->child_count());
  ASSERT_FALSE(AllModelsMatch());

  EnableNetwork(GetProfile(1));
  base::TimeDelta dt_down =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumBookmarks, GetBookmarkBarNode(0)->child_count());
  ASSERT_EQ(kNumBookmarks, GetBookmarkBarNode(1)->child_count());

  // TODO(braffert): Compare timings against some target value.
}

// TCM ID - 7566626.
IN_PROC_BROWSER_TEST_F(PerformanceLiveBookmarksSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  AddURLs(0, kNumBookmarks);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  DisableNetwork(GetProfile(1));
  RemoveURLs(0);
  base::TimeDelta dt_up = LiveSyncTimingHelper::TimeSyncCycle(GetClient(0));
  ASSERT_EQ(0, GetBookmarkBarNode(0)->child_count());
  ASSERT_EQ(kNumBookmarks, GetBookmarkBarNode(1)->child_count());

  EnableNetwork(GetProfile(1));
  base::TimeDelta dt_down =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetBookmarkBarNode(0)->child_count());
  ASSERT_EQ(0, GetBookmarkBarNode(1)->child_count());

  // TODO(braffert): Compare timings against some target value.
}

IN_PROC_BROWSER_TEST_F(PerformanceLiveBookmarksSyncTest, DISABLED_Benchmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  for (int i = 0; i < kNumBenchmarkPoints; ++i) {
    int num_bookmarks = kBenchmarkPoints[i];

    // Disable client 1.  Add bookmarks and time commit by client 0.
    DisableNetwork(GetProfile(1));
    AddURLs(0, num_bookmarks);
    base::TimeDelta dt_up = LiveSyncTimingHelper::TimeSyncCycle(GetClient(0));
    ASSERT_EQ(num_bookmarks, GetBookmarkBarNode(0)->child_count());
    ASSERT_EQ(0, GetBookmarkBarNode(1)->child_count());

    // Enable client 1 and time update (new bookmarks).
    EnableNetwork(GetProfile(1));
    base::TimeDelta dt_down =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_bookmarks, GetBookmarkBarNode(0)->child_count());
    ASSERT_TRUE(AllModelsMatch());

    VLOG(0) << std::endl << "Add: " << num_bookmarks << " "
            << dt_up.InSecondsF() << " " << dt_down.InSecondsF();

    // Disable client 1.  Modify bookmarks and time commit by client 0.
    DisableNetwork(GetProfile(1));
    UpdateURLs(0);
    dt_up = LiveSyncTimingHelper::TimeSyncCycle(GetClient(0));
    ASSERT_EQ(num_bookmarks, GetBookmarkBarNode(0)->child_count());
    ASSERT_EQ(num_bookmarks, GetBookmarkBarNode(1)->child_count());
    ASSERT_FALSE(AllModelsMatch());

    // Enable client 1 and time update (changed bookmarks).
    EnableNetwork(GetProfile(1));
    dt_down =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_bookmarks, GetBookmarkBarNode(0)->child_count());
    ASSERT_TRUE(AllModelsMatch());

    VLOG(0) << std::endl << "Update: " << num_bookmarks << " "
            << dt_up.InSecondsF() << " " << dt_down.InSecondsF();

    // Disable client 1.  Delete bookmarks and time commit by client 0.
    DisableNetwork(GetProfile(1));
    RemoveURLs(0);
    dt_up = LiveSyncTimingHelper::TimeSyncCycle(GetClient(0));
    ASSERT_EQ(0, GetBookmarkBarNode(0)->child_count());
    ASSERT_EQ(num_bookmarks, GetBookmarkBarNode(1)->child_count());

    // Enable client 1 and time update (deleted bookmarks).
    EnableNetwork(GetProfile(1));
    dt_down =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(0, GetBookmarkBarNode(0)->child_count());
    ASSERT_EQ(0, GetBookmarkBarNode(1)->child_count());

    VLOG(0) << std::endl << "Delete: " << num_bookmarks << " "
            << dt_up.InSecondsF() << " " << dt_down.InSecondsF();

    Cleanup();
  }
}
