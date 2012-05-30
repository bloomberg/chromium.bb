// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"

using content::WebContents;
using extensions::Extension;

namespace utils = extension_function_test_utils;

namespace {
// Non-abstract RenderViewContextMenu class.
class PlatformAppContextMenu : public RenderViewContextMenu {
 public:
  PlatformAppContextMenu(WebContents* web_contents,
                         const content::ContextMenuParams& params)
      : RenderViewContextMenu(web_contents, params) {}

 protected:
  // These two functions implement pure virtual methods of
  // RenderViewContextMenu.
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator) {
    return false;
  }
  virtual void PlatformInit() {}
  virtual void PlatformCancel() {}
};

}  // namespace

class PlatformAppBrowserTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePlatformApps);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

 protected:
  const Extension* LoadAndLaunchPlatformApp(const char* name) {
    ui_test_utils::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());

    const Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("platform_apps").AppendASCII(name));
    EXPECT_TRUE(extension);

    application_launch::OpenApplication(
        browser()->profile(),
        extension,
        extension_misc::LAUNCH_NONE,
        GURL(),
        NEW_WINDOW,
        NULL);

    app_loaded_observer.Wait();

    return extension;
  }

  // Gets the WebContents associated with the first shell window that is found
  // (most tests only deal with one platform app window, so this is good
  // enough).
  WebContents* GetFirstShellWindowWebContents() {
    ShellWindowRegistry* app_registry =
        ShellWindowRegistry::Get(browser()->profile());
    ShellWindowRegistry::const_iterator iter;
    ShellWindowRegistry::ShellWindowSet shell_windows =
        app_registry->shell_windows();
    for (iter = shell_windows.begin(); iter != shell_windows.end(); ++iter) {
      return (*iter)->web_contents();
    }

    return NULL;
  }

  // Runs chrome.windows.getAll for the given extension and returns the number
  // of windows that the function returns.
  size_t RunGetWindowsFunctionForExtension(const Extension* extension) {
    GetAllWindowsFunction* function = new GetAllWindowsFunction();
    function->set_extension(extension);
    scoped_ptr<base::ListValue> result(utils::ToList(
        utils::RunFunctionAndReturnResult(function, "[]", browser())));
    return result->GetSize();
  }

  // Runs chrome.windows.get(|window_id|) for the the given extension and
  // returns whether or not a window was found.
  bool RunGetWindowFunctionForExtension(
      int window_id, const Extension* extension) {
    GetWindowFunction* function = new GetWindowFunction();
    function->set_extension(extension);
    utils::RunFunction(
            function,
            base::StringPrintf("[%u]", window_id),
            browser(),
            utils::NONE);
    return function->GetResultValue() != NULL;
  }

  size_t GetShellWindowCount() {
    return ShellWindowRegistry::Get(browser()->profile())->
        shell_windows().size();
  }

  // The command line already has an argument on it - about:blank, which
  // is set by InProcessBrowserTest::PrepareTestCommandLine. For platform app
  // launch tests we need to clear this.
  void ClearCommandLineArgs() {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    CommandLine::StringVector args = command_line->GetArgs();
    CommandLine::StringVector argv = command_line->argv();
    for (size_t i = 0; i < args.size(); i++)
      argv.pop_back();
    command_line->InitFromArgv(argv);
  }

  void SetCommandLineArg(const std::string& test_file) {
    ClearCommandLineArgs();
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    FilePath test_doc(test_data_dir_.AppendASCII(test_file));
    test_doc = test_doc.NormalizePathSeparators();
    command_line->AppendArgPath(test_doc);
  }
};

// Tests that platform apps received the "launch" event when launched.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OnLaunchedEvent) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, EmptyContextMenu) {
  ExtensionTestMessageListener launched_listener("Launched", false);
  LoadAndLaunchPlatformApp("minimal");

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // The empty app doesn't add any context menu items, so its menu should
  // only include the developer tools.
  WebContents* web_contents = GetFirstShellWindowWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  content::ContextMenuParams params(data);
  PlatformAppContextMenu* menu = new PlatformAppContextMenu(web_contents,
      params);
  menu->Init();
  // TODO(benwells): Remove the constant below. Instead of checking the
  // number of menu items check certain item's absense and presence.
  // 3 including separator
  ASSERT_EQ(3, menu->menu_model().GetItemCount());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenu) {
  ExtensionTestMessageListener launched_listener("Launched", false);
  LoadAndLaunchPlatformApp("context_menu");

  // Wait for the extension to tell us it's initialized its context menus and
  // launched a window.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // The context_menu app has one context menu item. This, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstShellWindowWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  content::ContextMenuParams params(data);
  PlatformAppContextMenu* menu = new PlatformAppContextMenu(web_contents,
      params);
  menu->Init();
  // TODO(benwells): Remove the constant below. Instead of checking the
  // number of menu items check certain item's absense and presence.
  ASSERT_EQ(4, menu->menu_model().GetItemCount());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisallowNavigation) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/navigation")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Iframes) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/iframes")) << message_;
}

