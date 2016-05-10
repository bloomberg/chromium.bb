// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_notification_tracker.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(DISABLE_NACL)
#include "components/nacl/browser/nacl_process_host.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

using base::ASCIIToUTF16;
using extensions::Extension;

class AppBackgroundPageApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
    command_line->AppendSwitch(extensions::switches::kAllowHTTPBackgroundPage);
  }

  bool CreateApp(const std::string& app_manifest,
                 base::FilePath* app_dir) {
    if (!app_dir_.CreateUniqueTempDir()) {
      LOG(ERROR) << "Unable to create a temporary directory.";
      return false;
    }
    base::FilePath manifest_path = app_dir_.path().AppendASCII("manifest.json");
    int bytes_written = base::WriteFile(manifest_path,
                                        app_manifest.data(),
                                        app_manifest.size());
    if (bytes_written != static_cast<int>(app_manifest.size())) {
      LOG(ERROR) << "Unable to write complete manifest to file. Return code="
                 << bytes_written;
      return false;
    }
    *app_dir = app_dir_.path();
    return true;
  }

  bool WaitForBackgroundMode(bool expected_background_mode) {
#if defined(OS_CHROMEOS)
    // BackgroundMode is not supported on chromeos, so we should test the
    // behavior of BackgroundContents, but not the background mode state itself.
    return true;
#else
    BackgroundModeManager* manager =
        g_browser_process->background_mode_manager();
    // If background mode is disabled on this platform (e.g. cros), then skip
    // this check.
    if (!manager || !manager->IsBackgroundModePrefEnabled()) {
      DLOG(WARNING) << "Skipping check - background mode disabled";
      return true;
    }
    if (manager->IsBackgroundModeActive() == expected_background_mode)
      return true;

    // We are not currently in the expected state - wait for the state to
    // change.
    content::WindowedNotificationObserver watcher(
        chrome::NOTIFICATION_BACKGROUND_MODE_CHANGED,
        content::NotificationService::AllSources());
    watcher.Wait();
    return manager->IsBackgroundModeActive() == expected_background_mode;
#endif
  }

  void UnloadExtensionViaTask(const std::string& id) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AppBackgroundPageApiTest::UnloadExtension, this, id));
  }

 private:
  base::ScopedTempDir app_dir_;
};

namespace {

// Fixture to assist in testing v2 app background pages containing
// Native Client embeds.
class AppBackgroundPageNaClTest : public AppBackgroundPageApiTest {
 public:
  AppBackgroundPageNaClTest()
      : extension_(NULL) {}
  ~AppBackgroundPageNaClTest() override {}

  void SetUpOnMainThread() override {
    AppBackgroundPageApiTest::SetUpOnMainThread();
#if !defined(DISABLE_NACL)
    nacl::NaClProcessHost::SetPpapiKeepAliveThrottleForTesting(50);
#endif
    extensions::ProcessManager::SetEventPageIdleTimeForTesting(1000);
    extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(1000);
  }

  const Extension* extension() { return extension_; }

 protected:
  void LaunchTestingApp() {
    base::FilePath app_dir;
    PathService::Get(chrome::DIR_GEN_TEST_DATA, &app_dir);
    app_dir = app_dir.AppendASCII(
        "ppapi/tests/extensions/background_keepalive/newlib");
    extension_ = LoadExtension(app_dir);
    ASSERT_TRUE(extension_);
  }

 private:
  const Extension* extension_;
};

// Produces an extensions::ProcessManager::ImpulseCallbackForTesting callback
// that will match a specified goal and can be waited on.
class ImpulseCallbackCounter {
 public:
  explicit ImpulseCallbackCounter(extensions::ProcessManager* manager,
                                  const std::string& extension_id)
      : observed_(0),
        goal_(0),
        manager_(manager),
        extension_id_(extension_id) {
  }

  extensions::ProcessManager::ImpulseCallbackForTesting
      SetGoalAndGetCallback(int goal) {
    observed_ = 0;
    goal_ = goal;
    message_loop_runner_ = new content::MessageLoopRunner();
    return base::Bind(&ImpulseCallbackCounter::ImpulseCallback,
                      base::Unretained(this),
                      message_loop_runner_->QuitClosure(),
                      extension_id_);
  }

