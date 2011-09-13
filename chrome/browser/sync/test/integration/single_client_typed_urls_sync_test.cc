// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"

using typed_urls_helper::AddUrlToHistory;
using typed_urls_helper::AssertAllProfilesHaveSameURLsAsVerifier;
using typed_urls_helper::GetTypedUrlsFromClient;

const std::string kSanityHistoryUrl = "http://www.sanity-history.google.com";

class SingleClientTypedUrlsSyncTest : public SyncTest {
 public:
  SingleClientTypedUrlsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientTypedUrlsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientTypedUrlsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  GURL new_url(kSanityHistoryUrl);
  AddUrlToHistory(0, new_url);

  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());
  AssertAllProfilesHaveSameURLsAsVerifier();

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for typed url change."));

  // Verify client did not change.
  AssertAllProfilesHaveSameURLsAsVerifier();
}

IN_PROC_BROWSER_TEST_F(SingleClientTypedUrlsSyncTest, TwoVisits) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  std::vector<history::URLRow> urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(0U, urls.size());

  GURL new_url(kSanityHistoryUrl);
  // Adding twice should add two visits with distinct timestamps.
  AddUrlToHistory(0, new_url);
  AddUrlToHistory(0, new_url);

  urls = GetTypedUrlsFromClient(0);
  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(new_url, urls[0].url());
  AssertAllProfilesHaveSameURLsAsVerifier();

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for typed url change."));

  // Verify client did not change.
  AssertAllProfilesHaveSameURLsAsVerifier();
}


