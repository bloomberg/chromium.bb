// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/launcher.h"
#include "apps/ui/native_app_window.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/api/app_runtime.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#endif

using apps::AppWindow;
using apps::AppWindowRegistry;
using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace app_runtime = extensions::core_api::app_runtime;

namespace extensions {

namespace {

// Non-abstract RenderViewContextMenu class.
class PlatformAppContextMenu : public RenderViewContextMenu {
 public:
  PlatformAppContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params)
      : RenderViewContextMenu(render_frame_host, params) {}

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

class ScopedPreviewTestingDelegate : PrintPreviewUI::TestingDelegate {
 public:
  explicit ScopedPreviewTestingDelegate(bool auto_cancel)
      : auto_cancel_(auto_cancel),
        total_page_count_(1),
        rendered_page_count_(0) {
    PrintPreviewUI::SetDelegateForTesting(this);
  }

  ~ScopedPreviewTestingDelegate() {
    PrintPreviewUI::SetDelegateForTesting(NULL);
  }

  // PrintPreviewUI::TestingDelegate implementation.
  virtual bool IsAutoCancelEnabled() OVERRIDE {
    return auto_cancel_;
  }

  // PrintPreviewUI::TestingDelegate implementation.
  virtual void DidGetPreviewPageCount(int page_count) OVERRIDE {
    total_page_count_ = page_count;
  }

  // PrintPreviewUI::TestingDelegate implementation.
  virtual void DidRenderPreviewPage(content::WebContents* preview_dialog)
      OVERRIDE {
    dialog_size_ = preview_dialog->GetContainerBounds().size();
    ++rendered_page_count_;
    CHECK(rendered_page_count_ <= total_page_count_);
    if (waiting_runner_ && rendered_page_count_ == total_page_count_) {
      waiting_runner_->Quit();
    }
  }

  void WaitUntilPreviewIsReady() {
    CHECK(!waiting_runner_);
    if (rendered_page_count_ < total_page_count_) {
      waiting_runner_ = new content::MessageLoopRunner;
      waiting_runner_->Run();
      waiting_runner_ = NULL;
    }
  }

  gfx::Size dialog_size() {
    return dialog_size_;
  }

 private:
  bool auto_cancel_;
  int total_page_count_;
  int rendered_page_count_;
  scoped_refptr<content::MessageLoopRunner> waiting_runner_;
  gfx::Size dialog_size_;
};

#if !defined(OS_CHROMEOS) && !defined(OS_WIN)
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
#endif  // !defined(OS_CHROMEOS) && !defined(OS_WIN)

#if !defined(OS_CHROMEOS)
const char kTestFilePath[] = "platform_apps/launch_files/test.txt";
#endif

}  // namespace

// Tests that CreateAppWindow doesn't crash if you close it straight away.
// LauncherPlatformAppBrowserTest relies on this behaviour, but is only run for
// ash, so we test that it works here.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, CreateAndCloseAppWindow) {
  const Extension* extension = LoadAndLaunchPlatformApp("minimal", "Launched");
  AppWindow* window = CreateAppWindow(extension);
  CloseAppWindow(window);
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
  LoadAndLaunchPlatformApp("minimal", "Launched");

  // The empty app doesn't add any context menu items, so its menu should
  // only include the developer tools.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents->GetMainFrame(), params));
  menu->Init();
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_TRUE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_TRUE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenu) {
  LoadAndLaunchPlatformApp("context_menu", "Launched");

  // The context_menu app has two context menu items. These, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents->GetMainFrame(), params));
  menu->Init();
  int first_extensions_command_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(first_extensions_command_id));
  ASSERT_TRUE(menu->HasCommandWithId(first_extensions_command_id + 1));
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
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents->GetMainFrame(), params));
  menu->Init();
  int extensions_custom_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id));
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id + 1));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  ASSERT_FALSE(
      menu->HasCommandWithId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_BACK));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_SAVE_PAGE));
  ASSERT_FALSE(menu->HasCommandWithId(IDC_CONTENT_CONTEXT_UNDO));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, AppWithContextMenuTextField) {
  LoadAndLaunchPlatformApp("context_menu", "Launched");

  // The context_menu app has one context menu item. This, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  params.is_editable = true;
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents->GetMainFrame(), params));
  menu->Init();
  int extensions_custom_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id));
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
  LoadAndLaunchPlatformApp("context_menu", "Launched");

  // The context_menu app has one context menu item. This, along with a
  // separator and the developer tools, is all that should be in the menu.
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  params.selection_text = base::ASCIIToUTF16("Hello World");
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents->GetMainFrame(), params));
  menu->Init();
  int extensions_custom_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id));
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
  LoadAndLaunchPlatformApp("context_menu_click", "Launched");

  // Test that the menu item shows up
  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);
  content::ContextMenuParams params;
  params.page_url = GURL("http://foo.bar");
  scoped_ptr<PlatformAppContextMenu> menu;
  menu.reset(new PlatformAppContextMenu(web_contents->GetMainFrame(), params));
  menu->Init();
  int extensions_custom_id =
      ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->HasCommandWithId(extensions_custom_id));

  // Execute the menu item
  ExtensionTestMessageListener onclicked_listener("onClicked fired for id1",
                                                  false);
  menu->ExecuteCommand(extensions_custom_id, 0);

  ASSERT_TRUE(onclicked_listener.WaitUntilSatisfied());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_DisallowNavigation DISABLED_DisallowNavigation
