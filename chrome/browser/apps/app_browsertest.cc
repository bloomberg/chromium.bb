// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/launcher.h"
#include "apps/native_app_window.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/app_runtime.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

using apps::ShellWindow;
using apps::ShellWindowRegistry;
using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace extensions {

namespace app_runtime = api::app_runtime;

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

bool CopyTestDataAndSetCommandLineArg(
    const base::FilePath& test_data_file,
    const base::FilePath& temp_dir,
    const char* filename) {
  base::FilePath path = temp_dir.AppendASCII(
      filename).NormalizePathSeparators();
  if (!(base::CopyFile(test_data_file, path)))
    return false;

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendArgPath(path);
  return true;
}

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
  content::ContextMenuParams params;
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
  content::ContextMenuParams params;
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
  content::ContextMenuParams params;
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
  content::ContextMenuParams params;
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
  content::ContextMenuParams params;
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
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.bar");
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents, params));
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST));

  // Execute the menu item
  ExtensionTestMessageListener onclicked_listener("onClicked fired for id1",
                                                  false);
  menu->ExecuteCommand(IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST, 0);

  ASSERT_TRUE(onclicked_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, DisallowNavigation) {
  TabsAddedNotificationObserver observer(2);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/navigation")) << message_;

  observer.Wait();
  ASSERT_EQ(2U, observer.tabs().size());
  EXPECT_EQ(std::string(chrome::kExtensionInvalidRequestURL),
            observer.tabs()[0]->GetURL().spec());
  EXPECT_EQ("http://chromium.org/",
            observer.tabs()[1]->GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, Iframes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
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
// It is flaky: http://crbug.com/223467
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
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load a (non-app) page under the "localhost" origin that sets a cookie.
  GURL set_cookie_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/isolation/set_cookie.html");
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

// See crbug.com/248441
#if defined(OS_WIN)
#define MAYBE_ExtensionWindowingApis DISABLED_ExtensionWindowingApis
#else
#define MAYBE_ExtensionWindowingApis ExtensionWindowingApis
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_ExtensionWindowingApis) {
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
  ShellWindowRegistry::ShellWindowList shell_windows =
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
// the AppLaunchParams.current_directory field.
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
  AppLaunchParams params(browser()->profile(), extension,
                         extension_misc::LAUNCH_NONE, NEW_WINDOW);
  params.command_line = CommandLine::ForCurrentProcess();
  params.current_directory = test_data_dir_;
  OpenApplication(params);

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    ASSERT_TRUE(0);
  }
}

// Tests that launch data is sent through if the file extension matches.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithFileExtension) {
  SetCommandLineArg(kTestFilePath);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_file_by_extension"))
      << message_;
}

// Tests that launch data is sent through if the file extension and MIME type
// both match.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       LaunchWithFileExtensionAndMimeType) {
  SetCommandLineArg(kTestFilePath);
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/launch_file_by_extension_and_type")) << message_;
}

// Tests that launch data is sent through for a file with no extension if a
// handler accepts "".
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithFileWithoutExtension) {
  SetCommandLineArg("platform_apps/launch_files/test");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_file_with_no_extension"))
      << message_;
}

#if !defined(OS_WIN)
// Tests that launch data is sent through for a file with an empty extension if
// a handler accepts "".
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithFileEmptyExtension) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ClearCommandLineArgs();
  ASSERT_TRUE(CopyTestDataAndSetCommandLineArg(
      test_data_dir_.AppendASCII(kTestFilePath),
      temp_dir.path(),
      "test."));
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_file_with_no_extension"))
      << message_;
}

// Tests that launch data is sent through for a file with an empty extension if
// a handler accepts *.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       LaunchWithFileEmptyExtensionAcceptAny) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ClearCommandLineArgs();
  ASSERT_TRUE(CopyTestDataAndSetCommandLineArg(
      test_data_dir_.AppendASCII(kTestFilePath),
      temp_dir.path(),
      "test."));
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/launch_file_with_any_extension")) << message_;
}
#endif

