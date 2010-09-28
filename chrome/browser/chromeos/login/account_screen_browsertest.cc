// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/login/account_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_about_job.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class AccountScreenTest : public WizardInProcessBrowserTest {
 public:
  AccountScreenTest(): WizardInProcessBrowserTest("account") {
  }

 protected:
  // Overridden from WizardInProcessBrowserTest:
  virtual void SetUpWizard() {
    ASSERT_TRUE(test_server()->Start());
    GURL new_account_page_url(test_server()->GetURL("files/new_account.html"));
    AccountScreen::set_new_account_page_url(new_account_page_url);
    AccountScreen::set_check_for_https(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountScreenTest);
};

// A basic test. It does not care how things evolve after the URL is
// loaded. Thus no message loop is started. Just check that initial
// status is expected.
IN_PROC_BROWSER_TEST_F(AccountScreenTest, TestBasic) {
  ASSERT_TRUE(controller());
  EXPECT_EQ(controller()->GetAccountScreen(), controller()->current_screen());
}

static void QuitUIMessageLoop() {
  MessageLoopForUI::current()->Quit();
}

static bool inspector_called = false;  // had to use global flag as
                                       // InspectorHook() doesn't have context.

static URLRequestJob* InspectorHook(URLRequest* request,
                                    const std::string& scheme) {
  LOG(INFO) << "Intercepted: " << request->url() << ", scheme: " << scheme;

  // Expect that the parameters are the same as new_account.html gave us.
  EXPECT_STREQ("cros://inspector/?param1=value1+param2",
               request->url().spec().c_str());
  inspector_called = true;
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
                         NewRunnableFunction(QuitUIMessageLoop));

  // Do not navigate to the given URL. Navigate to about:blank instead.
  return new URLRequestAboutJob(request);
}

IN_PROC_BROWSER_TEST_F(AccountScreenTest, FLAKY_TestSchemeInspector) {
  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      chrome::kCrosScheme);
  URLRequestFilter::GetInstance()->AddHostnameHandler(chrome::kCrosScheme,
                                                      "inspector",
                                                      &InspectorHook);
  EXPECT_FALSE(inspector_called);
  ui_test_utils::RunMessageLoop();
  EXPECT_TRUE(inspector_called);
}

}  // namespace chromeos
