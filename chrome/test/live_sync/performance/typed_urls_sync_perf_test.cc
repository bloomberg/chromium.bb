// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/performance/sync_timing_helper.h"
#include "chrome/test/live_sync/live_typed_urls_sync_test.h"

// TODO(braffert): Move kNumBenchmarkPoints and kBenchmarkPoints for all
// datatypes into a performance test base class, once it is possible to do so.
static const int kNumUrls = 150;
static const size_t kNumBenchmarkPoints = 18;
static const size_t kBenchmarkPoints[] = {1, 10, 20, 30, 40, 50, 75, 100, 125,
                                          150, 175, 200, 225, 250, 300, 350,
                                          400, 500};

class TypedUrlsSyncPerfTest: public TwoClientLiveTypedUrlsSyncTest {
 public:
  TypedUrlsSyncPerfTest() : url_number(0) {}

  // Adds |num_urls| new unique typed urls to |profile|.
  void AddURLs(int profile, int num_urls);

  // Update all typed urls in |profile| by visiting them once again.
  void UpdateURLs(int profile);

  // Removes all typed urls for |profile|.
  void RemoveURLs(int profile);

  // Returns the number of typed urls stored in |profile|.
  int GetURLCount(int profile);

  // Remvoes all typed urls for all profiles.  Called between benchmark
  // iterations.
  void Cleanup();

 private:
  // Returns a new unique typed URL.
  GURL NextURL();

  // Returns a unique URL according to the integer |n|.
  GURL IntToURL(int n);

  int url_number;
  DISALLOW_COPY_AND_ASSIGN(TypedUrlsSyncPerfTest);
};

void TypedUrlsSyncPerfTest::AddURLs(int profile, int num_urls) {
  for (int i = 0; i < num_urls; ++i) {
    AddUrlToHistory(profile, NextURL());
  }
}

void TypedUrlsSyncPerfTest::UpdateURLs(int profile) {
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(profile);
  for (std::vector<history::URLRow>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    AddUrlToHistory(profile, it->url());
  }
}

void TypedUrlsSyncPerfTest::RemoveURLs(int profile) {
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(profile);
  for (std::vector<history::URLRow>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    DeleteUrlFromHistory(profile, it->url());
  }
}

int TypedUrlsSyncPerfTest::GetURLCount(int profile) {
  return GetTypedUrlsFromClient(profile).size();
}

void TypedUrlsSyncPerfTest::Cleanup() {
  for (int i = 0; i < num_clients(); ++i) {
    RemoveURLs(i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(0U, GetTypedUrlsFromClient(0).size());
  AssertAllProfilesHaveSameURLsAsVerifier();
}

GURL TypedUrlsSyncPerfTest::NextURL() {
  return IntToURL(url_number++);
}

GURL TypedUrlsSyncPerfTest::IntToURL(int n) {
  return GURL(StringPrintf("http://history%d.google.com/", n));
}

IN_PROC_BROWSER_TEST_F(TypedUrlsSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // TCM ID - 7985716.
  AddURLs(0, kNumUrls);
  base::TimeDelta dt =
      SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumUrls, GetURLCount(1));
  SyncTimingHelper::PrintResult("typed_urls", "add", dt);

  // TCM ID - 7981755.
  UpdateURLs(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumUrls, GetURLCount(1));
  SyncTimingHelper::PrintResult("typed_urls", "update", dt);

  // TCM ID - 7651271.
  RemoveURLs(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetURLCount(1));
  SyncTimingHelper::PrintResult("typed_urls", "delete", dt);
}

IN_PROC_BROWSER_TEST_F(TypedUrlsSyncPerfTest, DISABLED_Benchmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (size_t i = 0; i < kNumBenchmarkPoints; ++i) {
    size_t num_urls = kBenchmarkPoints[i];
    AddURLs(0, num_urls);
    base::TimeDelta dt_add =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_urls, GetTypedUrlsFromClient(0).size());
    AssertAllProfilesHaveSameURLsAsVerifier();
    VLOG(0) << std::endl << "Add: " << num_urls << " " << dt_add.InSecondsF();

    UpdateURLs(0);
    base::TimeDelta dt_update =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_urls, GetTypedUrlsFromClient(0).size());
    AssertAllProfilesHaveSameURLsAsVerifier();
    VLOG(0) << std::endl << "Update: " << num_urls << " "
         << dt_update.InSecondsF();

    RemoveURLs(0);
    base::TimeDelta dt_delete =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(0U, GetTypedUrlsFromClient(0).size());
    AssertAllProfilesHaveSameURLsAsVerifier();
    VLOG(0) << std::endl << "Delete: " << num_urls << " "
            << dt_delete.InSecondsF();

    Cleanup();
  }
}
