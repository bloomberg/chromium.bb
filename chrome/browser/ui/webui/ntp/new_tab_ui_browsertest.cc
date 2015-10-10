// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

using content::OpenURLParams;
using content::Referrer;

namespace {

static bool had_console_errors = false;

bool HandleMessage(int severity,
                   const char* file,
                   int line,
                   size_t message_start,
                   const std::string& str) {
  if (severity == logging::LOG_ERROR && file && file == std::string("CONSOLE"))
    had_console_errors = true;
  return false;
}

}  // namespace

class NewTabUIBrowserTest : public InProcessBrowserTest {
 public:
  NewTabUIBrowserTest() {
    logging::SetLogMessageHandler(&HandleMessage);
  }

  ~NewTabUIBrowserTest() override { logging::SetLogMessageHandler(NULL); }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    ASSERT_FALSE(had_console_errors);
  }
};

// TODO(samarth): delete along with rest of NTP4 code.
// Loads chrome://hang/ into two NTP tabs, ensuring we don't crash.
// See http://crbug.com/59859.
// If this flakes, use http://crbug.com/87200.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, DISABLED_ChromeHangInNTP) {
  // Bring up a new tab page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Navigate to chrome://hang/ to stall the process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(content::kChromeUIHangURL), CURRENT_TAB, 0);

  // Visit chrome://hang/ again in another NTP. Don't bother waiting for the
  // NTP to load, because it's hung.
  chrome::NewTab(browser());
  browser()->OpenURL(OpenURLParams(
      GURL(content::kChromeUIHangURL), Referrer(), CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));
}

// Navigate to incognito NTP. Fails if there are console errors.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, ShowIncognito) {
  ui_test_utils::NavigateToURL(CreateIncognitoBrowser(),
                               GURL(chrome::kChromeUINewTabURL));
}

class NewTabUIProcessPerTabTest : public NewTabUIBrowserTest {
 public:
   NewTabUIProcessPerTabTest() {}

   void SetUpCommandLine(base::CommandLine* command_line) override {
     command_line->AppendSwitch(switches::kProcessPerTab);
   }
};

// Navigates away from NTP before it commits, in process-per-tab mode.
// Ensures that we don't load the normal page in the NTP process (and thus
// crash), as in http://crbug.com/69224.
// If this flakes, use http://crbug.com/87200
IN_PROC_BROWSER_TEST_F(NewTabUIProcessPerTabTest, NavBeforeNTPCommits) {
  // Bring up a new tab page.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  // Navigate to chrome://hang/ to stall the process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(content::kChromeUIHangURL), CURRENT_TAB, 0);

  // Visit a normal URL in another NTP that hasn't committed.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB, 0);

  // We don't use ui_test_utils::NavigateToURLWithDisposition because that waits
  // for current loading to stop.
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->OpenURL(OpenURLParams(
      GURL("data:text/html,hello world"), Referrer(), CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));
  observer.Wait();
}
