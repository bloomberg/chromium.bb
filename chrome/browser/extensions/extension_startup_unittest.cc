// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

// This file contains high-level startup tests for the extensions system. We've
// had many silly bugs where command line flags did not get propagated correctly
// into the services, so we didn't start correctly.

class ExtensionStartupTestBase
  : public InProcessBrowserTest, public NotificationObserver {
 public:
  ExtensionStartupTestBase()
      : enable_extensions_(false), enable_user_scripts_(false) {
  }

 protected:
  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    EnableDOMAutomation();

    FilePath profile_dir;
    PathService::Get(chrome::DIR_USER_DATA, &profile_dir);
    profile_dir = profile_dir.AppendASCII("Default");
    file_util::CreateDirectory(profile_dir);

    preferences_file_ = profile_dir.AppendASCII("Preferences");
    user_scripts_dir_ = profile_dir.AppendASCII("User Scripts");
    extensions_dir_ = profile_dir.AppendASCII("Extensions");

    if (enable_extensions_) {
      FilePath src_dir;
      PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
      src_dir = src_dir.AppendASCII("extensions").AppendASCII("good");

      file_util::CopyFile(src_dir.AppendASCII("Preferences"),
                          preferences_file_);
      file_util::CopyDirectory(src_dir.AppendASCII("Extensions"),
                               profile_dir, true);  // recursive
    } else {
      command_line->AppendSwitch(switches::kDisableExtensions);
    }

    if (enable_user_scripts_) {
      command_line->AppendSwitch(switches::kEnableUserScripts);

      FilePath src_dir;
      PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
      src_dir = src_dir.AppendASCII("extensions").AppendASCII("good")
                       .AppendASCII("Extensions")
                       .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                       .AppendASCII("1.0.0.0");

      file_util::CreateDirectory(user_scripts_dir_);
      file_util::CopyFile(src_dir.AppendASCII("script2.js"),
                          user_scripts_dir_.AppendASCII("script2.user.js"));
    }

    if (!load_extension_.value().empty()) {
      command_line->AppendSwitchWithValue(switches::kLoadExtension,
                                          load_extension_.ToWStringHack());
    }
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSIONS_READY:
      case NotificationType::USER_SCRIPTS_UPDATED:
        MessageLoopForUI::current()->Quit();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void TearDown() {
    file_util::Delete(preferences_file_, false);
    file_util::Delete(user_scripts_dir_, true);
    file_util::Delete(extensions_dir_, true);
  }

  void WaitForServicesToStart(int num_expected_extensions,
                              bool expect_extensions_enabled) {
    ExtensionsService* service = browser()->profile()->GetExtensionsService();
    if (!service->is_ready()) {
      registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                     NotificationService::AllSources());
      ui_test_utils::RunMessageLoop();
      registrar_.Remove(this, NotificationType::EXTENSIONS_READY,
                        NotificationService::AllSources());
    }

    ASSERT_EQ(static_cast<uint32>(num_expected_extensions),
              service->extensions()->size());
    ASSERT_EQ(expect_extensions_enabled, service->extensions_enabled());

    UserScriptMaster* master = browser()->profile()->GetUserScriptMaster();
    if (!master->ScriptsReady()) {
      // Wait for UserScriptMaster to finish its scan.
      registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                     NotificationService::AllSources());
      ui_test_utils::RunMessageLoop();
      registrar_.Remove(this, NotificationType::USER_SCRIPTS_UPDATED,
                        NotificationService::AllSources());
    }
    ASSERT_TRUE(master->ScriptsReady());
  }

  void TestInjection(bool expect_css, bool expect_script) {
    // Load a page affected by the content script and test to see the effect.
    FilePath test_file;
    PathService::Get(chrome::DIR_TEST_DATA, &test_file);
    test_file = test_file.AppendASCII("extensions")
                         .AppendASCII("test_file.html");

    ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

    bool result = false;
    ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        L"window.domAutomationController.send("
        L"document.defaultView.getComputedStyle(document.body, null)."
        L"getPropertyValue('background-color') == 'rgb(245, 245, 220)')",
        &result);
    EXPECT_EQ(expect_css, result);

    result = false;
    ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        L"window.domAutomationController.send(document.title == 'Modified')",
        &result);
    EXPECT_EQ(expect_script, result);
  }

  FilePath preferences_file_;
  FilePath extensions_dir_;
  FilePath user_scripts_dir_;
  bool enable_extensions_;
  bool enable_user_scripts_;
  FilePath load_extension_;
  NotificationRegistrar registrar_;
};


// ExtensionsStartupTest
// Ensures that we can startup the browser with --enable-extensions and some
// extensions installed and see them run and do basic things.

class ExtensionsStartupTest : public ExtensionStartupTestBase {
 public:
  ExtensionsStartupTest() {
    enable_extensions_ = true;
  }
};

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionsStartupTest, Test) {
  WaitForServicesToStart(3, true);
  TestInjection(true, true);
}
#endif  // defined(OS_WIN)

// ExtensionsLoadTest
// Ensures that we can startup the browser with --load-extension and see them
// run.

class ExtensionsLoadTest : public ExtensionStartupTestBase {
 public:
  ExtensionsLoadTest() {
    PathService::Get(chrome::DIR_TEST_DATA, &load_extension_);
    load_extension_ = load_extension_
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsLoadTest, Test) {
  WaitForServicesToStart(1, false);
  TestInjection(true, true);
}


// ExtensionsStartupUserScriptTest
// Tests that we can startup with --enable-user-scripts and run user scripts and
// see them do basic things.

class ExtensionsStartupUserScriptTest : public ExtensionStartupTestBase {
 public:
  ExtensionsStartupUserScriptTest() {
    enable_user_scripts_ = true;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsStartupUserScriptTest, Test) {
  WaitForServicesToStart(0, false);
  TestInjection(false, true);
}

// Ensure we don't inject into chrome:// URLs
IN_PROC_BROWSER_TEST_F(ExtensionsStartupUserScriptTest, NoInjectIntoChrome) {
  WaitForServicesToStart(0, false);

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  bool result = false;
  ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title == 'Modified')",
      &result);
  EXPECT_FALSE(result);
}