// Tests that localStorage and WebSQL are disabled for platform apps.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisallowStorage) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/storage")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Restrictions) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/restrictions")) << message_;
}

// Tests that platform apps can use the chrome.windows.* API.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApi) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/windows_api")) << message_;
}

// Tests that platform apps have isolated storage by default.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Isolation) {
  ASSERT_TRUE(StartTestServer());

  // Load a (non-app) page under the "localhost" origin that sets a cookie.
  GURL set_cookie_url = test_server()->GetURL(
      "files/extensions/platform_apps/isolation/set_cookie.html");
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  set_cookie_url = set_cookie_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), set_cookie_url,
      CURRENT_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Make sure the cookie is set.
  int cookie_size;
  std::string cookie_value;
  automation_util::GetCookies(
      set_cookie_url,
      browser()->GetWebContentsAt(0),
      &cookie_size,
      &cookie_value);
  ASSERT_EQ("testCookie=1", cookie_value);

  // Let the platform app request the same URL, and make sure that it doesn't
  // see the cookie.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/isolation")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ExtensionWindowingApis) {
  // Initially there should be just the one browser window visible to the
  // extensions API.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("common/background_page"));
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(extension));

  // And no shell windows.
  ASSERT_EQ(0U, GetShellWindowCount());

  // Launch a platform app that shows a window.
  ExtensionTestMessageListener launched_listener("Launched", false);
  const Extension* platform_app = LoadAndLaunchPlatformApp("minimal");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_EQ(1U, GetShellWindowCount());
  ShellWindowRegistry::ShellWindowSet shell_windows =
      ShellWindowRegistry::Get(browser()->profile())->shell_windows();
  int shell_window_id = (*shell_windows.begin())->session_id().id();

  // But it's not visible to the extensions API, it still thinks there's just
  // one browser window.
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(extension));
  // It can't look it up by ID either
  ASSERT_FALSE(RunGetWindowFunctionForExtension(shell_window_id, extension));

  // The app can also only see one window (its own).
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(platform_app));
  ASSERT_TRUE(RunGetWindowFunctionForExtension(shell_window_id, platform_app));

  // Launch another platform app that also shows a window.
  ExtensionTestMessageListener launched_listener2("Launched", false);
  const Extension* platform_app2 = LoadAndLaunchPlatformApp("context_menu");
  ASSERT_TRUE(launched_listener2.WaitUntilSatisfied());

  // There are two total shell windows, but each app can only see its own.
  ASSERT_EQ(2U, GetShellWindowCount());
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(platform_app));
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(platform_app2));
}

// TODO(benwells): fix these tests for ChromeOS.
#if !defined(OS_CHROMEOS)
// Tests that command line parameters get passed through to platform apps
// via launchData correctly when launching with a file.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithFile) {
  SetCommandLineArg( "platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_file"))
      << message_;
}

// Tests that no launch data is sent through if the platform app provides
// an intent with the wrong action.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithWrongIntent) {
  SetCommandLineArg("platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_wrong_intent"))
      << message_;
}

// Tests that no launch data is sent through if the file is of the wrong MIME
// type.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithWrongType) {
  SetCommandLineArg("platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_wrong_type"))
      << message_;
}

// Tests that no launch data is sent through if the platform app does not
// provide an intent.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithNoIntent) {
  SetCommandLineArg("platform_apps/launch_files/test.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_no_intent"))
      << message_;
}

// Tests that no launch data is sent through if the file MIME type cannot
// be read.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchNoType) {
  SetCommandLineArg("platform_apps/launch_files/test.unknownextension");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_invalid"))
      << message_;
}

// Tests that no launch data is sent through if the file does not exist.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchNoFile) {
  SetCommandLineArg("platform_apps/launch_files/doesnotexist.txt");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_invalid"))
      << message_;
}

// Tests that no launch data is sent through if the argument is a directory.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithDirectory) {
  SetCommandLineArg("platform_apps/launch_files");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_invalid"))
      << message_;
}

// Tests that no launch data is sent through if there are no arguments passed
// on the command line
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithNothing) {
  ClearCommandLineArgs();
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_nothing"))
      << message_;
}
#endif  // defined(OS_CHROMEOS)