  void Wait() {
    message_loop_runner_->Run();
  }
 private:
  void ImpulseCallback(
      const base::Closure& quit_callback,
      const std::string& extension_id_from_test,
      const std::string& extension_id_from_manager) {
    if (extension_id_from_test == extension_id_from_manager) {
      if (++observed_ >= goal_) {
        // Clear callback to free reference to message loop.
        manager_->SetKeepaliveImpulseCallbackForTesting(
            extensions::ProcessManager::ImpulseCallbackForTesting());
        manager_->SetKeepaliveImpulseDecrementCallbackForTesting(
            extensions::ProcessManager::ImpulseCallbackForTesting());
        quit_callback.Run();
      }
    }
  }

  int observed_;
  int goal_;
  extensions::ProcessManager* manager_;
  const std::string extension_id_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

}  // namespace

// Disable on Mac only.  http://crbug.com/95139
#if defined(OS_MACOSX)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, MAYBE_Basic) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  // Background mode should not be active until a background page is created.
  ASSERT_TRUE(WaitForBackgroundMode(false));
  ASSERT_TRUE(RunExtensionTest("app_background_page/basic")) << message_;
  // The test closes the background contents, so we should fall back to no
  // background mode at the end.
  ASSERT_TRUE(WaitForBackgroundMode(false));
}

// Crashy, http://crbug.com/69215.
IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, DISABLED_LacksPermission) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  }"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  ASSERT_TRUE(RunExtensionTest("app_background_page/lacks_permission"))
      << message_;
  ASSERT_TRUE(WaitForBackgroundMode(false));
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, ManifestBackgroundPage) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background\": {"
      "    \"page\": \"http://a.com:%u/test.html\""
      "  }"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  // Background mode should not be active now because no background app was
  // loaded.
  ASSERT_TRUE(LoadExtension(app_dir));
  // Background mode be active now because a background page was created when
  // the app was loaded.
  ASSERT_TRUE(WaitForBackgroundMode(true));

  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));
  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, NoJsBackgroundPage) {
  // Keep the task manager up through this test to verify that a crash doesn't
  // happen when window.open creates a background page that switches
  // RenderViewHosts. See http://crbug.com/165138.
  chrome::ShowTaskManager(browser());

  // Make sure that no BackgroundContentses get deleted (a signal that repeated
  // window.open calls recreate instances, instead of being no-ops).
  content::TestNotificationTracker background_deleted_tracker;
  background_deleted_tracker.ListenFor(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
      content::Source<Profile>(browser()->profile()));

  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/test.html\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background\": {"
      "    \"allow_js_access\": false"
      "  }"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));

  // There isn't a background page loaded initially.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_FALSE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));
  // The test makes sure that window.open returns null.
  ASSERT_TRUE(RunExtensionTest("app_background_page/no_js")) << message_;
  // And after it runs there should be a background page.
  ASSERT_TRUE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));

  EXPECT_EQ(0u, background_deleted_tracker.size());
  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, NoJsManifestBackgroundPage) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background\": {"
      "    \"page\": \"http://a.com:%u/bg.html\","
      "    \"allow_js_access\": false"
      "  }"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));

  // The background page should load, but window.open should return null.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));
  ASSERT_TRUE(RunExtensionTest("app_background_page/no_js_manifest")) <<
      message_;
  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, OpenTwoBackgroundPages) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(RunExtensionTest("app_background_page/two_pages")) << message_;
  UnloadExtension(extension->id());
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, OpenTwoPagesWithManifest) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"background\": {"
      "    \"page\": \"http://a.com:%u/bg.html\""
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(RunExtensionTest("app_background_page/two_with_manifest")) <<
      message_;
  UnloadExtension(extension->id());
}