// Tests that launch data is sent through for a file with no extension if a
// handler accepts *.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       LaunchWithFileWithoutExtensionAcceptAny) {
  SetCommandLineArg("platform_apps/launch_files/test");
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/launch_file_with_any_extension")) << message_;
}

// Tests that launch data is sent through for a file with an extension if a
// handler accepts *.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       LaunchWithFileAcceptAnyExtension) {
  SetCommandLineArg(kTestFilePath);
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/launch_file_with_any_extension")) << message_;
}

// Tests that no launch data is sent through if the file has the wrong
// extension.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithWrongExtension) {
  SetCommandLineArg(kTestFilePath);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_wrong_extension"))
      << message_;
}

// Tests that no launch data is sent through if the file has no extension but
// the handler requires a specific extension.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithWrongEmptyExtension) {
  SetCommandLineArg("platform_apps/launch_files/test");
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_wrong_extension"))
      << message_;
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

// Tests that launch data is sent through with the MIME type set to
// application/octet-stream if the file MIME type cannot be read.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchNoType) {
  SetCommandLineArg("platform_apps/launch_files/test.unknownextension");
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/launch_application_octet_stream")) << message_;
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

// Tests that the file is created if the file does not exist and the app has the
// fileSystem.write permission.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchNewFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ClearCommandLineArgs();
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendArgPath(temp_dir.path().AppendASCII("new_file.txt"));
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/launch_new_file")) << message_;
}

#endif  // !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, OpenLink) {
  ASSERT_TRUE(StartEmbeddedTestServer());
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
//
// TODO(erg): Now a linux_aura asan regression too: http://crbug.com/304555
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_ShellWindowRestorePosition DISABLED_ShellWindowRestorePosition
#else
#define MAYBE_ShellWindowRestorePosition ShellWindowRestorePosition
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_ShellWindowRestorePosition) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/geometry"));
}

// This appears to be unreliable on linux.
// TODO(stevenjb): Investigate and enable
#if defined(OS_LINUX) && !defined(USE_ASH)
#define MAYBE_ShellWindowRestoreState DISABLED_ShellWindowRestoreState
#else
#define MAYBE_ShellWindowRestoreState ShellWindowRestoreState
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_ShellWindowRestoreState) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/restore_state"));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       ShellWindowAdjustBoundsToBeVisibleOnScreen) {
  const Extension* extension = LoadAndLaunchPlatformApp("minimal");
  ShellWindow* window = CreateShellWindow(extension);

  // The screen bounds didn't change, the cached bounds didn't need to adjust.
  gfx::Rect cached_bounds(80, 100, 400, 400);
  gfx::Rect cached_screen_bounds(0, 0, 1600, 900);
  gfx::Rect current_screen_bounds(0, 0, 1600, 900);
  gfx::Size minimum_size(200, 200);
  gfx::Rect bounds;
  CallAdjustBoundsToBeVisibleOnScreenForShellWindow(window,
                                                    cached_bounds,
                                                    cached_screen_bounds,
                                                    current_screen_bounds,
                                                    minimum_size,
                                                    &bounds);
  EXPECT_EQ(bounds, cached_bounds);

  // We have an empty screen bounds, the cached bounds didn't need to adjust.
  gfx::Rect empty_screen_bounds;
  CallAdjustBoundsToBeVisibleOnScreenForShellWindow(window,
                                                    cached_bounds,
                                                    empty_screen_bounds,
                                                    current_screen_bounds,
                                                    minimum_size,
                                                    &bounds);
  EXPECT_EQ(bounds, cached_bounds);

  // Cached bounds is completely off the new screen bounds in horizontal
  // locations. Expect to reposition the bounds.
  gfx::Rect horizontal_out_of_screen_bounds(-800, 100, 400, 400);
  CallAdjustBoundsToBeVisibleOnScreenForShellWindow(
      window,
      horizontal_out_of_screen_bounds,
      gfx::Rect(-1366, 0, 1600, 900),
      current_screen_bounds,
      minimum_size,
      &bounds);
  EXPECT_EQ(bounds, gfx::Rect(0, 100, 400, 400));

  // Cached bounds is completely off the new screen bounds in vertical
  // locations. Expect to reposition the bounds.
  gfx::Rect vertical_out_of_screen_bounds(10, 1000, 400, 400);
  CallAdjustBoundsToBeVisibleOnScreenForShellWindow(
      window,
      vertical_out_of_screen_bounds,
      gfx::Rect(-1366, 0, 1600, 900),
      current_screen_bounds,
      minimum_size,
      &bounds);
  EXPECT_EQ(bounds, gfx::Rect(10, 500, 400, 400));

  // From a large screen resulotion to a small one. Expect it fit on screen.
  gfx::Rect big_cache_bounds(10, 10, 1000, 1000);
  CallAdjustBoundsToBeVisibleOnScreenForShellWindow(
      window,
      big_cache_bounds,
      gfx::Rect(0, 0, 1600, 1000),
      gfx::Rect(0, 0, 800, 600),
      minimum_size,
      &bounds);
  EXPECT_EQ(bounds, gfx::Rect(0, 0, 800, 600));

  // Don't resize the bounds smaller than minimum size, when the minimum size is
  // larger than the screen.
  CallAdjustBoundsToBeVisibleOnScreenForShellWindow(
      window,
      big_cache_bounds,
      gfx::Rect(0, 0, 1600, 1000),
      gfx::Rect(0, 0, 800, 600),
      gfx::Size(900, 900),
      &bounds);
  EXPECT_EQ(bounds, gfx::Rect(0, 0, 900, 900));
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
    OpenApplication(AppLaunchParams(browser()->profile(),
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

// http://crbug.com/246634
#if defined(OS_CHROMEOS)
#define MAYBE_ReOpenedWithID DISABLED_ReOpenedWithID
#else
#define MAYBE_ReOpenedWithID ReOpenedWithID
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppDevToolsBrowserTest, MAYBE_ReOpenedWithID) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  RunTestWithDevTools("minimal_id", RELAUNCH | HAS_ID);
}

