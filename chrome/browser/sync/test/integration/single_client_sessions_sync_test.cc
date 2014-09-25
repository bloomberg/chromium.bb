// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "components/history/core/browser/history_types.h"
#include "sync/util/time.h"

using sessions_helper::CheckInitialState;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::OpenTabAndGetLocalWindows;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SessionWindowMap;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WindowsMatch;
using sync_integration_test_util::AwaitCommitActivityCompletion;
using typed_urls_helper::GetUrlFromClient;

class SingleClientSessionsSyncTest : public SyncTest {
 public:
  SingleClientSessionsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientSessionsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSessionsSyncTest);
};

// Timeout on Windows, see http://crbug.com/99819
#if defined(OS_WIN)
#define MAYBE_Sanity DISABLED_Sanity
#else
#define MAYBE_Sanity Sanity
#endif

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, MAYBE_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  // Add a new session to client 0 and wait for it to sync.
  ScopedWindowMap old_windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0,
                                        GURL("http://127.0.0.1/bubba"),
                                        old_windows.GetMutable()));
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  // Get foreign session data from client 0.
  SyncedSessionVector sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  ASSERT_EQ(0U, sessions.size());

  // Verify client didn't change.
  ScopedWindowMap new_windows;
  ASSERT_TRUE(GetLocalWindows(0, new_windows.GetMutable()));
  ASSERT_TRUE(WindowsMatch(*old_windows.Get(), *new_windows.Get()));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, TimestampMatchesHistory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  // We want a URL that doesn't 404 and has a non-empty title.
  // about:version is simple to render, too.
  const GURL url("about:version");

  ScopedWindowMap windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, url, windows.GetMutable()));

  int found_navigations = 0;
  for (SessionWindowMap::const_iterator it = windows.Get()->begin();
       it != windows.Get()->end(); ++it) {
    for (std::vector<SessionTab*>::const_iterator it2 =
             it->second->tabs.begin(); it2 != it->second->tabs.end(); ++it2) {
      for (std::vector<sessions::SerializedNavigationEntry>::const_iterator
               it3 = (*it2)->navigations.begin();
           it3 != (*it2)->navigations.end(); ++it3) {
        const base::Time timestamp = it3->timestamp();

        history::URLRow virtual_row;
        ASSERT_TRUE(GetUrlFromClient(0, it3->virtual_url(), &virtual_row));
        const base::Time history_timestamp = virtual_row.last_visit();

        ASSERT_EQ(timestamp, history_timestamp);
        ++found_navigations;
      }
    }
  }
  ASSERT_EQ(1, found_navigations);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, ResponseCodeIsPreserved) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  // We want a URL that doesn't 404 and has a non-empty title.
  // about:version is simple to render, too.
  const GURL url("about:version");

  ScopedWindowMap windows;
  ASSERT_TRUE(OpenTabAndGetLocalWindows(0, url, windows.GetMutable()));

  int found_navigations = 0;
  for (SessionWindowMap::const_iterator it = windows.Get()->begin();
       it != windows.Get()->end(); ++it) {
    for (std::vector<SessionTab*>::const_iterator it2 =
             it->second->tabs.begin(); it2 != it->second->tabs.end(); ++it2) {
      for (std::vector<sessions::SerializedNavigationEntry>::const_iterator
               it3 = (*it2)->navigations.begin();
           it3 != (*it2)->navigations.end(); ++it3) {
        EXPECT_EQ(200, it3->http_status_code());
        ++found_navigations;
      }
    }
  }
  ASSERT_EQ(1, found_navigations);
}
