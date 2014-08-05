// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"

#include "apps/app_window_contents.h"
#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

using apps::AppWindow;
using apps::AppWindowRegistry;
using content::WebContents;

namespace {

const char kAppWindowTestApp[] = "app_window/generic";

}  // namespace

namespace utils = extension_function_test_utils;

namespace extensions {

PlatformAppBrowserTest::PlatformAppBrowserTest() {
  ChromeAppDelegate::DisableExternalOpenForTesting();
}

void PlatformAppBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  // Skips ExtensionApiTest::SetUpCommandLine.
  ExtensionBrowserTest::SetUpCommandLine(command_line);

  // Make event pages get suspended quicker.
  command_line->AppendSwitchASCII(switches::kEventPageIdleTime, "1000");
  command_line->AppendSwitchASCII(switches::kEventPageSuspendingTime, "1000");
}

// static
AppWindow* PlatformAppBrowserTest::GetFirstAppWindowForBrowser(
    Browser* browser) {
  AppWindowRegistry* app_registry = AppWindowRegistry::Get(browser->profile());
  const AppWindowRegistry::AppWindowList& app_windows =
      app_registry->app_windows();

  AppWindowRegistry::const_iterator iter = app_windows.begin();
  if (iter != app_windows.end())
    return *iter;

  return NULL;
}

const Extension* PlatformAppBrowserTest::LoadAndLaunchPlatformApp(
    const char* name,
    ExtensionTestMessageListener* listener) {
  DCHECK(listener);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII(name));
  EXPECT_TRUE(extension);

  LaunchPlatformApp(extension);

  EXPECT_TRUE(listener->WaitUntilSatisfied()) << "'" << listener->message()
                                              << "' message was not receieved";

  return extension;
}

const Extension* PlatformAppBrowserTest::LoadAndLaunchPlatformApp(
    const char* name,
    const std::string& message) {
  ExtensionTestMessageListener launched_listener(message, false);
  const Extension* extension =
      LoadAndLaunchPlatformApp(name, &launched_listener);

  return extension;
}

const Extension* PlatformAppBrowserTest::InstallPlatformApp(
    const char* name) {
  const Extension* extension = InstallExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII(name), 1);
  EXPECT_TRUE(extension);

  return extension;
}

const Extension* PlatformAppBrowserTest::InstallAndLaunchPlatformApp(
    const char* name) {
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());

  const Extension* extension = InstallPlatformApp(name);

  LaunchPlatformApp(extension);

  app_loaded_observer.Wait();

  return extension;
}

void PlatformAppBrowserTest::LaunchPlatformApp(const Extension* extension) {
  OpenApplication(AppLaunchParams(
      browser()->profile(), extension, LAUNCH_CONTAINER_NONE, NEW_WINDOW));
}

WebContents* PlatformAppBrowserTest::GetFirstAppWindowWebContents() {
  AppWindow* window = GetFirstAppWindow();
  if (window)
    return window->web_contents();

  return NULL;
}

AppWindow* PlatformAppBrowserTest::GetFirstAppWindow() {
  return GetFirstAppWindowForBrowser(browser());
}

apps::AppWindow* PlatformAppBrowserTest::GetFirstAppWindowForApp(
    const std::string& app_id) {
  AppWindowRegistry* app_registry =
      AppWindowRegistry::Get(browser()->profile());
  const AppWindowRegistry::AppWindowList& app_windows =
      app_registry->GetAppWindowsForApp(app_id);

  AppWindowRegistry::const_iterator iter = app_windows.begin();
  if (iter != app_windows.end())
    return *iter;

  return NULL;
}

size_t PlatformAppBrowserTest::RunGetWindowsFunctionForExtension(
    const Extension* extension) {
  scoped_refptr<WindowsGetAllFunction> function = new WindowsGetAllFunction();
  function->set_extension(extension);
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[]",
                                              browser())));
  return result->GetSize();
}

bool PlatformAppBrowserTest::RunGetWindowFunctionForExtension(
    int window_id,
    const Extension* extension) {
  scoped_refptr<WindowsGetFunction> function = new WindowsGetFunction();
  function->set_extension(extension);
  utils::RunFunction(
          function.get(),
          base::StringPrintf("[%u]", window_id),
          browser(),
          utils::NONE);
  return function->GetResultList() != NULL;
}

size_t PlatformAppBrowserTest::GetAppWindowCount() {
  return AppWindowRegistry::Get(browser()->profile())->app_windows().size();
}

size_t PlatformAppBrowserTest::GetAppWindowCountForApp(
    const std::string& app_id) {
  return AppWindowRegistry::Get(browser()->profile())
      ->GetAppWindowsForApp(app_id)
      .size();
}

void PlatformAppBrowserTest::ClearCommandLineArgs() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  CommandLine::StringVector args = command_line->GetArgs();
  CommandLine::StringVector argv = command_line->argv();
  for (size_t i = 0; i < args.size(); i++)
    argv.pop_back();
  command_line->InitFromArgv(argv);
}

void PlatformAppBrowserTest::SetCommandLineArg(const std::string& test_file) {
  ClearCommandLineArgs();
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  base::FilePath test_doc(test_data_dir_.AppendASCII(test_file));
  test_doc = test_doc.NormalizePathSeparators();
  command_line->AppendArgPath(test_doc);
}

AppWindow* PlatformAppBrowserTest::CreateAppWindow(const Extension* extension) {
  return CreateAppWindowFromParams(extension, AppWindow::CreateParams());
}

AppWindow* PlatformAppBrowserTest::CreateAppWindowFromParams(
    const Extension* extension,
    const AppWindow::CreateParams& params) {
  AppWindow* window =
      new AppWindow(browser()->profile(), new ChromeAppDelegate(), extension);
  window->Init(
      GURL(std::string()), new apps::AppWindowContentsImpl(window), params);
  return window;
}

void PlatformAppBrowserTest::CloseAppWindow(AppWindow* window) {
  content::WebContentsDestroyedWatcher destroyed_watcher(
      window->web_contents());
  window->GetBaseWindow()->Close();
  destroyed_watcher.Wait();
}

void PlatformAppBrowserTest::CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
    AppWindow* window,
    const gfx::Rect& cached_bounds,
    const gfx::Rect& cached_screen_bounds,
    const gfx::Rect& current_screen_bounds,
    const gfx::Size& minimum_size,
    gfx::Rect* bounds) {
  window->AdjustBoundsToBeVisibleOnScreen(cached_bounds,
                                          cached_screen_bounds,
                                          current_screen_bounds,
                                          minimum_size,
                                          bounds);
}

apps::AppWindow* PlatformAppBrowserTest::CreateTestAppWindow(
    const std::string& window_create_options) {
  ExtensionTestMessageListener launched_listener("launched", true);
  ExtensionTestMessageListener loaded_listener("window_loaded", false);

  // Load and launch the test app.
  const Extension* extension =
      LoadAndLaunchPlatformApp(kAppWindowTestApp, &launched_listener);
  EXPECT_TRUE(extension);
  EXPECT_TRUE(launched_listener.WaitUntilSatisfied());

  // Send the options for window creation.
  launched_listener.Reply(window_create_options);

  // Wait for the window to be opened and loaded.
  EXPECT_TRUE(loaded_listener.WaitUntilSatisfied());

  EXPECT_EQ(1U, GetAppWindowCount());
  AppWindow* app_window = GetFirstAppWindow();
  return app_window;
}

void ExperimentalPlatformAppBrowserTest::SetUpCommandLine(
    CommandLine* command_line) {
  PlatformAppBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
}

}  // namespace extensions
