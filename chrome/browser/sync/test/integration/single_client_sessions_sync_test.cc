// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/session_hierarchy_match_checker.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/typed_urls_helper.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/session_types.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/test/fake_server/sessions_hierarchy.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using base::HistogramBase;
using base::HistogramSamples;
using base::HistogramTester;
using fake_server::SessionsHierarchy;
using sessions_helper::CheckInitialState;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::ModelAssociatorHasTabWithUrl;
using sessions_helper::MoveTab;
using sessions_helper::NavigateTab;
using sessions_helper::NavigateTabBack;
using sessions_helper::NavigateTabForward;
using sessions_helper::OpenTab;
using sessions_helper::OpenTabAtIndex;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SessionWindowMap;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WaitForTabsToLoad;
using sessions_helper::WindowsMatch;
using typed_urls_helper::GetUrlFromClient;

namespace {

void ExpectUniqueSampleGE(const HistogramTester& histogram_tester,
                          const std::string& name,
                          HistogramBase::Sample sample,
                          HistogramBase::Count expected_inclusive_lower_bound) {
  std::unique_ptr<HistogramSamples> samples =
      histogram_tester.GetHistogramSamplesSinceCreation(name);
  int sample_count = samples->GetCount(sample);
  EXPECT_GE(expected_inclusive_lower_bound, sample_count);
  EXPECT_EQ(sample_count, samples->TotalCount());
}

}  // namespace

class SingleClientSessionsSyncTest : public SyncTest {
 public:
  SingleClientSessionsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientSessionsSyncTest() override {}

