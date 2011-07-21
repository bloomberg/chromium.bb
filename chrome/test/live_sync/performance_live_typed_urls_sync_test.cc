// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_sync_timing_helper.h"
#include "chrome/test/live_sync/live_typed_urls_sync_test.h"

static const size_t kNumUrls = 150;

// TODO(braffert): Consider the range / resolution of these test points.
static const size_t kNumBenchmarkPoints = 18;
static const size_t kBenchmarkPoints[] = {1, 10, 20, 30, 40, 50, 75, 100, 125,
                                          150, 175, 200, 225, 250, 300, 350,
                                          400, 500};

// TODO(braffert): Move this class into its own .h/.cc files.  What should the
// class files be named as opposed to the file containing the tests themselves?
class PerformanceLiveTypedUrlsSyncTest
    : public TwoClientLiveTypedUrlsSyncTest {
 public:
  PerformanceLiveTypedUrlsSyncTest() : url_number(0) {}

  // Adds |num_urls| new unique typed urls to |profile|.
  void AddURLs(int profile, int num_urls);

  // Update all typed urls in |profile| by visiting them once again.
  void UpdateURLs(int profile);

  // Removes all typed urls for |profile|.
  void RemoveURLs(int profile);

  // Remvoes all typed urls for all profiles.  Called between benchmark
  // iterations.
  void Cleanup();

 private:
  // Returns a new unique typed URL.
  GURL NextURL();

  // Returns a unique URL according to the integer |n|.
  GURL IntToURL(int n);

  int url_number;
  DISALLOW_COPY_AND_ASSIGN(PerformanceLiveTypedUrlsSyncTest);
};

void PerformanceLiveTypedUrlsSyncTest::AddURLs(int profile, int num_urls) {
  for (int i = 0; i < num_urls; ++i) {
    AddUrlToHistory(profile, NextURL());
  }
}

void PerformanceLiveTypedUrlsSyncTest::UpdateURLs(int profile) {
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(profile);
  for (std::vector<history::URLRow>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    AddUrlToHistory(profile, it->url());
  }
}

void PerformanceLiveTypedUrlsSyncTest::RemoveURLs(int profile) {
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(profile);
  for (std::vector<history::URLRow>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    DeleteUrlFromHistory(profile, it->url());
  }
}

void PerformanceLiveTypedUrlsSyncTest::Cleanup() {
  for (int i = 0; i < num_clients(); ++i) {
    RemoveURLs(i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(0U, GetTypedUrlsFromClient(0).size());
  AssertAllProfilesHaveSameURLsAsVerifier();
}

GURL PerformanceLiveTypedUrlsSyncTest::NextURL() {
  return IntToURL(url_number++);
}

GURL PerformanceLiveTypedUrlsSyncTest::IntToURL(int n) {
  return GURL(StringPrintf("http://history%d.google.com/", n));
}

// TCM ID - 7985716.
IN_PROC_BROWSER_TEST_F(PerformanceLiveTypedUrlsSyncTest, Add) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddURLs(0, kNumUrls);
  base::TimeDelta dt =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumUrls, GetTypedUrlsFromClient(0).size());
  AssertAllProfilesHaveSameURLsAsVerifier();

  // TODO(braffert): Compare timings against some target value.
  VLOG(0) << std::endl << "dt: " << dt.InSecondsF() << " s";
}

// TCM ID - 7981755.
IN_PROC_BROWSER_TEST_F(PerformanceLiveTypedUrlsSyncTest, Update) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddURLs(0, kNumUrls);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  UpdateURLs(0);
  base::TimeDelta dt =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumUrls, GetTypedUrlsFromClient(0).size());
  AssertAllProfilesHaveSameURLsAsVerifier();

  // TODO(braffert): Compare timings against some target value.
  VLOG(0) << std::endl << "dt: " << dt.InSecondsF() << " s";
}

// TCM ID - 7651271
IN_PROC_BROWSER_TEST_F(PerformanceLiveTypedUrlsSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddURLs(0, kNumUrls);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  RemoveURLs(0);
  base::TimeDelta dt =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0U, GetTypedUrlsFromClient(0).size());
  AssertAllProfilesHaveSameURLsAsVerifier();

  // TODO(braffert): Compare timings against some target value.
  VLOG(0) << std::endl << "dt: " << dt.InSecondsF() << " s";
}

IN_PROC_BROWSER_TEST_F(PerformanceLiveTypedUrlsSyncTest, DISABLED_Benchmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (size_t i = 0; i < kNumBenchmarkPoints; ++i) {
    size_t num_urls = kBenchmarkPoints[i];
    AddURLs(0, num_urls);
    base::TimeDelta dt_add =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_urls, GetTypedUrlsFromClient(0).size());
    AssertAllProfilesHaveSameURLsAsVerifier();
    VLOG(0) << std::endl << "Add: " << num_urls << " " << dt_add.InSecondsF();

    UpdateURLs(0);
    base::TimeDelta dt_update =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_urls, GetTypedUrlsFromClient(0).size());
    AssertAllProfilesHaveSameURLsAsVerifier();
    VLOG(0) << std::endl << "Update: " << num_urls << " "
         << dt_update.InSecondsF();

    RemoveURLs(0);
    base::TimeDelta dt_delete =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(0U, GetTypedUrlsFromClient(0).size());
    AssertAllProfilesHaveSameURLsAsVerifier();
    VLOG(0) << std::endl << "Delete: " << num_urls << " "
            << dt_delete.InSecondsF();

    Cleanup();
  }
}