// http://crbug.com/246999
#if defined(OS_CHROMEOS) || defined(OS_WIN)
#define MAYBE_ReOpenedWithURL DISABLED_ReOpenedWithURL
#else
#define MAYBE_ReOpenedWithURL ReOpenedWithURL
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppDevToolsBrowserTest, MAYBE_ReOpenedWithURL) {
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
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

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
  OpenApplication(AppLaunchParams(browser()->profile(),
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
  OpenApplication(AppLaunchParams(browser()->profile(),
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
  extensions::ExtensionSystem::Get(browser()->profile())->event_router()->
      SetRegisteredEvents(extension->id(), std::set<std::string>());

  DictionaryPrefUpdate update(extension_prefs->pref_service(),
                              prefs::kExtensionsPref);
  DictionaryValue* dict = update.Get();
  std::string key(extension->id());
  key += ".manifest.version";
  dict->SetString(key, "1");
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
  OpenApplication(AppLaunchParams(browser()->profile(),
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

// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
#define MAYBE_WebContentsHasFocus DISABLED_WebContentsHasFocus
#else
// This test depends on focus and so needs to be in interactive_ui_tests.
// http://crbug.com/227041
#define MAYBE_WebContentsHasFocus DISABLED_WebContentsHasFocus
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_WebContentsHasFocus) {
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("minimal");
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  EXPECT_EQ(1LU, GetShellWindowCount());
  ShellWindowRegistry::ShellWindowList shell_windows = ShellWindowRegistry::Get(
      browser()->profile())->shell_windows();
  EXPECT_TRUE((*shell_windows.begin())->web_contents()->
      GetRenderWidgetHostView()->HasFocus());
}

// The next two tests will only run automatically with Chrome branded builds
// because they require the PDF preview plug-in. To run these tests manually for
// Chromium (non-Chrome branded) builds in a development environment:
//
//   1) Remove "MAYBE_" in the first line of each test definition
//   2) Build Chromium browser_tests
//   3) Make a copy of the PDF plug-in from a recent version of Chrome (Canary
//      or a recent development build) to your Chromium build:
//      - On Linux and Chrome OS, copy /opt/google/chrome/libpdf.so to
//        <path-to-your-src>/out/Debug
//      - On OS X, copy PDF.plugin from
//        <recent-chrome-app-folder>/*/*/*/*/"Internet Plug-Ins" to
//        <path-to-your-src>/out/Debug/Chromium.app/*/*/*/*/"Internet Plug-Ins"
//   4) Run browser_tests with the --enable-print-preview flag

#if !defined(GOOGLE_CHROME_BUILD)
#define MAYBE_WindowDotPrintShouldBringUpPrintPreview \
    DISABLED_WindowDotPrintShouldBringUpPrintPreview
#else
#define MAYBE_WindowDotPrintShouldBringUpPrintPreview \
    WindowDotPrintShouldBringUpPrintPreview
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_WindowDotPrintShouldBringUpPrintPreview) {
  PrintPreviewUI::ScopedAutoCancelForTesting auto_cancel;
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/print_api")) << message_;
  EXPECT_EQ(1, auto_cancel.GetCountForTesting());
}

// Currently fails on OS X.
#if !defined(GOOGLE_CHROME_BUILD) || defined(OS_MACOSX)
#define MAYBE_ClosingWindowWhilePrintingShouldNotCrash \
    DISABLED_ClosingWindowWhilePrintingShouldNotCrash
#else
#define MAYBE_ClosingWindowWhilePrintingShouldNotCrash \
    ClosingWindowWhilePrintingShouldNotCrash
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_ClosingWindowWhilePrintingShouldNotCrash) {
  PrintPreviewUI::ScopedAutoCancelForTesting auto_cancel;
  ASSERT_TRUE(RunPlatformAppTestWithArg("platform_apps/print_api",
                                        "close")) << message_;
  EXPECT_EQ(0, auto_cancel.GetCountForTesting());
}


#if defined(OS_CHROMEOS)

class PlatformAppIncognitoBrowserTest : public PlatformAppBrowserTest,
                                        public ShellWindowRegistry::Observer {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Tell chromeos to launch in Guest mode, aka incognito.
    command_line->AppendSwitch(switches::kIncognito);
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }
  virtual void SetUp() OVERRIDE {
    // Make sure the file manager actually gets loaded.
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    PlatformAppBrowserTest::SetUp();
  }

  // ShellWindowRegistry::Observer implementation.
  virtual void OnShellWindowAdded(ShellWindow* shell_window) OVERRIDE {
    opener_app_ids_.insert(shell_window->extension()->id());
  }
  virtual void OnShellWindowIconChanged(ShellWindow* shell_window) OVERRIDE {}
  virtual void OnShellWindowRemoved(ShellWindow* shell_window) OVERRIDE {}

 protected:
  // A set of ids of apps we've seen open a shell window.
  std::set<std::string> opener_app_ids_;
};

IN_PROC_BROWSER_TEST_F(PlatformAppIncognitoBrowserTest, IncognitoComponentApp) {
  // Get the file manager app.
  const Extension* file_manager = extension_service()->GetExtensionById(
      "hhaomjibdihmijegdhdafkllkbggdgoj", false);
  ASSERT_TRUE(file_manager != NULL);
  Profile* incognito_profile = profile()->GetOffTheRecordProfile();
  ASSERT_TRUE(incognito_profile != NULL);

  // Wait until the file manager has had a chance to register its listener
  // for the launch event.
  EventRouter* router = ExtensionSystem::Get(incognito_profile)->event_router();
  ASSERT_TRUE(router != NULL);
  while (!router->ExtensionHasEventListener(
      file_manager->id(), app_runtime::OnLaunched::kEventName)) {
    content::RunAllPendingInMessageLoop();
  }

  // Listen for new shell windows so we see the file manager app launch itself.
  ShellWindowRegistry* registry = ShellWindowRegistry::Get(incognito_profile);
  ASSERT_TRUE(registry != NULL);
  registry->AddObserver(this);

  OpenApplication(AppLaunchParams(incognito_profile, file_manager, 0));

  while (!ContainsKey(opener_app_ids_, file_manager->id())) {
    content::RunAllPendingInMessageLoop();
  }
}

#endif  // defined(OS_CHROMEOS)


}  // namespace extensions
