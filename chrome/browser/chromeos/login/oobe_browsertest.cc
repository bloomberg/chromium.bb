// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace net::test_server;

namespace {

// Used to add an observer to NotificationService after it's created.
class TestBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts,
      public content::NotificationObserver {
 public:
  TestBrowserMainExtraParts()
      : webui_visible_(false),
        browsing_data_removed_(false),
        signin_screen_shown_(false) {}
  virtual ~TestBrowserMainExtraParts() {}

  // ChromeBrowserMainExtraParts implementation.
  virtual void PreEarlyInitialization() OVERRIDE {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_BROWSING_DATA_REMOVED,
                   content::NotificationService::AllSources());
  }

  void set_quit_task(const base::Closure& quit_task) { quit_task_ = quit_task; }

 private:
  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE) {
      LOG(INFO) << "NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE";
      webui_visible_ = true;
      if (browsing_data_removed_ && !signin_screen_shown_) {
        signin_screen_shown_ = true;
        ShowSigninScreen();
      }
    } else if (type == chrome::NOTIFICATION_BROWSING_DATA_REMOVED) {
      LOG(INFO) << "chrome::NOTIFICATION_BROWSING_DATA_REMOVED";
      browsing_data_removed_ = true;
      if (webui_visible_ && !signin_screen_shown_) {
        signin_screen_shown_ = true;
        ShowSigninScreen();
      }
    } else if (type == chrome::NOTIFICATION_SESSION_STARTED) {
      LOG(INFO) << "chrome::NOTIFICATION_SESSION_STARTED";
      quit_task_.Run();
    } else {
      NOTREACHED();
    }
  }

  void ShowSigninScreen() {
    chromeos::ExistingUserController* controller =
        chromeos::ExistingUserController::current_controller();
    CHECK(controller);
    chromeos::WebUILoginDisplay* webui_login_display =
        static_cast<chromeos::WebUILoginDisplay*>(
            controller->login_display());
    CHECK(webui_login_display);
    webui_login_display->ShowSigninScreenForCreds("username", "password");
    // TODO(glotov): mock GAIA server (test_server()) should support
    // username/password configuration.
  }

  bool webui_visible_, browsing_data_removed_, signin_screen_shown_;
  content::NotificationRegistrar registrar_;
  base::Closure quit_task_;
  GURL gaia_url_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserMainExtraParts);
};

class TestContentBrowserClient : public chrome::ChromeContentBrowserClient {
 public:
  TestContentBrowserClient() {}
  virtual ~TestContentBrowserClient() {}

  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE {
    ChromeBrowserMainParts* main_parts = static_cast<ChromeBrowserMainParts*>(
        ChromeContentBrowserClient::CreateBrowserMainParts(parameters));

    browser_main_extra_parts_ = new TestBrowserMainExtraParts();
    main_parts->AddParts(browser_main_extra_parts_);
    return main_parts;
  }

  TestBrowserMainExtraParts* browser_main_extra_parts_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestContentBrowserClient);
};

class OobeTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(
        chromeos::switches::kDisableChromeCaptivePortalDetector);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(
        chromeos::switches::kAuthExtensionPath, "gaia_auth");
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    content_browser_client_.reset(new TestContentBrowserClient());
    original_content_browser_client_ = content::SetBrowserClientForTesting(
        content_browser_client_.get());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    CHECK(embedded_test_server()->InitializeAndWaitUntilReady());
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
    LOG(INFO) << "Set up http server at " << embedded_test_server()->base_url();

    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kGaiaUrl, embedded_test_server()->base_url().spec());
  }

  scoped_ptr<TestContentBrowserClient> content_browser_client_;
  content::ContentBrowserClient* original_content_browser_client_;
  FakeGaia fake_gaia_;
};

IN_PROC_BROWSER_TEST_F(OobeTest, NewUser) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting();

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  content_browser_client_->browser_main_extra_parts_->set_quit_task(
      runner->QuitClosure());
  runner->Run();
}

}  // namespace
