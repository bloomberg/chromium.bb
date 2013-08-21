// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/app_launch_controller.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace net::test_server;

namespace chromeos {

namespace {

const char kWebstoreDomain[] = "cws.com";
const base::FilePath kServiceLogin("chromeos/service_login.html");

// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ggbflgnkafappblpkiflbgpmkfdpnhhe
const char kTestKioskApp[] = "ggbflgnkafappblpkiflbgpmkfdpnhhe";

// Helper function for GetConsumerKioskModeStatusCallback.
void ConsumerKioskModeStatusCheck(
    KioskAppManager::ConsumerKioskModeStatus* out_status,
    const base::Closure& runner_quit_task,
    KioskAppManager::ConsumerKioskModeStatus in_status) {
  LOG(INFO) << "KioskAppManager::ConsumerKioskModeStatus = " << in_status;
  *out_status = in_status;
  runner_quit_task.Run();
}

// Helper KioskAppManager::EnableKioskModeCallback implementation.
void ConsumerKioskModeLockCheck(
    bool* out_locked,
    const base::Closure& runner_quit_task,
    bool in_locked) {
  LOG(INFO) << "kioks locked  = " << in_locked;
  *out_locked = in_locked;
  runner_quit_task.Run();
}

}  // namespace

class KioskTest : public InProcessBrowserTest,
                  // Param defining is multi-profiles enabled.
                  public testing::WithParamInterface<bool> {
 public:
  KioskTest() {
    set_exit_when_last_browser_closes(false);
  }

  virtual ~KioskTest() {}

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    test_server_.reset(new EmbeddedTestServer(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO)));
    CHECK(test_server_->InitializeAndWaitUntilReady());
    test_server_->RegisterRequestHandler(
        base::Bind(&KioskTest::HandleRequest, base::Unretained(this)));
    LOG(INFO) << "Set up http server at " << test_server_->base_url();

    const GURL gaia_url("http://localhost:" + test_server_->base_url().port());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kGaiaUrl, gaia_url.spec());
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHostImpl::default_host()) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::Bind(&chrome::AttemptExit));
      content::RunMessageLoop();
    }

    // Clean up while main thread still runs.
    // See http://crbug.com/176659.
    KioskAppManager::Get()->CleanUp();

    LOG(INFO) << "Stopping the http server.";
    EXPECT_TRUE(test_server_->ShutdownAndWaitUntilComplete());
    test_server_.reset();  // Destructor wants UI thread.
  }

  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    GURL url = test_server_->GetURL(request.relative_url);
    LOG(INFO) << "Http request: " << url.spec();

    scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    if (url.path() == "/ServiceLogin") {
      http_response->set_code(net::HTTP_OK);
      http_response->set_content(service_login_response_);
      http_response->set_content_type("text/html");
    } else if (url.path() == "/ServiceLoginAuth") {
      LOG(INFO) << "Params: " << request.content;
      static const char kContinueParam[] = "continue=";
      int continue_arg_begin = request.content.find(kContinueParam) +
          arraysize(kContinueParam) - 1;
      int continue_arg_end = request.content.find("&", continue_arg_begin);
      const std::string continue_url = request.content.substr(
          continue_arg_begin, continue_arg_end - continue_arg_begin);
      http_response->set_code(net::HTTP_OK);
      const std::string redirect_js =
          "document.location.href = unescape('" + continue_url + "');";
      http_response->set_content(
          "<HTML><HEAD><SCRIPT>\n" + redirect_js + "\n</SCRIPT></HEAD></HTML>");
      http_response->set_content_type("text/html");
    } else {
      LOG(ERROR) << "Unsupported url: " << url.path();
    }
    return http_response.PassAs<HttpResponse>();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    if (GetParam()) {
      command_line->AppendSwitch(::switches::kMultiProfiles);
    }
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(
        chromeos::switches::kDisableChromeCaptivePortalDetector);
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");

    ASSERT_TRUE(test_server()->Start());
    net::HostPortPair host_port = test_server()->host_port_pair();
    std::string test_gallery_url = base::StringPrintf(
        "http://%s:%d/files/chromeos/app_mode/webstore",
        kWebstoreDomain, host_port.port());
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryURL, test_gallery_url);

    std::string test_gallery_download_url = test_gallery_url;
    test_gallery_download_url.append("/downloads/%s.crx");
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryDownloadURL, test_gallery_download_url);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    CHECK(file_util::ReadFileToString(test_data_dir.Append(kServiceLogin),
                                      &service_login_response_));

    host_resolver()->AddRule(kWebstoreDomain, "127.0.0.1");
  }

  void ReloadKioskApps() {
    KioskAppManager::Get()->AddApp(kTestKioskApp);
  }

  void ReloadAutolaunchKioskApps() {
    KioskAppManager::Get()->AddApp(kTestKioskApp);
    KioskAppManager::Get()->SetAutoLaunchApp(kTestKioskApp);
  }

  void EnableConsumerKioskMode() {
    scoped_ptr<bool> locked(new bool(false));
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    KioskAppManager::Get()->EnableConsumerModeKiosk(
        base::Bind(&ConsumerKioskModeLockCheck,
                   locked.get(),
                   runner->QuitClosure()));
    runner->Run();
    EXPECT_TRUE(*locked.get());
  }

  KioskAppManager::ConsumerKioskModeStatus GetConsumerKioskModeStatus() {
    KioskAppManager::ConsumerKioskModeStatus status =
        static_cast<KioskAppManager::ConsumerKioskModeStatus>(-1);
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    KioskAppManager::Get()->GetConsumerKioskModeStatus(
        base::Bind(&ConsumerKioskModeStatusCheck,
                   &status,
                   runner->QuitClosure()));
    runner->Run();
    CHECK_NE(status, static_cast<KioskAppManager::ConsumerKioskModeStatus>(-1));
    return status;
  }

  content::WebUI* GetLoginUI() {
    return static_cast<chromeos::LoginDisplayHostImpl*>(
        chromeos::LoginDisplayHostImpl::default_host())->GetOobeUI()->web_ui();
  }

  std::string service_login_response_;
  scoped_ptr<EmbeddedTestServer> test_server_;
 };