#else
#define MAYBE_DisallowNavigation DisallowNavigation
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_DisallowNavigation) {
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

// Failing on some Win and Linux buildbots.  See crbug.com/354425.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_Iframes DISABLED_Iframes
#else
#define MAYBE_Iframes Iframes
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_Iframes) {
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
  ui_test_utils::GetCookies(
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

  // And no app windows.
  ASSERT_EQ(0U, GetAppWindowCount());

  // Launch a platform app that shows a window.
  LoadAndLaunchPlatformApp("minimal", "Launched");
  ASSERT_EQ(1U, GetAppWindowCount());
  int app_window_id = GetFirstAppWindow()->session_id().id();

  // But it's not visible to the extensions API, it still thinks there's just
  // one browser window.
  ASSERT_EQ(1U, RunGetWindowsFunctionForExtension(extension));
  // It can't look it up by ID either
  ASSERT_FALSE(RunGetWindowFunctionForExtension(app_window_id, extension));

  // The app can also only see one window (its own).
  // TODO(jeremya): add an extension function to get an app window by ID, and
  // to get a list of all the app windows, so we can test this.

  // Launch another platform app that also shows a window.
  LoadAndLaunchPlatformApp("context_menu", "Launched");

  // There are two total app windows, but each app can only see its own.
  ASSERT_EQ(2U, GetAppWindowCount());
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
  AppLaunchParams params(
      browser()->profile(), extension, LAUNCH_CONTAINER_NONE, NEW_WINDOW);
  params.command_line = *CommandLine::ForCurrentProcess();
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

// Tests that launch data is sent through to a whitelisted extension if the file
// extension matches.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       LaunchWhiteListedExtensionWithFile) {
  SetCommandLineArg(kTestFilePath);
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/launch_whitelisted_ext_with_file"))
          << message_;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_LaunchWithFileExtensionAndMimeType DISABLED_LaunchWithFileExtensionAndMimeType
#else
#define MAYBE_LaunchWithFileExtensionAndMimeType LaunchWithFileExtensionAndMimeType
#endif

// Tests that launch data is sent through if the file extension and MIME type
// both match.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_LaunchWithFileExtensionAndMimeType) {
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

// Tests that launch data is sent through when the file has unknown extension
// but the MIME type can be sniffed and the sniffed type matches.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchWithSniffableType) {
  SetCommandLineArg("platform_apps/launch_files/test.unknownextension");
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/launch_file_by_extension_and_type")) << message_;
}

// Tests that launch data is sent through with the MIME type set to
// application/octet-stream if the file MIME type cannot be read.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, LaunchNoType) {
  SetCommandLineArg("platform_apps/launch_files/test_binary.unknownextension");
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
  LoadAndLaunchPlatformApp("open_link", "Launched");
  observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MutationEventsDisabled) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/mutation_events")) << message_;
}

