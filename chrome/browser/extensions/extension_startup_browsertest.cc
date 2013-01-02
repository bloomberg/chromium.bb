// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_util.h"

using extensions::FeatureSwitch;

// This file contains high-level startup tests for the extensions system. We've
// had many silly bugs where command line flags did not get propagated correctly
// into the services, so we didn't start correctly.

class ExtensionStartupTestBase : public InProcessBrowserTest {
 public:
  ExtensionStartupTestBase() :
      enable_extensions_(false),
      override_sideload_wipeout_(FeatureSwitch::sideload_wipeout(), false) {
    num_expected_extensions_ = 3;
  }

 protected:
  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    if (!enable_extensions_)
      command_line->AppendSwitch(switches::kDisableExtensions);

    if (!load_extensions_.empty()) {
      FilePath::StringType paths = JoinString(load_extensions_, ',');
      command_line->AppendSwitchNative(switches::kLoadExtension,
                                       paths);
      command_line->AppendSwitch(switches::kDisableExtensionsFileAccessCheck);
    }
  }

  virtual bool SetUpUserDataDirectory() {
    FilePath profile_dir;
    PathService::Get(chrome::DIR_USER_DATA, &profile_dir);
    profile_dir = profile_dir.AppendASCII(TestingProfile::kTestUserProfileDir);
    file_util::CreateDirectory(profile_dir);

    preferences_file_ = profile_dir.AppendASCII("Preferences");
    user_scripts_dir_ = profile_dir.AppendASCII("User Scripts");
    extensions_dir_ = profile_dir.AppendASCII("Extensions");

    if (enable_extensions_ && load_extensions_.empty()) {
      FilePath src_dir;
      PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
      src_dir = src_dir.AppendASCII("extensions").AppendASCII("good");

      file_util::CopyFile(src_dir.AppendASCII("Preferences"),
                          preferences_file_);
      file_util::CopyDirectory(src_dir.AppendASCII("Extensions"),
                               profile_dir, true);  // recursive
    }
    return true;
  }

  virtual void TearDown() {
    EXPECT_TRUE(file_util::Delete(preferences_file_, false));

    // TODO(phajdan.jr): Check return values of the functions below, carefully.
    file_util::Delete(user_scripts_dir_, true);
    file_util::Delete(extensions_dir_, true);

    InProcessBrowserTest::TearDown();
  }

  void WaitForServicesToStart(int num_expected_extensions,
                              bool expect_extensions_enabled) {
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();

    // Count the number of non-component extensions.
    int found_extensions = 0;
    for (ExtensionSet::const_iterator it = service->extensions()->begin();
         it != service->extensions()->end(); ++it)
      if ((*it)->location() != extensions::Extension::COMPONENT)
        found_extensions++;

    ASSERT_EQ(static_cast<uint32>(num_expected_extensions),
              static_cast<uint32>(found_extensions));
    ASSERT_EQ(expect_extensions_enabled, service->extensions_enabled());

    content::WindowedNotificationObserver user_scripts_observer(
        chrome::NOTIFICATION_USER_SCRIPTS_UPDATED,
        content::NotificationService::AllSources());
    extensions::UserScriptMaster* master =
        extensions::ExtensionSystem::Get(browser()->profile())->
            user_script_master();
    if (!master->ScriptsReady())
      user_scripts_observer.Wait();
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
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
        chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
        "",
        "window.domAutomationController.send("
        "document.defaultView.getComputedStyle(document.body, null)."
        "getPropertyValue('background-color') == 'rgb(245, 245, 220)')",
        &result));
    EXPECT_EQ(expect_css, result);

    result = false;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
        chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
        "",
        "window.domAutomationController.send(document.title == 'Modified')",
        &result));
    EXPECT_EQ(expect_script, result);
  }

  FilePath preferences_file_;
  FilePath extensions_dir_;
  FilePath user_scripts_dir_;
  bool enable_extensions_;
  // Extensions to load from the command line.
  std::vector<FilePath::StringType> load_extensions_;

  // Disable the sideload wipeout UI.
  FeatureSwitch::ScopedOverride override_sideload_wipeout_;

  int num_expected_extensions_;
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

IN_PROC_BROWSER_TEST_F(ExtensionsStartupTest, Test) {
  WaitForServicesToStart(num_expected_extensions_, true);
  TestInjection(true, true);
}

// Sometimes times out on Mac.  http://crbug.com/48151
#if defined(OS_MACOSX)
#define MAYBE_NoFileAccess DISABLED_NoFileAccess
#else
#define MAYBE_NoFileAccess NoFileAccess
#endif
// Tests that disallowing file access on an extension prevents it from injecting
// script into a page with a file URL.
IN_PROC_BROWSER_TEST_F(ExtensionsStartupTest, MAYBE_NoFileAccess) {
  WaitForServicesToStart(num_expected_extensions_, true);

  // Keep a separate list of extensions for which to disable file access, since
  // doing so reloads them.
  std::vector<const extensions::Extension*> extension_list;

  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  for (ExtensionSet::const_iterator it = service->extensions()->begin();
       it != service->extensions()->end(); ++it) {
    if ((*it)->location() == extensions::Extension::COMPONENT)
      continue;
    if (service->AllowFileAccess(*it))
      extension_list.push_back(*it);
  }

  for (size_t i = 0; i < extension_list.size(); ++i) {
    content::WindowedNotificationObserver user_scripts_observer(
        chrome::NOTIFICATION_USER_SCRIPTS_UPDATED,
        content::NotificationService::AllSources());
    service->SetAllowFileAccess(extension_list[i], false);
    user_scripts_observer.Wait();
  }

  TestInjection(false, false);
}

// ExtensionsLoadTest
// Ensures that we can startup the browser with --load-extension and see them
// run.

class ExtensionsLoadTest : public ExtensionStartupTestBase {
 public:
  ExtensionsLoadTest() {
    enable_extensions_ = true;
    FilePath one_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &one_extension_path);
    one_extension_path = one_extension_path
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
    load_extensions_.push_back(one_extension_path.value());
  }
};

// Fails inconsistently on Linux x64. http://crbug.com/80961
// TODO(dpapad): Has not failed since October 2011, let's reenable, monitor
// and act accordingly.
IN_PROC_BROWSER_TEST_F(ExtensionsLoadTest, Test) {
  WaitForServicesToStart(1, true);
  TestInjection(true, true);
}

// ExtensionsLoadMultipleTest
// Ensures that we can startup the browser with multiple extensions
// via --load-extension=X1,X2,X3.
class ExtensionsLoadMultipleTest : public ExtensionStartupTestBase {
 public:
  ExtensionsLoadMultipleTest() {
    enable_extensions_ = true;
    FilePath one_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &one_extension_path);
    one_extension_path = one_extension_path
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
    load_extensions_.push_back(one_extension_path.value());

    FilePath second_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &second_extension_path);
    second_extension_path = second_extension_path
        .AppendASCII("extensions")
        .AppendASCII("app");
    load_extensions_.push_back(second_extension_path.value());

    FilePath third_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &third_extension_path);
    third_extension_path = third_extension_path
        .AppendASCII("extensions")
        .AppendASCII("app1");
    load_extensions_.push_back(third_extension_path.value());

    FilePath fourth_extension_path;
    PathService::Get(chrome::DIR_TEST_DATA, &fourth_extension_path);
    fourth_extension_path = fourth_extension_path
        .AppendASCII("extensions")
        .AppendASCII("app2");
    load_extensions_.push_back(fourth_extension_path.value());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsLoadMultipleTest, Test) {
  WaitForServicesToStart(4, true);
  TestInjection(true, true);
}