IN_PROC_BROWSER_TEST_P(KioskTest, InstallAndLaunchApp) {
  EnableConsumerKioskMode();
  chromeos::AppLaunchController::SkipSplashWaitForTesting();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting();

  ReloadKioskApps();

  // Wait for the Kiosk App configuration to reload, then launch the app.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APPS_LOADED,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("login.AppsMenuButton.runAppForTesting",
                                       base::StringValue(kTestKioskApp));

  // Wait for the Kiosk App to launch.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
      content::NotificationService::AllSources()).Wait();

  // Check installer status.
  EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
            chromeos::KioskAppLaunchError::Get());

  // Check if the kiosk webapp is really installed for the default profile.
  ASSERT_TRUE(ProfileManager::GetDefaultProfile());
  const extensions::Extension* app =
      extensions::ExtensionSystem::Get(ProfileManager::GetDefaultProfile())->
      extension_service()->GetInstalledExtension(kTestKioskApp);
  EXPECT_TRUE(app);

  // Wait until the app terminates.
  content::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_P(KioskTest, AutolaunchWarningCancel) {
  EnableConsumerKioskMode();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  ReloadAutolaunchKioskApps();
  wizard_controller->SkipToLoginForTesting();

  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the auto launch warning come up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.AutolaunchScreen.confirmAutoLaunchForTesting",
      base::FundamentalValue(false));

  // Wait for the auto launch warning to go away.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());
}

IN_PROC_BROWSER_TEST_P(KioskTest, AutolaunchWarningConfirm) {
  EnableConsumerKioskMode();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting();

  ReloadAutolaunchKioskApps();
  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the auto launch warning come up.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.AutolaunchScreen.confirmAutoLaunchForTesting",
      base::FundamentalValue(true));

  // Wait for the auto launch warning to go away.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  EXPECT_FALSE(KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_TRUE(KioskAppManager::Get()->IsAutoLaunchEnabled());

  // Wait for the Kiosk App to launch.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
      content::NotificationService::AllSources()).Wait();

  // Check installer status.
  EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
            chromeos::KioskAppLaunchError::Get());

  // Check if the kiosk webapp is really installed for the default profile.
  ASSERT_TRUE(ProfileManager::GetDefaultProfile());
  const extensions::Extension* app =
      extensions::ExtensionSystem::Get(ProfileManager::GetDefaultProfile())->
      extension_service()->GetInstalledExtension(kTestKioskApp);
  EXPECT_TRUE(app);

  // Wait until the app terminates.
  content::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_P(KioskTest, KioskEnableCancel) {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE,
            GetConsumerKioskModeStatus());

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting();
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // Wait for the kiosk_enable screen to show and cancel the screen.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.KioskEnableScreen.enableKioskForTesting",
      base::FundamentalValue(false));

  // Wait for the kiosk_enable screen to disappear.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED,
      content::NotificationService::AllSources()).Wait();

  // Check that the status still says configurable.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE,
            GetConsumerKioskModeStatus());
}

IN_PROC_BROWSER_TEST_P(KioskTest, KioskEnableConfirmed) {
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  // Check Kiosk mode status.
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE,
            GetConsumerKioskModeStatus());
  wizard_controller->SkipToLoginForTesting();

  // Wait for the login UI to come up and switch to the kiosk_enable screen.
  wizard_controller->SkipToLoginForTesting();
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));

  // Wait for the kiosk_enable screen to show and cancel the screen.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  GetLoginUI()->CallJavascriptFunction(
      "login.KioskEnableScreen.enableKioskForTesting",
      base::FundamentalValue(true));

  // Wait for the signal that indicates Kiosk Mode is enabled.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_KIOSK_ENABLED,
      content::NotificationService::AllSources()).Wait();
  EXPECT_EQ(KioskAppManager::CONSUMER_KIOSK_MODE_ENABLED,
            GetConsumerKioskModeStatus());
}

INSTANTIATE_TEST_CASE_P(KioskTestInstantiation, KioskTest, testing::Bool());

}  // namespace chromeos
