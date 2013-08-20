// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
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

// Helper method for GetConsumerKioskModeStatusCallback.
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

class TestBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts,
      public content::NotificationObserver {
 public:
  TestBrowserMainExtraParts() {}
  virtual ~TestBrowserMainExtraParts() {}

  void set_quit_task(const base::Closure& quit_task) { quit_task_ = quit_task; }

  void SetupSigninScreen() {
    chromeos::ExistingUserController* controller =
        chromeos::ExistingUserController::current_controller();
    CHECK(controller);
    chromeos::WebUILoginDisplay* webui_login_display =
        static_cast<chromeos::WebUILoginDisplay*>(
            controller->login_display());
    CHECK(webui_login_display);
    webui_login_display->ShowSigninScreenForCreds("username", "password");
  }

 protected:
  content::NotificationRegistrar registrar_;
  base::Closure quit_task_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserMainExtraParts);
};

// Used to add an observer to NotificationService after it's created.
class KioskAppLaunchScenarioHandler : public TestBrowserMainExtraParts {
 public:
  KioskAppLaunchScenarioHandler() {}

  virtual ~KioskAppLaunchScenarioHandler() {}

 private:
  // ChromeBrowserMainExtraParts implementation.
  virtual void PreEarlyInitialization() OVERRIDE {
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_APPS_LOADED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
                   content::NotificationService::AllSources());
  }

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_KIOSK_APPS_LOADED) {
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

  DISALLOW_COPY_AND_ASSIGN(KioskAppLaunchScenarioHandler);
};

class AutostartWarningCancelScenarioHandler : public TestBrowserMainExtraParts {
 public:
  AutostartWarningCancelScenarioHandler() {
  }

  virtual ~AutostartWarningCancelScenarioHandler() {}

 private:
  // ChromeBrowserMainExtraParts implementation.
  virtual void PreEarlyInitialization() OVERRIDE {
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
                   content::NotificationService::AllSources());
  }

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE: {
        LOG(INFO) << "chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE";
        content::WebUI* web_ui = static_cast<chromeos::LoginDisplayHostImpl*>(
            chromeos::LoginDisplayHostImpl::default_host())->
                GetOobeUI()->web_ui();
        web_ui->CallJavascriptFunction(
            "login.AutolaunchScreen.confirmAutoLaunchForTesting",
            base::FundamentalValue(false));
        break;
      }
      case chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED: {
        LOG(INFO) << "chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED";
        quit_task_.Run();
        break;
      }
      case chrome::NOTIFICATION_KIOSK_APP_LAUNCHED: {
        LOG(FATAL) << "chrome::NOTIFICATION_KIOSK_APP_LAUNCHED";
        break;
      }
      default: {
        NOTREACHED();
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(AutostartWarningCancelScenarioHandler);
};

class AutostartWarningConfirmScenarioHandler
    : public TestBrowserMainExtraParts {
 public:
  AutostartWarningConfirmScenarioHandler() : first_pass_(true) {
  }

  virtual ~AutostartWarningConfirmScenarioHandler() {}

 private:
  // ChromeBrowserMainExtraParts implementation.
  virtual void PreEarlyInitialization() OVERRIDE {
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
                   content::NotificationService::AllSources());
  }

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE: {
        LOG(INFO) << "chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE";
        if (!first_pass_)
          break;

        content::WebUI* web_ui = static_cast<chromeos::LoginDisplayHostImpl*>(
            chromeos::LoginDisplayHostImpl::default_host())->
                GetOobeUI()->web_ui();
        web_ui->CallJavascriptFunction(
            "login.AutolaunchScreen.confirmAutoLaunchForTesting",
            base::FundamentalValue(true));
      }
      case chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED: {
        LOG(INFO) << "chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED";
        first_pass_ = false;
        break;
      }
      case chrome::NOTIFICATION_KIOSK_APP_LAUNCHED:
        LOG(INFO) << "chrome::NOTIFICATION_KIOSK_APP_LAUNCHED";
        registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                       content::NotificationService::AllSources());
        quit_task_.Run();
       break;
      case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
        LOG(INFO) << "chrome::NOTIFICATION_RENDERER_PROCESS_CLOSED";
        quit_task_.Run();
        break;
      }
      default: {
        NOTREACHED();
      }
    }
  }

  bool first_pass_;

  DISALLOW_COPY_AND_ASSIGN(AutostartWarningConfirmScenarioHandler);
};


