// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class HistoryApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    host_resolver()->AddRule("www.a.com", "127.0.0.1");
    host_resolver()->AddRule("www.b.com", "127.0.0.1");

    ASSERT_TRUE(StartTestServer());
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

// Full text search indexing sometimes exceeds a timeout. (http://crbug/119505)
IN_PROC_BROWSER_TEST_F(HistoryApiTest, DISABLED_MiscSearch) {
  ASSERT_TRUE(RunExtensionSubtest("history", "misc_search.html")) << message_;
}

// Same could happen here without the FTS (http://crbug/119505)
IN_PROC_BROWSER_TEST_F(HistoryApiTest, DISABLED_TimedSearch) {
  ASSERT_TRUE(RunExtensionSubtest("history", "timed_search.html")) << message_;
}

#if defined(OS_WIN)
// Flaky on Windows: crbug.com/88318
#define MAYBE_Delete DISABLED_Delete
#else
#define MAYBE_Delete Delete
#endif
IN_PROC_BROWSER_TEST_F(HistoryApiTest, MAYBE_Delete) {
  ASSERT_TRUE(RunExtensionSubtest("history", "delete.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(HistoryApiTest, DeleteProhibited) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, false);
  ASSERT_TRUE(RunExtensionSubtest("history", "delete_prohibited.html")) <<
      message_;
}

// See crbug.com/79074
IN_PROC_BROWSER_TEST_F(HistoryApiTest, DISABLED_GetVisits) {
  ASSERT_TRUE(RunExtensionSubtest("history", "get_visits.html")) << message_;
}

#if defined(OS_WIN)
// Searching for a URL right after adding it fails on win XP.
// Fix this as part of crbug/76170.
#define MAYBE_SearchAfterAdd DISABLED_SearchAfterAdd
#else
#define MAYBE_SearchAfterAdd SearchAfterAdd
#endif

IN_PROC_BROWSER_TEST_F(HistoryApiTest, MAYBE_SearchAfterAdd) {
  ASSERT_TRUE(RunExtensionSubtest("history", "search_after_add.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(HistoryApiTest, MostVisited) {
  // Add entries to the history database that we can query for (using the
  // extension history API for this doesn't work as it only adds URLs with
  // LINK transition type).
  HistoryService* hs = HistoryServiceFactory::GetForProfile(
      browser()->profile(),
      Profile::EXPLICIT_ACCESS);

  GURL google_url = GURL("http://www.google.com");
  base::Time google_time;
  ASSERT_TRUE(base::Time::FromString("Tue, 24 Apr 2012, 13:00:00",
                                     &google_time));
  hs->AddPage(google_url, google_time, NULL, 0, GURL(), history::RedirectList(),
              content::PAGE_TRANSITION_TYPED, history::SOURCE_EXTENSION, false);
  hs->SetPageTitle(google_url, UTF8ToUTF16("Google"));

  GURL picasa_url = GURL("http://www.picasa.com");
  base::Time picasa_time;
  ASSERT_TRUE(base::Time::FromString("Tue, 24 Apr 2012, 14:00:00",
                                     &picasa_time));
  hs->AddPage(picasa_url, picasa_time, NULL, 0, GURL(), history::RedirectList(),
              content::PAGE_TRANSITION_TYPED, history::SOURCE_EXTENSION, false);
  hs->SetPageTitle(picasa_url, UTF8ToUTF16("Picasa"));

  // Run the test.
  ASSERT_TRUE(RunExtensionSubtest("history", "most_visited.html"))
    << message_;
}

}  // namespace extensions
