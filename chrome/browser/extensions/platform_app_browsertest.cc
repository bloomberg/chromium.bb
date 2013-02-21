// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/prefs/pref_service.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_utils.h"
#include "googleurl/src/gurl.h"

using content::WebContents;

namespace extensions {

namespace {

// Non-abstract RenderViewContextMenu class.
class PlatformAppContextMenu : public RenderViewContextMenu {
 public:
  PlatformAppContextMenu(WebContents* web_contents,
                         const content::ContextMenuParams& params)
      : RenderViewContextMenu(web_contents, params) {}

  bool HasCommandWithId(int command_id) {
    return menu_model_.GetIndexOfCommandId(command_id) != -1;
  }

 protected:
  // RenderViewContextMenu implementation.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }
  virtual void PlatformInit() OVERRIDE {}
  virtual void PlatformCancel() OVERRIDE {}
};

// This class keeps track of tabs as they are added to the browser. It will be
// "done" (i.e. won't block on Wait()) once |observations| tabs have been added.
class TabsAddedNotificationObserver
    : public content::WindowedNotificationObserver {
 public:
  explicit TabsAddedNotificationObserver(size_t observations)
      : content::WindowedNotificationObserver(
            chrome::NOTIFICATION_TAB_ADDED,
            content::NotificationService::AllSources()),
        observations_(observations) {
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    observed_tabs_.push_back(
        content::Details<WebContents>(details).ptr());
    if (observed_tabs_.size() == observations_)
      content::WindowedNotificationObserver::Observe(type, source, details);
  }

  const std::vector<content::WebContents*>& tabs() { return observed_tabs_; }

 private:
  size_t observations_;
  std::vector<content::WebContents*> observed_tabs_;

  DISALLOW_COPY_AND_ASSIGN(TabsAddedNotificationObserver);
};

const char kTestFilePath[] = "platform_apps/launch_files/test.txt";

}  // namespace

// Tests that CreateShellWindow doesn't crash if you close it straight away.
// LauncherPlatformAppBrowserTest relies on this behaviour, but is only run for
// ash, so we test that it works here.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, CreateAndCloseShellWindow) {
  const Extension* extension = LoadAndLaunchPlatformApp("minimal");
  ShellWindow* window = CreateShellWindow(extension);
  CloseShellWindow(window);
}

// Tests that platform apps received the "launch" event when launched.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OnLaunchedEvent) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch")) << message_;
}

// Tests that platform apps cannot use certain disabled window properties, but
// can override them and then use them.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisabledWindowProperties) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/disabled_window_properties"))
      << message_;
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
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents, params));
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenu) {
  ExtensionTestMessageListener launched_listener("Launched", false);
  LoadAndLaunchPlatformApp("context_menu");

  // Wait for the extension to tell us it's initialized its context menus and
  // launched a window.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // The context_menu app has two context menu items. These, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstShellWindowWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  content::ContextMenuParams params(data);
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents, params));
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, InstalledAppWithContextMenu) {
  ExtensionTestMessageListener launched_listener("Launched", false);
  InstallAndLaunchPlatformApp("context_menu");

  // Wait for the extension to tell us it's initialized its context menus and
  // launched a window.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // The context_menu app has two context menu items. For an installed app
  // these are all that should be in the menu.
  WebContents* web_contents = GetFirstShellWindowWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  content::ContextMenuParams params(data);
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents, params));
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST + 1));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_FALSE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenuTextField) {
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
  params.is_editable = true;
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents, params));
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenuSelection) {
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
  params.selection_text = ASCIIToUTF16("Hello World");
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents, params));
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_COPY));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenuClicked) {
  ExtensionTestMessageListener launched_listener("Launched", false);
  LoadAndLaunchPlatformApp("context_menu_click");

  // Wait for the extension to tell us it's initialized its context menus and
  // launched a window.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  // Test that the menu item shows up
  WebContents* web_contents = GetFirstShellWindowWebContents();
  ASSERT_TRUE(web_contents);
  WebKit::WebContextMenuData data;
  content::ContextMenuParams params(data);
  params.page_url = GURL("http://foo.bar");
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents, params));
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST));

  // Execute the menu item
  ExtensionTestMessageListener onclicked_listener("onClicked fired for id1",
                                                  false);
  menu->ExecuteCommand(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST);

  ASSERT_TRUE(onclicked_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisallowNavigation) {
  TabsAddedNotificationObserver observer(2);

  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/navigation")) << message_;

  observer.Wait();
  ASSERT_EQ(2U, observer.tabs().size());
  EXPECT_EQ(std::string(chrome::kExtensionInvalidRequestURL),
            observer.tabs()[0]->GetURL().spec());
  EXPECT_EQ("http://chromium.org/",
            observer.tabs()[1]->GetURL().spec());
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

// Tests that platform apps can use the chrome.app.window.* API.
// Flaky, http://crbug.com/167097 .
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DISABLED_WindowsApi) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/windows_api")) << message_;
}