class KioskEnableScenarioHandler
    : public TestBrowserMainExtraParts {
 public:
  explicit KioskEnableScenarioHandler(bool enable_kiosk)
      : enable_kiosk_(enable_kiosk) {
  }

  virtual ~KioskEnableScenarioHandler() {}

 private:
  // ChromeBrowserMainExtraParts implementation.
  virtual void PreEarlyInitialization() OVERRIDE {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_KIOSK_ENABLED,
                   content::NotificationService::AllSources());
  }

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
        LOG(INFO) << "NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE";
        SetupSigninScreen();
        content::WebUI* web_ui = static_cast<chromeos::LoginDisplayHostImpl*>(
            chromeos::LoginDisplayHostImpl::default_host())->
                GetOobeUI()->web_ui();
        web_ui->CallJavascriptFunction("cr.ui.Oobe.handleAccelerator",
                                       base::StringValue("kiosk_enable"));
        break;
      }
      case chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE: {
        LOG(INFO) << "chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE";
        content::WebUI* web_ui = static_cast<chromeos::LoginDisplayHostImpl*>(
            chromeos::LoginDisplayHostImpl::default_host())->
                GetOobeUI()->web_ui();
        web_ui->CallJavascriptFunction(
            "login.KioskEnableScreen.enableKioskForTesting",
            base::FundamentalValue(enable_kiosk_));
        break;
      }
      case chrome::NOTIFICATION_KIOSK_ENABLED: {
        LOG(INFO) << "chrome::NOTIFICATION_KIOSK_ENABLED";
        ASSERT_TRUE(enable_kiosk_);
        quit_task_.Run();
        break;
      }
      case chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED: {
        LOG(INFO) << "chrome::NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED";
        quit_task_.Run();
        break;
      }
      default: {
        NOTREACHED();
        break;
      }
    }
  }

  bool enable_kiosk_;
  DISALLOW_COPY_AND_ASSIGN(KioskEnableScenarioHandler);
};

class TestContentBrowserClient : public chrome::ChromeContentBrowserClient {
 public:
  enum LaunchEventSequence {
    KioskAppLaunch,
    AutostartWarningCanceled,
    AutostartWarningConfirmed,
    KioskEnableRejected,
    KioskEnableConfirmed,
  };

  explicit TestContentBrowserClient(LaunchEventSequence sequence)
      : browser_main_extra_parts_(NULL), sequence_(sequence) {
  }

  virtual ~TestContentBrowserClient() {}

  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE {
    ChromeBrowserMainParts* main_parts = static_cast<ChromeBrowserMainParts*>(
        ChromeContentBrowserClient::CreateBrowserMainParts(parameters));

    switch (sequence_) {
      case KioskAppLaunch:
        browser_main_extra_parts_ = new KioskAppLaunchScenarioHandler();
        break;
      case AutostartWarningCanceled:
        browser_main_extra_parts_ = new AutostartWarningCancelScenarioHandler();
        break;
      case AutostartWarningConfirmed:
        browser_main_extra_parts_ =
            new AutostartWarningConfirmScenarioHandler();
        break;
      case KioskEnableRejected:
        browser_main_extra_parts_ =
            new KioskEnableScenarioHandler(false);
        break;
      case KioskEnableConfirmed:
        browser_main_extra_parts_ =
            new KioskEnableScenarioHandler(true);
        break;
    }

    main_parts->AddParts(browser_main_extra_parts_);
    return main_parts;
  }

  TestBrowserMainExtraParts* browser_main_extra_parts_;

 private:
  LaunchEventSequence sequence_;

  DISALLOW_COPY_AND_ASSIGN(TestContentBrowserClient);
};

}  // namespace


class KioskTest : public InProcessBrowserTest,
                  // Param defining is multi-profiles enabled.
                  public testing::WithParamInterface<bool> {
 public:
  KioskTest()
     : original_content_browser_client_(NULL),
       test_server_(NULL) {
    set_exit_when_last_browser_closes(false);
  }

  virtual ~KioskTest() {}

 protected:
  virtual void InitializeKioskTest() = 0;

  virtual void SetUpOnMainThread() OVERRIDE {
    test_server_ = new EmbeddedTestServer(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::IO));
    CHECK(test_server_->InitializeAndWaitUntilReady());
    test_server_->RegisterRequestHandler(
        base::Bind(&KioskTest::HandleRequest, base::Unretained(this)));
    LOG(INFO) << "Set up http server at " << test_server_->base_url();

    const GURL gaia_url("http://localhost:" + test_server_->base_url().port());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kGaiaUrl, gaia_url.spec());
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // Clean up while main thread still runs.
    // See http://crbug.com/176659.
    KioskAppManager::Get()->CleanUp();

    LOG(INFO) << "Stopping the http server.";
    EXPECT_TRUE(test_server_->ShutdownAndWaitUntilComplete());
    delete test_server_;  // Destructor wants UI thread.
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
    InitializeKioskTest();
    original_content_browser_client_ = content::SetBrowserClientForTesting(
        content_browser_client_.get());

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

  scoped_ptr<TestContentBrowserClient> content_browser_client_;
  content::ContentBrowserClient* original_content_browser_client_;
  std::string service_login_response_;
  EmbeddedTestServer* test_server_;  // cant use scoped_ptr because destructor
                                     // needs UI thread.
 };

