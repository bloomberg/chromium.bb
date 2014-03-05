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
  std::string url_string("chrome://");
  url_string += chrome::kChromeUIPasswordManagerInternalsHost;
  ui_test_utils::NavigateToURL(browser(), GURL(url_string));
  controller_ = static_cast<PasswordManagerInternalsUI*>(
      GetWebContents()->GetWebUI()->GetController());
  AddLibrary(base::FilePath(
      FILE_PATH_LITERAL("password_manager_internals_browsertest.js")));
}

void PasswordManagerInternalsWebUIBrowserTest::SetUpCommandLine(
    CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnablePasswordManagerInternalsUI);
}

content::WebContents*
PasswordManagerInternalsWebUIBrowserTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogSavePasswordProgress) {
  controller()->LogSavePasswordProgress("text test text");
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
}
