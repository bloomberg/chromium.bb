// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class ChromeDoNotTrackTest : public InProcessBrowserTest {
 protected:
  void SetEnableDoNotTrack(bool enabled) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kEnableDoNotTrack, enabled);
  }

  void ExpectPageTextEq(const std::string& expected_content) {
    std::string text;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.body.innerText);",
        &text));
    EXPECT_EQ(expected_content, text);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, NotEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(false /* enabled */);

  GURL url = embedded_test_server()->GetURL("/echoheader?DNT");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(false,
            GetWebContents()->GetMutableRendererPrefs()->enable_do_not_track);
  ExpectPageTextEq("None");
}

IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, Enabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  GURL url = embedded_test_server()->GetURL("/echoheader?DNT");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(true,
            GetWebContents()->GetMutableRendererPrefs()->enable_do_not_track);
  ExpectPageTextEq("1");
}

// Checks that the DNT header is preserved when fetching from a dedicated
// worker.
IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, FetchFromWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  const GURL fetch_url = embedded_test_server()->GetURL("/echoheader?DNT");
  const GURL url = embedded_test_server()->GetURL(
      std::string("/workers/fetch_from_worker.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  const std::string script = "fetch_from_worker('" + fetch_url.spec() + "');";
  ASSERT_TRUE(ExecJs(GetWebContents(), script));
  const base::string16 title = base::ASCIIToUTF16("DONE");
  {
    content::TitleWatcher watcher(GetWebContents(), title);
    EXPECT_EQ(title, watcher.WaitAndGetTitle());
  }
  ExpectPageTextEq("1");

  // Updating settings should be reflected immediately.
  // Disabled due to crbug.com/853085.
  //
  // SetEnableDoNotTrack(false /* enabled */);
  // ASSERT_TRUE(ExecJs(GetWebContents(), script));
  // {
  //   content::TitleWatcher watcher(GetWebContents(), title);
  //   EXPECT_EQ(title, watcher.WaitAndGetTitle());
  // }
  // ExpectPageTextEq("None");
}

// Checks that the DNT header is preserved when fetching from a shared worker.
//
// Disabled on Android since a shared worker is not available on Android:
// crbug.com/869745.
#if defined(OS_ANDROID)
#define MAYBE_FetchFromSharedWorker DISABLED_FetchFromSharedWorker
#else
#define MAYBE_FetchFromSharedWorker FetchFromSharedWorker
#endif
IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, MAYBE_FetchFromSharedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  const GURL fetch_url = embedded_test_server()->GetURL("/echoheader?DNT");
  const GURL url = embedded_test_server()->GetURL(
      std::string("/workers/fetch_from_shared_worker.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  const std::string script =
      "fetch_from_shared_worker('" + fetch_url.spec() + "');";
  ASSERT_TRUE(ExecJs(GetWebContents(), script));
  const base::string16 title = base::ASCIIToUTF16("DONE");
  {
    content::TitleWatcher watcher(GetWebContents(), title);
    EXPECT_EQ(title, watcher.WaitAndGetTitle());
  }
  ExpectPageTextEq("1");

  // Updating settings should be reflected immediately.
  // Disabled due to crbug.com/853085.
  //
  // SetEnableDoNotTrack(false /* enabled */);
  // ASSERT_TRUE(ExecJs(GetWebContents(), script));
  // {
  //   content::TitleWatcher watcher(GetWebContents(), title);
  //   EXPECT_EQ(title, watcher.WaitAndGetTitle());
  // }
  // ExpectPageTextEq("None");
}

// Checks that the DNT header is preserved when fetching from a service worker.
IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, FetchFromServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  const GURL fetch_url = embedded_test_server()->GetURL("/echoheader?DNT");
  const GURL url = embedded_test_server()->GetURL(
      std::string("/workers/fetch_from_service_worker.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Wait until service worker become ready.
  const base::string16 title = base::ASCIIToUTF16("DONE");
  {
    content::TitleWatcher watcher(GetWebContents(), title);
    EXPECT_EQ(title, watcher.WaitAndGetTitle());
  }
  ExpectPageTextEq("ready");

  const std::string script =
      "fetch_from_service_worker('" + fetch_url.spec() + "');";
  ASSERT_TRUE(ExecJs(GetWebContents(), script));
  {
    content::TitleWatcher watcher(GetWebContents(), title);
    EXPECT_EQ(title, watcher.WaitAndGetTitle());
  }
  ExpectPageTextEq("1");

  // Updating settings should be reflected immediately.
  SetEnableDoNotTrack(false /* enabled */);
  ASSERT_TRUE(ExecJs(GetWebContents(), script));
  {
    content::TitleWatcher watcher(GetWebContents(), title);
    EXPECT_EQ(title, watcher.WaitAndGetTitle());
  }
  ExpectPageTextEq("None");
}

}  // namespace