  void SetUpCommandLine(base::CommandLine* cl) override {
    // This is a hacky override of the switches set in
    // SyncTest::SetUpCommandLine() to avoid the switch that speeds up nudge
    // delays. CookieJarMismatch asserts exact histogram counts assuming that
    // sync is relatively slow, so we preserve that assumption.
    if (!cl->HasSwitch(switches::kDisableBackgroundNetworking))
      cl->AppendSwitch(switches::kDisableBackgroundNetworking);

    if (!cl->HasSwitch(switches::kSyncShortInitialRetryOverride))
      cl->AppendSwitch(switches::kSyncShortInitialRetryOverride);

#if defined(OS_CHROMEOS)
    // kIgnoreUserProfileMappingForTests will let UserManager always return
    // active user. If this switch is not set, sync test's profile will not
    // match UserManager's active user, then UserManager won't return active
    // user to our tests.
    if (!cl->HasSwitch(chromeos::switches::kIgnoreUserProfileMappingForTests))
      cl->AppendSwitch(chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void ExpectNavigationChain(const std::vector<GURL>& urls) {
    ScopedWindowMap windows;
    ASSERT_TRUE(GetLocalWindows(0, &windows));
    ASSERT_EQ(windows.begin()->second->tabs.size(), 1u);
    sessions::SessionTab* tab = windows.begin()->second->tabs[0].get();

    int index = 0;
    EXPECT_EQ(urls.size(), tab->navigations.size());
    for (auto it = tab->navigations.begin(); it != tab->navigations.end();
         ++it, ++index) {
      EXPECT_EQ(urls[index], it->virtual_url());
    }
  }

  // Block until the expected hierarchy is recorded on the FakeServer for
  // profile 0. This will time out if the hierarchy is never
  // recorded.
  void WaitForHierarchyOnServer(
      const fake_server::SessionsHierarchy& hierarchy) {
    SessionHierarchyMatchChecker checker(hierarchy, GetSyncService(0),
                                         GetFakeServer());
    EXPECT_TRUE(checker.Wait());
  }

  // Shortcut to call WaitForHierarchyOnServer for only |url| in a single
  // window.
  void WaitForURLOnServer(const GURL& url) {
    WaitForHierarchyOnServer({{url.spec()}});
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSessionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  // Add a new session to client 0 and wait for it to sync.
  ScopedWindowMap old_windows;
  GURL url = GURL("http://127.0.0.1/bubba");
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(GetLocalWindows(0, &old_windows));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Get foreign session data from client 0.
  SyncedSessionVector sessions;
  ASSERT_FALSE(GetSessionData(0, &sessions));
  ASSERT_EQ(0U, sessions.size());

  // Verify client didn't change.
  ScopedWindowMap new_windows;
  ASSERT_TRUE(GetLocalWindows(0, &new_windows));
  ASSERT_TRUE(WindowsMatch(old_windows, new_windows));

  WaitForURLOnServer(url);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, NoSessions) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  WaitForHierarchyOnServer(SessionsHierarchy());
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, ChromeHistory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  ASSERT_TRUE(OpenTab(0, GURL(chrome::kChromeUIHistoryURL)));
  WaitForURLOnServer(GURL(chrome::kChromeUIHistoryURL));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, TimestampMatchesHistory) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  // We want a URL that doesn't 404 and has a non-empty title.
  const GURL url("data:text/html,<html><title>Test</title></html>");

  ScopedWindowMap windows;
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(GetLocalWindows(0, &windows));

  int found_navigations = 0;
  for (auto it = windows.begin(); it != windows.end(); ++it) {
    for (auto it2 = it->second->tabs.begin(); it2 != it->second->tabs.end();
         ++it2) {
      for (auto it3 = (*it2)->navigations.begin();
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
  const GURL url("data:text/html,<html><title>Test</title></html>");

  ScopedWindowMap windows;
  ASSERT_TRUE(OpenTab(0, url));
  ASSERT_TRUE(GetLocalWindows(0, &windows));

  int found_navigations = 0;
  for (auto it = windows.begin(); it != windows.end(); ++it) {
    for (auto it2 = it->second->tabs.begin(); it2 != it->second->tabs.end();
         ++it2) {
      for (auto it3 = (*it2)->navigations.begin();
           it3 != (*it2)->navigations.end(); ++it3) {
        EXPECT_EQ(200, it3->http_status_code());
        ++found_navigations;
      }
    }
  }
  ASSERT_EQ(1, found_navigations);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, FragmentURLNavigation) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL url = GURL("http://127.0.0.1/bubba");
  ASSERT_TRUE(OpenTab(0, url));
  WaitForURLOnServer(url);

  GURL fragment_url = GURL("http://127.0.0.1/bubba#fragment");
  NavigateTab(0, fragment_url);
  WaitForURLOnServer(fragment_url);
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       NavigationChainForwardBack) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL first_url = GURL("http://127.0.0.1/foobar");
  ASSERT_TRUE(OpenTab(0, first_url));
  WaitForURLOnServer(first_url);

  GURL second_url = GURL("http://127.0.0.1/barbaz");
  NavigateTab(0, second_url);
  WaitForURLOnServer(second_url);

  NavigateTabBack(0);
  WaitForURLOnServer(first_url);

  ExpectNavigationChain({first_url, second_url});

  NavigateTabForward(0);
  WaitForURLOnServer(second_url);

  ExpectNavigationChain({first_url, second_url});
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest,
                       NavigationChainAlteredDestructively) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url = GURL("http://127.0.0.1/bubba");
  ASSERT_TRUE(OpenTab(0, base_url));
  WaitForURLOnServer(base_url);

  GURL first_url = GURL("http://127.0.0.1/foobar");
  ASSERT_TRUE(NavigateTab(0, first_url));
  WaitForURLOnServer(first_url);

  // Check that the navigation chain matches the above sequence of {base_url,
  // first_url}.
  ExpectNavigationChain({base_url, first_url});

  NavigateTabBack(0);
  WaitForURLOnServer(base_url);

  GURL second_url = GURL("http://127.0.0.1/barbaz");
  NavigateTab(0, second_url);
  WaitForURLOnServer(second_url);

  NavigateTabBack(0);
  WaitForURLOnServer(base_url);

  // Check that the navigation chain contains second_url where first_url was
  // before.
  ExpectNavigationChain({base_url, second_url});
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, OpenNewTab) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url = GURL("http://127.0.0.1/bubba");
  ASSERT_TRUE(OpenTabAtIndex(0, 0, base_url));

  WaitForURLOnServer(base_url);

  GURL new_tab_url = GURL("http://127.0.0.1/foobar");
  ASSERT_TRUE(OpenTabAtIndex(0, 1, new_tab_url));