class KioskLaunchTest : public KioskTest {
 public:
  KioskLaunchTest() : KioskTest() {}
  virtual ~KioskLaunchTest() {}

  // KioskTest overrides.
  virtual void InitializeKioskTest() OVERRIDE {
    content_browser_client_.reset(
        new TestContentBrowserClient(TestContentBrowserClient::KioskAppLaunch));
  }
};

IN_PROC_BROWSER_TEST_P(KioskLaunchTest, InstallAndLaunchApp) {
  EnableConsumerKioskMode();
  chromeos::AppLaunchController::SkipSplashWaitForTesting();
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

class KioskAutolaunchCancelTest : public KioskTest {
 public:
  KioskAutolaunchCancelTest() : KioskTest(), login_display_host_(NULL) {}
  virtual ~KioskAutolaunchCancelTest() {}

 private:
  // Overrides from KioskTest.
  virtual void InitializeKioskTest() OVERRIDE {
    content_browser_client_.reset(new TestContentBrowserClient(
            TestContentBrowserClient::AutostartWarningCanceled));
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    login_display_host_ = LoginDisplayHostImpl::default_host();
    KioskTest::SetUpOnMainThread();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // LoginDisplayHost owns controllers and all windows.
    base::MessageLoopForUI::current()->DeleteSoon(FROM_HERE,
                                                  login_display_host_);
    KioskTest::CleanUpOnMainThread();
  }

  LoginDisplayHost* login_display_host_;
};

IN_PROC_BROWSER_TEST_P(KioskAutolaunchCancelTest, AutolaunchWarningCancel) {
  EnableConsumerKioskMode();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  ReloadAutolaunchKioskApps();
  wizard_controller->SkipToLoginForTesting();

  EXPECT_FALSE(
      KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(
      KioskAppManager::Get()->IsAutoLaunchEnabled());

  // The loop exits after we receive
  // NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED after
  // NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE notification - right after
  // auto launch is canceled.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  content_browser_client_->browser_main_extra_parts_->set_quit_task(
      runner->QuitClosure());
  runner->Run();

  EXPECT_FALSE(
      KioskAppManager::Get()->IsAutoLaunchEnabled());
}

class KioskAutolaunchConfirmTest : public KioskTest {
 public:
  KioskAutolaunchConfirmTest() : KioskTest() {}
  virtual ~KioskAutolaunchConfirmTest() {}

 private:
  // Overrides from KioskTest.
  virtual void InitializeKioskTest() OVERRIDE {
    content_browser_client_.reset(new TestContentBrowserClient(
            TestContentBrowserClient::AutostartWarningConfirmed));
  }
};

IN_PROC_BROWSER_TEST_P(KioskAutolaunchConfirmTest, AutolaunchWarningConfirm) {
  EnableConsumerKioskMode();
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);
  wizard_controller->SkipToLoginForTesting();

  ReloadAutolaunchKioskApps();
  EXPECT_FALSE(
      KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(
      KioskAppManager::Get()->IsAutoLaunchEnabled());

  // The loop exits after we receive NOTIFICATION_KIOSK_APP_LAUNCHED
  // notification - right at app launch.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  content_browser_client_->browser_main_extra_parts_->set_quit_task(
      runner->QuitClosure());
  runner->Run();

  EXPECT_FALSE(
      KioskAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_TRUE(
      KioskAppManager::Get()->IsAutoLaunchEnabled());

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

class KioskEnableTest : public KioskTest {
 public:
  KioskEnableTest() : login_display_host_(NULL) {}
  virtual ~KioskEnableTest() {}

 private:
  // Overrides from KioskTest.
  virtual void SetUpOnMainThread() OVERRIDE {
    login_display_host_ = LoginDisplayHostImpl::default_host();
    KioskTest::SetUpOnMainThread();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // LoginDisplayHost owns controllers and all windows.
    base::MessageLoopForUI::current()->DeleteSoon(FROM_HERE,
                                                  login_display_host_);
    KioskTest::CleanUpOnMainThread();
  }

  LoginDisplayHost* login_display_host_;
};

class KioskEnableCancelTest : public KioskEnableTest {
 public:
  KioskEnableCancelTest() {}
  virtual ~KioskEnableCancelTest() {}

 private:
  // Overrides from KioskTest.
  virtual void InitializeKioskTest() OVERRIDE {
    content_browser_client_.reset(new TestContentBrowserClient(
            TestContentBrowserClient::KioskEnableRejected));
  }
};

IN_PROC_BROWSER_TEST_P(KioskEnableCancelTest, KioskEnableCancel) {
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  // Check the intial state of the consumer kiosk.
  scoped_ptr<KioskAppManager::ConsumerKioskModeStatus> status(
      new KioskAppManager::ConsumerKioskModeStatus(
          KioskAppManager::CONSUMER_KIOSK_MODE_DISABLED));
  KioskAppManager::Get()->GetConsumerKioskModeStatus(
      base::Bind(&ConsumerKioskModeStatusCheck,
                 status.get(),
                 runner->QuitClosure()));
  runner->Run();
  EXPECT_EQ(*status.get(),  KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE);

  wizard_controller->SkipToLoginForTesting();
  // The loop exits after we receive
  // NOTIFICATION_KIOSK_ENABLE_WARNING_COMPLETED after
  // NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE notification - right after
  // enable consumer kiosk screen is canceled.
  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  content_browser_client_->browser_main_extra_parts_->set_quit_task(
      runner2->QuitClosure());
  runner2->Run();

  scoped_refptr<content::MessageLoopRunner> runner3 =
      new content::MessageLoopRunner;
  // Check the end state of the consumer kiosk after disabling the feature.
  KioskAppManager::Get()->GetConsumerKioskModeStatus(
      base::Bind(&ConsumerKioskModeStatusCheck,
                 status.get(),
                 runner3->QuitClosure()));
  runner3->Run();
  EXPECT_EQ(*status.get(),  KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE);
}


class KioskEnableConfirmedTest : public KioskEnableTest {
 public:
  KioskEnableConfirmedTest() {}
  virtual ~KioskEnableConfirmedTest() {}

 private:
  // Overrides from KioskTest.
  virtual void InitializeKioskTest() OVERRIDE {
    content_browser_client_.reset(new TestContentBrowserClient(
            TestContentBrowserClient::KioskEnableConfirmed));
  }
};

IN_PROC_BROWSER_TEST_P(KioskEnableConfirmedTest, KioskEnableConfirmed) {
  // Start UI, find menu entry for this app and launch it.
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* wizard_controller =
      chromeos::WizardController::default_controller();
  CHECK(wizard_controller);

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  // Check the intial state of the consumer kiosk.
  scoped_ptr<KioskAppManager::ConsumerKioskModeStatus> status(
      new KioskAppManager::ConsumerKioskModeStatus(
          KioskAppManager::CONSUMER_KIOSK_MODE_DISABLED));
  KioskAppManager::Get()->GetConsumerKioskModeStatus(
      base::Bind(&ConsumerKioskModeStatusCheck,
                 status.get(),
                 runner->QuitClosure()));
  runner->Run();
  EXPECT_EQ(*status.get(),  KioskAppManager::CONSUMER_KIOSK_MODE_CONFIGURABLE);

  wizard_controller->SkipToLoginForTesting();
  // The loop exits after we receive
  // NOTIFICATION_KIOSK_ENABLED after
  // NOTIFICATION_KIOSK_ENABLE_WARNING_VISIBLE notification - right after
  // enable consumer kiosk screen is canceled.
  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  content_browser_client_->browser_main_extra_parts_->set_quit_task(
      runner2->QuitClosure());
  runner2->Run();

  scoped_refptr<content::MessageLoopRunner> runner3 =
      new content::MessageLoopRunner;
  // Check the end state of the consumer kiosk after disabling the feature.
  KioskAppManager::Get()->GetConsumerKioskModeStatus(
      base::Bind(&ConsumerKioskModeStatusCheck,
                 status.get(),
                 runner3->QuitClosure()));
  runner3->Run();
  EXPECT_EQ(*status.get(),  KioskAppManager::CONSUMER_KIOSK_MODE_ENABLED);
}

INSTANTIATE_TEST_CASE_P(KioskLaunchTestInstantiation,
                        KioskLaunchTest,
                        testing::Bool());

INSTANTIATE_TEST_CASE_P(KioskAutolaunchCancelTestInstantiation,
                        KioskAutolaunchCancelTest,
                        testing::Bool());

INSTANTIATE_TEST_CASE_P(KioskAutolaunchConfirmTestInstantiation,
                        KioskAutolaunchConfirmTest,
                        testing::Bool());

INSTANTIATE_TEST_CASE_P(KioskEnableCancelTestInstantiation,
                        KioskEnableCancelTest,
                        testing::Bool());

INSTANTIATE_TEST_CASE_P(KioskEnableConfirmedTestInstantiation,
                        KioskEnableConfirmedTest,
                        testing::Bool());

}  // namespace chromeos