// Tests that extensions can't use platform-app-only APIs.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, PlatformAppsOnly) {
  ASSERT_TRUE(RunExtensionTestIgnoreManifestWarnings(
      "platform_apps/apps_only")) << message_;
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
      browser()->tab_strip_model()->GetWebContentsAt(0),
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
  LoadAndLaunchPlatformApp("minimal");
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
  // TODO(jeremya): add an extension function to get a shell window by ID, and
  // to get a list of all the shell windows, so we can test this.

  // Launch another platform app that also shows a window.
  ExtensionTestMessageListener launched_listener2("Launched", false);
  LoadAndLaunchPlatformApp("context_menu");
  ASSERT_TRUE(launched_listener2.WaitUntilSatisfied());

  // There are two total shell windows, but each app can only see its own.
  ASSERT_EQ(2U, GetShellWindowCount());
  // TODO(jeremya): as above, this requires more extension functions.
}

// ChromeOS does not support passing arguments on the command line, so the tests
// that rely on this functionality are disabled.
#if !defined(OS_CHROMEOS)
// Tests that command line parameters get passed through to platform apps
// via launchData correctly when launching with a file.
// TODO(benwells/jeremya): tests need a way to specify a handler ID.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithFile) {
  SetCommandLineArg(kTestFilePath);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_file"))
      << message_;
}

// Tests that relative paths can be passed through to the platform app.
// This test doesn't use the normal test infrastructure as it needs to open
// the application differently to all other platform app tests, by setting
// the chrome::AppLaunchParams.current_directory field.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithRelativeFile) {
  // Setup the command line
  ClearCommandLineArgs();
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  base::FilePath relative_test_doc =
      base::FilePath::FromUTF8Unsafe(kTestFilePath);
  relative_test_doc = relative_test_doc.NormalizePathSeparators();
  command_line->AppendArgPath(relative_test_doc);

  // Load the extension
  ResultCatcher catcher;
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps/launch_file"));
  ASSERT_TRUE(extension);

  // Run the test
  chrome::AppLaunchParams params(browser()->profile(), extension,
                                 extension_misc::LAUNCH_NONE, NEW_WINDOW);
  params.command_line = CommandLine::ForCurrentProcess();
  params.current_directory = test_data_dir_;
  chrome::OpenApplication(params);

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    ASSERT_TRUE(0);
  }
}

// Tests that no launch data is sent through if the file is of the wrong MIME
// type.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithWrongType) {
  SetCommandLineArg(kTestFilePath);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_wrong_type"))
      << message_;
}

// Tests that no launch data is sent through if the platform app does not
// provide an intent.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithNoIntent) {
  SetCommandLineArg(kTestFilePath);
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

// Test that platform apps can use the chrome.fileSystem.getDisplayPath
// function to get the native file system path of a file they are launched with.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, GetDisplayPath) {
  SetCommandLineArg(kTestFilePath);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/get_display_path"))
      << message_;
}

#endif  // defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OpenLink) {
  ASSERT_TRUE(StartTestServer());
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::Source<content::WebContentsDelegate>(browser()));
  LoadAndLaunchPlatformApp("open_link");
  observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MutationEventsDisabled) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/mutation_events")) << message_;
}

// Test that windows created with an id will remember and restore their
// geometry when opening new windows.
// Originally disabled due to flakiness (see http://crbug.com/155459)
// but now because a regression breaks the test (http://crbug.com/160343).
#if defined(TOOLKIT_GTK)
#define MAYBE_ShellWindowRestorePosition DISABLED_ShellWindowRestorePosition
#else
#define MAYBE_ShellWindowRestorePosition ShellWindowRestorePosition
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_ShellWindowRestorePosition) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/geometry"));
}

