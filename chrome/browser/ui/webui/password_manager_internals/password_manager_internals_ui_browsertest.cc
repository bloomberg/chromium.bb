// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/password_manager_internals/password_manager_internals_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browsertest.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "content/public/browser/web_contents.h"

class PasswordManagerInternalsWebUIBrowserTest : public WebUIBrowserTest {
 public:
  PasswordManagerInternalsWebUIBrowserTest();
  virtual ~PasswordManagerInternalsWebUIBrowserTest();

  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

 protected:
  content::WebContents* GetWebContents();

  // Opens a new tab, and navigates to the internals page in that. Also assigns
  // the corresponding UI controller to |controller_|.
  void OpenNewTabWithTheInternalsPage();

  PasswordManagerInternalsUI* controller() {
    return controller_;
  };

 private:
  PasswordManagerInternalsUI* controller_;
};

PasswordManagerInternalsWebUIBrowserTest::
    PasswordManagerInternalsWebUIBrowserTest()
    : controller_(NULL) {}

PasswordManagerInternalsWebUIBrowserTest::
    ~PasswordManagerInternalsWebUIBrowserTest() {}

void PasswordManagerInternalsWebUIBrowserTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();
  OpenNewTabWithTheInternalsPage();
}

void PasswordManagerInternalsWebUIBrowserTest::SetUpCommandLine(
    CommandLine* command_line) {
  command_line->AppendSwitch(
      password_manager::switches::kEnablePasswordManagerInternalsUI);
}

content::WebContents*
PasswordManagerInternalsWebUIBrowserTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void
PasswordManagerInternalsWebUIBrowserTest::OpenNewTabWithTheInternalsPage() {
  std::string url_string("chrome://");
  url_string += chrome::kChromeUIPasswordManagerInternalsHost;
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url_string),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  controller_ = static_cast<PasswordManagerInternalsUI*>(
      GetWebContents()->GetWebUI()->GetController());
  AddLibrary(base::FilePath(
      FILE_PATH_LITERAL("password_manager_internals_browsertest.js")));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogSavePasswordProgress) {
  controller()->LogSavePasswordProgress("<script> text for testing");
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
}

// Test that if two tabs with the internals page are open, the second displays
// the same logs. In particular, this checks that both the second tab gets the
// logs created before the second tab was opened, and also that the second tab
// waits with displaying until the internals page is ready (trying to display
// the old logs just on construction time would fail).
// TODO(vabr): Disabled until multiple tabs with the internals page can exist
// without crashing.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       DISABLED_LogSavePasswordProgress_MultipleTabsIdentical) {
  // First, open one tab with the internals page, and log something.
  controller()->LogSavePasswordProgress("<script> text for testing");
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
  // Now open a second tab with the internals page, but do not log anything.
  OpenNewTabWithTheInternalsPage();
  // The previously logged text should have made it to the page.
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
}