// This appears to be unreliable on linux.
// TODO(stevenjb): Investigate and enable
#if defined(OS_LINUX) && !defined(USE_ASH)
#define MAYBE_AppWindowRestoreState DISABLED_AppWindowRestoreState
#else
#define MAYBE_AppWindowRestoreState AppWindowRestoreState
#endif
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_AppWindowRestoreState) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/restore_state"));
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       AppWindowAdjustBoundsToBeVisibleOnScreen) {
  const Extension* extension = LoadAndLaunchPlatformApp("minimal", "Launched");

  AppWindow* window = CreateAppWindow(extension);

  // The screen bounds didn't change, the cached bounds didn't need to adjust.
  gfx::Rect cached_bounds(80, 100, 400, 400);
  gfx::Rect cached_screen_bounds(0, 0, 1600, 900);
  gfx::Rect current_screen_bounds(0, 0, 1600, 900);
  gfx::Size minimum_size(200, 200);
  gfx::Rect bounds;
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(window,
                                                  cached_bounds,
                                                  cached_screen_bounds,
                                                  current_screen_bounds,
                                                  minimum_size,
                                                  &bounds);
  EXPECT_EQ(bounds, cached_bounds);

  // We have an empty screen bounds, the cached bounds didn't need to adjust.
  gfx::Rect empty_screen_bounds;
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(window,
                                                  cached_bounds,
                                                  empty_screen_bounds,
                                                  current_screen_bounds,
                                                  minimum_size,
                                                  &bounds);
  EXPECT_EQ(bounds, cached_bounds);

  // Cached bounds is completely off the new screen bounds in horizontal
  // locations. Expect to reposition the bounds.
  gfx::Rect horizontal_out_of_screen_bounds(-800, 100, 400, 400);
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
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
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
      window,
      vertical_out_of_screen_bounds,
      gfx::Rect(-1366, 0, 1600, 900),
      current_screen_bounds,
      minimum_size,
      &bounds);
  EXPECT_EQ(bounds, gfx::Rect(10, 500, 400, 400));

  // From a large screen resulotion to a small one. Expect it fit on screen.
  gfx::Rect big_cache_bounds(10, 10, 1000, 1000);
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(window,
                                                  big_cache_bounds,
                                                  gfx::Rect(0, 0, 1600, 1000),
                                                  gfx::Rect(0, 0, 800, 600),
                                                  minimum_size,
                                                  &bounds);
  EXPECT_EQ(bounds, gfx::Rect(0, 0, 800, 600));

  // Don't resize the bounds smaller than minimum size, when the minimum size is
  // larger than the screen.
  CallAdjustBoundsToBeVisibleOnScreenForAppWindow(window,
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
  // Runs a test inside a harness that opens DevTools on an app window.
  void RunTestWithDevTools(const char* name, int test_flags);
};

void PlatformAppDevToolsBrowserTest::RunTestWithDevTools(
    const char* name, int test_flags) {
  using content::DevToolsAgentHost;
  const Extension* extension = LoadAndLaunchPlatformApp(name, "Launched");
  ASSERT_TRUE(extension);
  AppWindow* window = GetFirstAppWindow();
  ASSERT_TRUE(window);
  ASSERT_EQ(window->window_key().empty(), (test_flags & HAS_ID) == 0);
  content::RenderViewHost* rvh = window->web_contents()->GetRenderViewHost();
  ASSERT_TRUE(rvh);

  // Ensure no DevTools open for the AppWindow, then open one.
  ASSERT_FALSE(DevToolsAgentHost::HasFor(rvh));
  DevToolsWindow::OpenDevToolsWindow(rvh);
  ASSERT_TRUE(DevToolsAgentHost::HasFor(rvh));

  if (test_flags & RELAUNCH) {
    // Close the AppWindow, and ensure it is gone.
    CloseAppWindow(window);
    ASSERT_FALSE(GetFirstAppWindow());

    // Relaunch the app and get a new AppWindow.
    content::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    OpenApplication(AppLaunchParams(
        browser()->profile(), extension, LAUNCH_CONTAINER_NONE, NEW_WINDOW));
    app_loaded_observer.Wait();
    window = GetFirstAppWindow();
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
      LoadAndLaunchPlatformApp("optional_permission_request", "Launched");
  ASSERT_TRUE(extension) << "Failed to load extension.";

  WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);

  // Verify that the app window has a dialog attached.
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
  const Extension* extension =
      LoadAndLaunchPlatformApp("reload", &launched_listener);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetFirstAppWindow());

  // Now tell the app to reload itself
  ExtensionTestMessageListener launched_listener2("Launched", false);
  launched_listener.Reply("reload");
  ASSERT_TRUE(launched_listener2.WaitUntilSatisfied());
  ASSERT_TRUE(GetFirstAppWindow());
}