namespace {

class PlatformAppDevToolsBrowserTest : public PlatformAppBrowserTest {
 protected:
  enum TestFlags {
    RELAUNCH = 0x1,
    HAS_ID = 0x2,
  };
  // Runs a test inside a harness that opens DevTools on a shell window.
  void RunTestWithDevTools(const char* name, int test_flags);
};

void PlatformAppDevToolsBrowserTest::RunTestWithDevTools(
    const char* name, int test_flags) {
  using content::DevToolsAgentHost;
  ExtensionTestMessageListener launched_listener("Launched", false);
  const Extension* extension = LoadAndLaunchPlatformApp(name);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ShellWindow* window = GetFirstShellWindow();
  ASSERT_TRUE(window);
  ASSERT_EQ(window->window_key().empty(), (test_flags & HAS_ID) == 0);
  content::RenderViewHost* rvh = window->web_contents()->GetRenderViewHost();
  ASSERT_TRUE(rvh);

  // Ensure no DevTools open for the ShellWindow, then open one.
  ASSERT_FALSE(DevToolsAgentHost::HasFor(rvh));
  DevToolsWindow* devtools_window = DevToolsWindow::OpenDevToolsWindow(rvh);
  content::WindowedNotificationObserver loaded_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &devtools_window->web_contents()->GetController()));
  loaded_observer.Wait();
  ASSERT_TRUE(DevToolsAgentHost::HasFor(rvh));

  if (test_flags & RELAUNCH) {
    // Close the ShellWindow, and ensure it is gone.
    CloseShellWindow(window);
    ASSERT_FALSE(GetFirstShellWindow());

    // Relaunch the app and get a new ShellWindow.
    content::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    chrome::OpenApplication(chrome::AppLaunchParams(browser()->profile(),
                                                    extension,
                                                    extension_misc::LAUNCH_NONE,
                                                    NEW_WINDOW));
    app_loaded_observer.Wait();
    window = GetFirstShellWindow();
    ASSERT_TRUE(window);

    // DevTools should have reopened with the relaunch.
    rvh = window->web_contents()->GetRenderViewHost();
    ASSERT_TRUE(rvh);
    ASSERT_TRUE(DevToolsAgentHost::HasFor(rvh));
  }
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PlatformAppDevToolsBrowserTest, ReOpenedWithID) {
  RunTestWithDevTools("minimal_id", RELAUNCH | HAS_ID);
}

IN_PROC_BROWSER_TEST_F(PlatformAppDevToolsBrowserTest, ReOpenedWithURL) {
  RunTestWithDevTools("minimal", RELAUNCH);
}

// Test that showing a permission request as a constrained window works and is
// correctly parented.
#if defined(OS_MACOSX)
#define MAYBE_ConstrainedWindowRequest DISABLED_ConstrainedWindowRequest
#else
// TODO(sail): Enable this on other platforms once http://crbug.com/95455 is
// fixed.
#define MAYBE_ConstrainedWindowRequest DISABLED_ConstrainedWindowRequest
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_ConstrainedWindowRequest) {
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  const Extension* extension =
      LoadAndLaunchPlatformApp("optional_permission_request");
  ASSERT_TRUE(extension) << "Failed to load extension.";

  WebContents* web_contents = GetFirstShellWindowWebContents();
  ASSERT_TRUE(web_contents);

  // Verify that the shell window has a dialog attached.
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsShowingDialog());

  // Close the constrained window and wait for the reply to the permission
  // request.
  ExtensionTestMessageListener listener("PermissionRequestDone", false);
  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);
  test_api.CloseAllDialogs();
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

// Tests that an app calling chrome.runtime.reload will reload the app and
// relaunch it if it was running.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ReloadRelaunches) {
  ExtensionTestMessageListener launched_listener("Launched", true);
  const Extension* extension = LoadAndLaunchPlatformApp("reload");
  ASSERT_TRUE(extension);
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_TRUE(GetFirstShellWindow());

  // Now tell the app to reload itself
  ExtensionTestMessageListener launched_listener2("Launched", false);
  launched_listener.Reply("reload");
  ASSERT_TRUE(launched_listener2.WaitUntilSatisfied());
  ASSERT_TRUE(GetFirstShellWindow());
}

namespace {

// Simple observer to check for NOTIFICATION_EXTENSION_INSTALLED events to
// ensure installation does or does not occur in certain scenarios.
class CheckExtensionInstalledObserver : public content::NotificationObserver {
 public:
  CheckExtensionInstalledObserver() : seen_(false) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_EXTENSION_INSTALLED,
                   content::NotificationService::AllSources());
  }

  bool seen() const {
    return seen_;
  };

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    EXPECT_FALSE(seen_);
    seen_ = true;
  }

 private:
  bool seen_;
  content::NotificationRegistrar registrar_;
};

}  // namespace