  WaitForHierarchyOnServer(
      SessionsHierarchy({{base_url.spec(), new_tab_url.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, OpenNewWindow) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url = GURL("http://127.0.0.1/bubba");
  ASSERT_TRUE(OpenTab(0, base_url));

  WaitForURLOnServer(base_url);

  GURL new_window_url = GURL("http://127.0.0.1/foobar");
  AddBrowser(0);
  ASSERT_TRUE(OpenTab(1, new_window_url));

  WaitForHierarchyOnServer(
      SessionsHierarchy({{base_url.spec()}, {new_window_url.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, TabMovedToOtherWindow) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(CheckInitialState(0));

  GURL base_url = GURL("http://127.0.0.1/bubba");
  GURL moved_tab_url = GURL("http://127.0.0.1/foobar");

  ASSERT_TRUE(OpenTab(0, base_url));
  ASSERT_TRUE(OpenTabAtIndex(0, 1, moved_tab_url));

  GURL new_window_url = GURL("http://127.0.0.1/barbaz");
  AddBrowser(0);
  ASSERT_TRUE(OpenTab(1, new_window_url));

  WaitForHierarchyOnServer(SessionsHierarchy(
      {{base_url.spec(), moved_tab_url.spec()}, {new_window_url.spec()}}));

  // Move tab 1 in browser 0 to browser 1.
  MoveTab(0, 1, 1);

  WaitForHierarchyOnServer(SessionsHierarchy(
      {{base_url.spec()}, {new_window_url.spec(), moved_tab_url.spec()}}));
}

IN_PROC_BROWSER_TEST_F(SingleClientSessionsSyncTest, CookieJarMismatch) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));

  ScopedWindowMap old_windows;
  sync_pb::ClientToServerMessage message;

  // The HistogramTester objects are scoped to allow more precise verification.
  {
    HistogramTester histogram_tester;

    // Add a new session to client 0 and wait for it to sync.
    GURL url = GURL("http://127.0.0.1/bubba");
    ASSERT_TRUE(OpenTab(0, url));
    WaitForURLOnServer(url);

    // The cookie jar mismatch value will be true by default due to
    // the way integration tests trigger signin (which does not involve a normal
    // web content signin flow).
    ASSERT_TRUE(GetFakeServer()->GetLastCommitMessage(&message));
    ASSERT_TRUE(message.commit().config_params().cookie_jar_mismatch());

    // It is possible that multiple sync cycles occured during the call to
    // OpenTab, which would cause multiple identical samples.
    ExpectUniqueSampleGE(histogram_tester, "Sync.CookieJarMatchOnNavigation",
                         false, 1);
    ExpectUniqueSampleGE(histogram_tester, "Sync.CookieJarEmptyOnMismatch",
                         true, 1);

    // Trigger a cookie jar change (user signing in to content area).
    gaia::ListedAccount signed_in_account;
    signed_in_account.id =
        GetClient(0)->service()->signin()->GetAuthenticatedAccountId();
    std::vector<gaia::ListedAccount> accounts;
    std::vector<gaia::ListedAccount> signed_out_accounts;
    accounts.push_back(signed_in_account);
    GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
    GetClient(0)->service()->OnGaiaAccountsInCookieUpdated(
        accounts, signed_out_accounts, error);
  }

  {
    HistogramTester histogram_tester;

    // Trigger a sync and wait for it.
    GURL url = GURL("http://127.0.0.1/bubba2");
    ASSERT_TRUE(OpenTab(0, url));
    ASSERT_TRUE(GetLocalWindows(0, &old_windows));
    TriggerSyncForModelTypes(0, syncer::ModelTypeSet(syncer::SESSIONS));
    ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

    // Verify the cookie jar mismatch bool is set to false.
    ASSERT_TRUE(GetFakeServer()->GetLastCommitMessage(&message));
    ASSERT_FALSE(message.commit().config_params().cookie_jar_mismatch());

    // Verify the histograms were recorded properly.
    ExpectUniqueSampleGE(histogram_tester, "Sync.CookieJarMatchOnNavigation",
                         true, 1);
    histogram_tester.ExpectTotalCount("Sync.CookieJarEmptyOnMismatch", 0);
  }
}
