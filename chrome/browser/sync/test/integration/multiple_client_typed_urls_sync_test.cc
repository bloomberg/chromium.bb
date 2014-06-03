// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"

using typed_urls_helper::AddUrlToHistory;
using typed_urls_helper::AwaitCheckAllProfilesHaveSameURLsAsVerifier;
using typed_urls_helper::GetTypedUrlsFromClient;

class MultipleClientTypedUrlsSyncTest : public SyncTest {
 public:
  MultipleClientTypedUrlsSyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientTypedUrlsSyncTest() {}

  virtual bool TestUsesSelfNotifications() OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientTypedUrlsSyncTest);
};

// TCM: 3728323
IN_PROC_BROWSER_TEST_F(MultipleClientTypedUrlsSyncTest, AddToOne) {
  const base::string16 kHistoryUrl(
      base::ASCIIToUTF16("http://www.add-one-history.google.com/"));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate one client with a URL, should sync to all others.
  GURL new_url(kHistoryUrl);
  AddUrlToHistory(0, new_url);
  history::URLRows urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());

  // All clients should have this URL.
  ASSERT_TRUE(AwaitCheckAllProfilesHaveSameURLsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(MultipleClientTypedUrlsSyncTest, AddToAll) {
  const base::string16 kHistoryUrl(
      base::ASCIIToUTF16("http://www.add-all-history.google.com/"));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Populate clients with the same URL.
  for (int i = 0; i < num_clients(); ++i) {
    history::URLRows urls = GetTypedUrlsFromClient(i);
    ASSERT_EQ(0U, urls.size());

    base::string16 unique_url = kHistoryUrl + base::FormatNumber(i);
    GURL new_url(unique_url);
    AddUrlToHistory(i, new_url);

    urls = GetTypedUrlsFromClient(i);
    ASSERT_EQ(1U, urls.size());
    ASSERT_EQ(new_url, urls[0].url());
  }

  // Verify that all clients have all urls.
  ASSERT_TRUE(AwaitCheckAllProfilesHaveSameURLsAsVerifier());
}