namespace {

// Simple observer to check for
// NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED events to ensure
// installation does or does not occur in certain scenarios.
class CheckExtensionInstalledObserver : public content::NotificationObserver {
 public:
  CheckExtensionInstalledObserver() : seen_(false) {
    registrar_.Add(
        this,
        extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
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
// the script resource in the opened app window.
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
  OpenApplication(AppLaunchParams(
      browser()->profile(), extension, LAUNCH_CONTAINER_NONE, NEW_WINDOW));

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
  // never saw the NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED event.
  CheckExtensionInstalledObserver should_not_install;
  const Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("component"));
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener launched_listener("Launched", false);
  OpenApplication(AppLaunchParams(
      browser()->profile(), extension, LAUNCH_CONTAINER_NONE, NEW_WINDOW));

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  ASSERT_FALSE(should_not_install.seen());

  // Simulate a "downgrade" from version 2 in the test manifest.json to 1.
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser()->profile());

  // Clear the registered events to ensure they are updated.
  extensions::EventRouter::Get(browser()->profile())
      ->SetRegisteredEvents(extension->id(), std::set<std::string>());

  DictionaryPrefUpdate update(extension_prefs->pref_service(),
                              extensions::pref_names::kExtensions);
  base::DictionaryValue* dict = update.Get();
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
  OpenApplication(AppLaunchParams(
      browser()->profile(), extension, LAUNCH_CONTAINER_NONE, NEW_WINDOW));

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
}

// Fails on Win7. http://crbug.com/171450
#if defined(OS_WIN)
#define MAYBE_Messaging DISABLED_Messaging
#else
#define MAYBE_Messaging Messaging
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MAYBE_Messaging) {
  ExtensionApiTest::ResultCatcher result_catcher;
  LoadAndLaunchPlatformApp("messaging/app2", "Launched");
  LoadAndLaunchPlatformApp("messaging/app1", "Launched");
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
  LoadAndLaunchPlatformApp("minimal", "Launched");

  EXPECT_EQ(1LU, GetAppWindowCount());
  EXPECT_TRUE(GetFirstAppWindow()
                  ->web_contents()
                  ->GetRenderWidgetHostView()
                  ->HasFocus());
}

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
#define MAYBE_WindowDotPrintShouldBringUpPrintPreview \
    DISABLED_WindowDotPrintShouldBringUpPrintPreview
#else
#define MAYBE_WindowDotPrintShouldBringUpPrintPreview \
    WindowDotPrintShouldBringUpPrintPreview
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_WindowDotPrintShouldBringUpPrintPreview) {
  ScopedPreviewTestingDelegate preview_delegate(true);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/print_api")) << message_;
  preview_delegate.WaitUntilPreviewIsReady();
}

// This test verifies that http://crbug.com/297179 is fixed.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       DISABLED_ClosingWindowWhilePrintingShouldNotCrash) {
  ScopedPreviewTestingDelegate preview_delegate(false);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/print_api")) << message_;
  preview_delegate.WaitUntilPreviewIsReady();
  GetFirstAppWindow()->GetBaseWindow()->Close();
}

// This test currently only passes on OS X (on other platforms the print preview
// dialog's size is limited by the size of the window being printed).
#if !defined(OS_MACOSX)
#define MAYBE_PrintPreviewShouldNotBeTooSmall \
    DISABLED_PrintPreviewShouldNotBeTooSmall
#else
#define MAYBE_PrintPreviewShouldNotBeTooSmall \
    PrintPreviewShouldNotBeTooSmall
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_PrintPreviewShouldNotBeTooSmall) {
  // Print preview dialogs with widths less than 410 pixels will have preview
  // areas that are too small, and ones with heights less than 191 pixels will
  // have vertical scrollers for their controls that are too small.
  gfx::Size minimum_dialog_size(410, 191);
  ScopedPreviewTestingDelegate preview_delegate(false);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/print_api")) << message_;
  preview_delegate.WaitUntilPreviewIsReady();
  EXPECT_GE(preview_delegate.dialog_size().width(),
            minimum_dialog_size.width());
  EXPECT_GE(preview_delegate.dialog_size().height(),
            minimum_dialog_size.height());
  GetFirstAppWindow()->GetBaseWindow()->Close();
}


#if defined(OS_CHROMEOS)