// Component App Test 1 of 3: ensure that the initial load of a component
// extension utilizing a background page (e.g. a v2 platform app) has its
// background page run and is launchable. Waits for the Launched response from
// the script resource in the opened shell window.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       PRE_PRE_ComponentAppBackgroundPage) {
  CheckExtensionInstalledObserver should_install;

  // Ensure that we wait until the background page is run (to register the
  // OnLaunched listener) before trying to open the application. This is similar
  // to LoadAndLaunchPlatformApp, but we want to load as a component extension.
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());

  const Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("component"));
  ASSERT_TRUE(extension);

  app_loaded_observer.Wait();
  ASSERT_TRUE(should_install.seen());

  ExtensionTestMessageListener launched_listener("Launched", false);
  chrome::OpenApplication(chrome::AppLaunchParams(browser()->profile(),
                                                  extension,
                                                  extension_misc::LAUNCH_NONE,
                                                  NEW_WINDOW));

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
}

// Component App Test 2 of 3: ensure an installed component app can be launched
// on a subsequent browser start, without requiring any install/upgrade logic
// to be run, then perform setup for step 3.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       PRE_ComponentAppBackgroundPage) {

  // Since the component app is now installed, re-adding it in the same profile
  // should not cause it to be re-installed. Instead, we wait for the OnLaunched
  // in a different observer (which would timeout if not the app was not
  // previously installed properly) and then check this observer to make sure it
  // never saw the NOTIFICATION_EXTENSION_INSTALLED event.
  CheckExtensionInstalledObserver should_not_install;
  const Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("component"));
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener launched_listener("Launched", false);
  chrome::OpenApplication(chrome::AppLaunchParams(browser()->profile(),
                                                  extension,
                                                  extension_misc::LAUNCH_NONE,
                                                  NEW_WINDOW));

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_FALSE(should_not_install.seen());

  // Simulate a "downgrade" from version 2 in the test manifest.json to 1.
  ExtensionPrefs* extension_prefs =
      extensions::ExtensionSystem::Get(browser()->profile())->
      extension_service()->extension_prefs();

  // Clear the registered events to ensure they are updated.
  extension_prefs->SetRegisteredEvents(extension->id(),
                                       std::set<std::string>());

  const base::StringValue old_version("1");
  std::string pref_path("extensions.settings.");
  pref_path += extension->id();
  pref_path += ".manifest.version";
  // TODO(joi): Do registrations up front.
  PrefRegistrySyncable* registry = static_cast<PrefRegistrySyncable*>(
      extension_prefs->pref_service()->DeprecatedGetPrefRegistry());
  registry->RegisterStringPref(
      pref_path.c_str(), std::string(), PrefRegistrySyncable::UNSYNCABLE_PREF);
  extension_prefs->pref_service()->Set(pref_path.c_str(), old_version);
}

// Component App Test 3 of 3: simulate a component extension upgrade that
// re-adds the OnLaunched event, and allows the app to be launched.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ComponentAppBackgroundPage) {
  CheckExtensionInstalledObserver should_install;
  // Since we are forcing an upgrade, we need to wait for the load again.
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());

  const Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("component"));
  ASSERT_TRUE(extension);
  app_loaded_observer.Wait();
  ASSERT_TRUE(should_install.seen());

  ExtensionTestMessageListener launched_listener("Launched", false);
  chrome::OpenApplication(chrome::AppLaunchParams(browser()->profile(),
                                                  extension,
                                                  extension_misc::LAUNCH_NONE,
                                                  NEW_WINDOW));

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
}

// Flakes on Windows: http://crbug.com/171450
#if defined(OS_WIN)
#define MAYBE_Messaging DISABLED_Messaging
#else
#define MAYBE_Messaging Messaging
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_Messaging) {
  ExtensionApiTest::ResultCatcher result_catcher;
  LoadAndLaunchPlatformApp("messaging/app2");
  LoadAndLaunchPlatformApp("messaging/app1");
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// TODO(jeremya): this doesn't work on GTK yet. See http://crbug.com/159450.
#if defined(TOOLKIT_GTK)
#define MAYBE_WebContentsHasFocus DISABLED_WebContentsHasFocus
#else
#define MAYBE_WebContentsHasFocus WebContentsHasFocus
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_WebContentsHasFocus) {
  const Extension* extension = LoadAndLaunchPlatformApp("minimal");
  ShellWindow* window = CreateShellWindow(extension);
  EXPECT_TRUE(window->web_contents()->GetRenderWidgetHostView()->HasFocus());
  CloseShellWindow(window);
}

}  // namespace extensions
