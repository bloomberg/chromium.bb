// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google_apis/test_server/http_request.h"
#include "chrome/browser/google_apis/test_server/http_response.h"
#include "chrome/browser/google_apis/test_server/http_server.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace google_apis;
using namespace google_apis::test_server;

namespace chromeos {

namespace {

const char kWebstoreDomain[] = "cws.com";

// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ggbflgnkafappblpkiflbgpmkfdpnhhe
const char kTestKioskApp[] = "ggbflgnkafappblpkiflbgpmkfdpnhhe";

// Used to add an observer to NotificationService after it's created.
class TestBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts,
      public content::NotificationObserver {
 public:
  TestBrowserMainExtraParts() {}

  virtual ~TestBrowserMainExtraParts() {}

  // ChromeBrowserMainExtraParts implementation.
  virtual void PreEarlyInitialization() OVERRIDE {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_APPS_LOADED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
                   content::NotificationService::AllSources());
  }

  void set_quit_task(const base::Closure& quit_task) { quit_task_ = quit_task; }

 private:
  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE) {
      LOG(INFO) << "NOTIFICATION_LOGIN_WEBUI_VISIBLE";
    } else if (type == chrome::NOTIFICATION_KIOSK_APPS_LOADED) {
      LOG(INFO) << "chrome::NOTIFICATION_KIOSK_APPS_LOADED";
      content::WebUI* web_ui = static_cast<chromeos::LoginDisplayHostImpl*>(
          chromeos::LoginDisplayHostImpl::default_host())->
              GetOobeUI()->web_ui();
      web_ui->CallJavascriptFunction("login.AppsMenuButton.runAppForTesting",
                                     base::StringValue(kTestKioskApp));
    } else if (type == chrome::NOTIFICATION_KIOSK_APP_LAUNCHED) {
      LOG(INFO) << "chrome::NOTIFICATION_KIOSK_APP_LAUNCHED";
      registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                     content::NotificationService::AllSources());
      quit_task_.Run();
    } else if (type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED) {
      LOG(INFO) << "content::NOTIFICATION_RENDERER_PROCESS_CLOSED";
      quit_task_.Run();
    } else {
      NOTREACHED();
    }
  }

  content::NotificationRegistrar registrar_;
  base::Closure quit_task_;

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

}  // namespace

class KioskTest : public chromeos::CrosInProcessBrowserTest {
 public:
  KioskTest() : chromeos::CrosInProcessBrowserTest() {
    SetExitWhenLastBrowserCloses(false);
  }

 protected:

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(::switches::kDisableChromeCaptivePortalDetector);
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
    content_browser_client_.reset(new TestContentBrowserClient());
    original_content_browser_client_ = content::SetBrowserClientForTesting(
        content_browser_client_.get());
    host_resolver()->AddRule(kWebstoreDomain, "127.0.0.1");
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // Clean up while main thread still runs.
    // See http://crbug.com/176659.
    chromeos::KioskAppManager::Get()->CleanUp();
  }

  void ReloadKioskApps() {
    chromeos::KioskAppManager::Get()->AddApp(kTestKioskApp);
  }

  scoped_ptr<TestContentBrowserClient> content_browser_client_;
  content::ContentBrowserClient* original_content_browser_client_;
  std::string service_login_response_;
};

IN_PROC_BROWSER_TEST_F(KioskTest, InstallAndLaunchApp) {
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting();

  ReloadKioskApps();

  // The first loop exits after we receive NOTIFICATION_KIOSK_APP_LAUNCHED
  // notification - right at app launch.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  content_browser_client_->browser_main_extra_parts_->set_quit_task(
      runner->QuitClosure());
  runner->Run();

  // Check installer status.
  EXPECT_EQ(chromeos::KioskAppLaunchError::NONE,
            chromeos::KioskAppLaunchError::Get());

  // Check if the kiosk webapp is really installed for the default profile.
  ASSERT_TRUE(ProfileManager::GetDefaultProfile());
  const extensions::Extension* app =
      extensions::ExtensionSystem::Get(ProfileManager::GetDefaultProfile())->
      extension_service()->GetInstalledExtension(kTestKioskApp);
  EXPECT_TRUE(app);

  // The second loop exits when kiosk app terminates and we receive
  // NOTIFICATION_RENDERER_PROCESS_CLOSED.
  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  content_browser_client_->browser_main_extra_parts_->set_quit_task(
      runner2->QuitClosure());
  runner2->Run();
}

}  // namespace chromeos
