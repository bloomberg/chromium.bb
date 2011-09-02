// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/live_sync/live_sync_test.h"
#include "chrome/browser/sync/test/live_sync/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/live_sync/typed_urls_helper.h"

using typed_urls_helper::AddUrlToHistory;
using typed_urls_helper::AssertAllProfilesHaveSameURLsAsVerifier;
using typed_urls_helper::DeleteUrlFromHistory;
using typed_urls_helper::GetTypedUrlsFromClient;

static const int kNumUrls = 150;

class TypedUrlsSyncPerfTest : public LiveSyncTest {
 public:
  TypedUrlsSyncPerfTest()
      : LiveSyncTest(TWO_CLIENT),
        url_number_(0) {}

  // Adds |num_urls| new unique typed urls to |profile|.
  void AddURLs(int profile, int num_urls);

  // Update all typed urls in |profile| by visiting them once again.
  void UpdateURLs(int profile);

  // Removes all typed urls for |profile|.
  void RemoveURLs(int profile);

  // Returns the number of typed urls stored in |profile|.
  int GetURLCount(int profile);

 private:
  // Returns a new unique typed URL.
  GURL NextURL();

  // Returns a unique URL according to the integer |n|.
  GURL IntToURL(int n);

  int url_number_;
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

GURL TypedUrlsSyncPerfTest::NextURL() {
  return IntToURL(url_number_++);
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
  SyncTimingHelper::PrintResult("typed_urls", "add_typed_urls", dt);

  // TCM ID - 7981755.
  UpdateURLs(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumUrls, GetURLCount(1));
  SyncTimingHelper::PrintResult("typed_urls", "update_typed_urls", dt);

  // TCM ID - 7651271.
  RemoveURLs(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetURLCount(1));
  SyncTimingHelper::PrintResult("typed_urls", "delete_typed_urls", dt);
}
