// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_typed_urls_sync_test.h"

// TCM: 3728323
IN_PROC_BROWSER_TEST_F(MultipleClientLiveTypedUrlsSyncTest, AddToOne) {
  const string16 kHistoryUrl(
      ASCIIToUTF16("http://www.add-one-history.google.com/"));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a URL, should sync to all others.
  GURL new_url(kHistoryUrl);
  AddUrlToHistory(0, new_url);
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());

  // Let sync finish.
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));

  // All clients should have this URL.
  AssertAllProfilesHaveSameURLsAsVerifier();
}

IN_PROC_BROWSER_TEST_F(MultipleClientLiveTypedUrlsSyncTest, AddToAll) {
  const string16 kHistoryUrl(
      ASCIIToUTF16("http://www.add-all-history.google.com/"));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate clients with the same URL.
  for (int i = 0; i < num_clients(); ++i) {
    std::vector<history::URLRow> urls = GetTypedUrlsFromClient(i);
    ASSERT_EQ(0U, urls.size());

    string16 unique_url = kHistoryUrl + base::FormatNumber(i);
    GURL new_url(unique_url);
    AddUrlToHistory(i, new_url);

    urls = GetTypedUrlsFromClient(i);
    ASSERT_EQ(1U, urls.size());
    ASSERT_EQ(new_url, urls[0].url());
  }

  // Wait for sync.
  ASSERT_TRUE(AwaitQuiescence());

  // Verify that all clients have all urls.
  AssertAllProfilesHaveSameURLsAsVerifier();
}