class PlatformAppIncognitoBrowserTest : public PlatformAppBrowserTest,
                                        public AppWindowRegistry::Observer {
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

  // AppWindowRegistry::Observer implementation.
  virtual void OnAppWindowAdded(AppWindow* app_window) OVERRIDE {
    opener_app_ids_.insert(app_window->extension_id());
  }

 protected:
  // A set of ids of apps we've seen open a app window.
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
  EventRouter* router = EventRouter::Get(incognito_profile);
  ASSERT_TRUE(router != NULL);
  while (!router->ExtensionHasEventListener(
      file_manager->id(), app_runtime::OnLaunched::kEventName)) {
    content::RunAllPendingInMessageLoop();
  }

  // Listen for new app windows so we see the file manager app launch itself.
  AppWindowRegistry* registry = AppWindowRegistry::Get(incognito_profile);
  ASSERT_TRUE(registry != NULL);
  registry->AddObserver(this);

  OpenApplication(AppLaunchParams(
      incognito_profile, file_manager, 0, chrome::HOST_DESKTOP_TYPE_NATIVE));

  while (!ContainsKey(opener_app_ids_, file_manager->id())) {
    content::RunAllPendingInMessageLoop();
  }
}

class RestartDeviceTest : public PlatformAppBrowserTest {
 public:
  RestartDeviceTest()
      : power_manager_client_(NULL),
        mock_user_manager_(NULL) {}
  virtual ~RestartDeviceTest() {}

  // PlatformAppBrowserTest overrides
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();

    chromeos::FakeDBusThreadManager* dbus_manager =
        new chromeos::FakeDBusThreadManager;
    dbus_manager->SetFakeClients();
    power_manager_client_ = new chromeos::FakePowerManagerClient;
    dbus_manager->SetPowerManagerClient(
        scoped_ptr<chromeos::PowerManagerClient>(power_manager_client_));
    chromeos::DBusThreadManager::SetInstanceForTesting(dbus_manager);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    PlatformAppBrowserTest::SetUpOnMainThread();

    mock_user_manager_ = new chromeos::MockUserManager;
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(mock_user_manager_));

    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
        .WillRepeatedly(testing::Return(true));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    user_manager_enabler_.reset();
    PlatformAppBrowserTest::TearDownOnMainThread();
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    PlatformAppBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  int num_request_restart_calls() const {
    return power_manager_client_->num_request_restart_calls();
  }

 private:
  chromeos::FakePowerManagerClient* power_manager_client_;
  chromeos::MockUserManager* mock_user_manager_;
  scoped_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(RestartDeviceTest);
};

// Tests that chrome.runtime.restart would request device restart in
// ChromeOS kiosk mode.
IN_PROC_BROWSER_TEST_F(RestartDeviceTest, Restart) {
  ASSERT_EQ(0, num_request_restart_calls());

  ExtensionTestMessageListener launched_listener("Launched", true);
  const Extension* extension = LoadAndLaunchPlatformApp("restart_device",
                                                        &launched_listener);
  ASSERT_TRUE(extension);

  launched_listener.Reply("restart");
  ExtensionTestMessageListener restart_requested_listener("restartRequested",
                                                          false);
  ASSERT_TRUE(restart_requested_listener.WaitUntilSatisfied());

  EXPECT_EQ(1, num_request_restart_calls());
}

#endif  // defined(OS_CHROMEOS)

// Test that when an application is uninstalled and re-install it does not have
// access to the previously set data.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, ReinstallDataCleanup) {
  // The application is installed and launched. After the 'Launched' message is
  // acknowledged by the browser process, the application will test that some
  // data are not installed and then install them. The application will then be
  // uninstalled and the same process will be repeated.
  std::string extension_id;

  {
    const Extension* extension =
        LoadAndLaunchPlatformApp("reinstall_data_cleanup", "Launched");
    ASSERT_TRUE(extension);
    extension_id = extension->id();

    ExtensionApiTest::ResultCatcher result_catcher;
    EXPECT_TRUE(result_catcher.GetNextResult());
  }

  UninstallExtension(extension_id);
  content::RunAllPendingInMessageLoop();

  {
    const Extension* extension =
        LoadAndLaunchPlatformApp("reinstall_data_cleanup", "Launched");
    ASSERT_TRUE(extension);
    ASSERT_EQ(extension_id, extension->id());

    ExtensionApiTest::ResultCatcher result_catcher;
    EXPECT_TRUE(result_catcher.GetNextResult());
  }
}

}  // namespace extensions