// Times out occasionally -- see crbug.com/108493
IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, DISABLED_OpenPopupFromBGPage) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"background\": { \"page\": \"http://a.com:%u/extensions/api_test/"
      "app_background_page/bg_open/bg_open_bg.html\" },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  ASSERT_TRUE(RunExtensionTest("app_background_page/bg_open")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, DISABLED_OpenThenClose) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  // There isn't a background page loaded initially.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_FALSE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));
  // Background mode should not be active until a background page is created.
  ASSERT_TRUE(WaitForBackgroundMode(false));
  ASSERT_TRUE(RunExtensionTest("app_background_page/basic_open")) << message_;
  // Background mode should be active now because a background page was created.
  ASSERT_TRUE(WaitForBackgroundMode(true));
  ASSERT_TRUE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));
  // Now close the BackgroundContents.
  ASSERT_TRUE(RunExtensionTest("app_background_page/basic_close")) << message_;
  // Background mode should no longer be active.
  ASSERT_TRUE(WaitForBackgroundMode(false));
  ASSERT_FALSE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, UnloadExtensionWhileHidden) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());

  std::string app_manifest = base::StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"manifest_version\": 2,"
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%u/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background\": {"
      "    \"page\": \"http://a.com:%u/test.html\""
      "  }"
      "}",
      embedded_test_server()->port(),
      embedded_test_server()->port());

  base::FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  // Background mode should not be active now because no background app was
  // loaded.
  ASSERT_TRUE(LoadExtension(app_dir));
  // Background mode be active now because a background page was created when
  // the app was loaded.
  ASSERT_TRUE(WaitForBackgroundMode(true));

  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));

  // Close all browsers - app should continue running.
  set_exit_when_last_browser_closes(false);
  CloseBrowserSynchronously(browser());

  // Post a task to unload the extension - this should cause Chrome to exit
  // cleanly (not crash).
  UnloadExtensionViaTask(extension->id());
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(WaitForBackgroundMode(false));
}

// Verify active NaCl embeds cause many keepalive impulses to be sent.
// Disabled on Windows due to flakiness: http://crbug.com/346278
#if defined(OS_WIN)
#define MAYBE_BackgroundKeepaliveActive DISABLED_BackgroundKeepaliveActive
#else
// Disabling other platforms too since the test started failing
// consistently. http://crbug.com/490440
#define MAYBE_BackgroundKeepaliveActive DISABLED_BackgroundKeepaliveActive
#endif
IN_PROC_BROWSER_TEST_F(AppBackgroundPageNaClTest,
                       MAYBE_BackgroundKeepaliveActive) {
#if !defined(DISABLE_NACL)
  ExtensionTestMessageListener nacl_modules_loaded("nacl_modules_loaded", true);
  LaunchTestingApp();
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  ImpulseCallbackCounter active_impulse_counter(manager, extension()->id());
  EXPECT_TRUE(nacl_modules_loaded.WaitUntilSatisfied());

  // Target .5 seconds: .5 seconds / 50ms throttle * 2 embeds == 20 impulses.
  manager->SetKeepaliveImpulseCallbackForTesting(
      active_impulse_counter.SetGoalAndGetCallback(20));
  active_impulse_counter.Wait();
#endif
}

// Verify that nacl modules that go idle will not send keepalive impulses.
// Disabled on windows due to Win XP failures:
// DesktopWindowTreeHostWin::HandleCreate not implemented. crbug.com/331954
#if defined(OS_WIN)
#define MAYBE_BackgroundKeepaliveIdle DISABLED_BackgroundKeepaliveIdle
#else
// ASAN errors appearing: https://crbug.com/332440
#define MAYBE_BackgroundKeepaliveIdle DISABLED_BackgroundKeepaliveIdle
#endif
IN_PROC_BROWSER_TEST_F(AppBackgroundPageNaClTest,
                       MAYBE_BackgroundKeepaliveIdle) {
#if !defined(DISABLE_NACL)
  ExtensionTestMessageListener nacl_modules_loaded("nacl_modules_loaded", true);
  LaunchTestingApp();
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  ImpulseCallbackCounter idle_impulse_counter(manager, extension()->id());
  EXPECT_TRUE(nacl_modules_loaded.WaitUntilSatisfied());

  manager->SetKeepaliveImpulseDecrementCallbackForTesting(
      idle_impulse_counter.SetGoalAndGetCallback(1));
  nacl_modules_loaded.Reply("be idle");
  idle_impulse_counter.Wait();
#endif
}
