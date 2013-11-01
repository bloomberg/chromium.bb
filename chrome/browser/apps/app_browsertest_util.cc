// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"

#include "apps/app_window_contents.h"
#include "apps/shell_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/ui/apps/chrome_shell_window_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

using apps::ShellWindow;
using apps::ShellWindowRegistry;
using content::WebContents;

namespace utils = extension_function_test_utils;

namespace extensions {

PlatformAppBrowserTest::PlatformAppBrowserTest() {
  ChromeShellWindowDelegate::DisableExternalOpenForTesting();
}

void PlatformAppBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  // Skips ExtensionApiTest::SetUpCommandLine.
  ExtensionBrowserTest::SetUpCommandLine(command_line);

  // Make event pages get suspended quicker.
  command_line->AppendSwitchASCII(::switches::kEventPageIdleTime, "1");
  command_line->AppendSwitchASCII(::switches::kEventPageSuspendingTime, "1");
}

// static
ShellWindow* PlatformAppBrowserTest::GetFirstShellWindowForBrowser(
    Browser* browser) {
  ShellWindowRegistry* app_registry =
      ShellWindowRegistry::Get(browser->profile());
  const ShellWindowRegistry::ShellWindowList& shell_windows =
      app_registry->shell_windows();

  ShellWindowRegistry::const_iterator iter = shell_windows.begin();
  if (iter != shell_windows.end())
    return *iter;

  return NULL;
}

const Extension* PlatformAppBrowserTest::LoadAndLaunchPlatformApp(
    const char* name) {
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII(name));
  EXPECT_TRUE(extension);

  OpenApplication(AppLaunchParams(browser()->profile(),
                                  extension,
                                  extension_misc::LAUNCH_NONE,
                                  NEW_WINDOW));

  app_loaded_observer.Wait();

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

  OpenApplication(AppLaunchParams(browser()->profile(),
                                  extension,
                                  extension_misc::LAUNCH_NONE,
                                  NEW_WINDOW));

  app_loaded_observer.Wait();

  return extension;
}

WebContents* PlatformAppBrowserTest::GetFirstShellWindowWebContents() {
  ShellWindow* window = GetFirstShellWindow();
  if (window)
    return window->web_contents();

  return NULL;
}

ShellWindow* PlatformAppBrowserTest::GetFirstShellWindow() {
  return GetFirstShellWindowForBrowser(browser());
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

size_t PlatformAppBrowserTest::GetShellWindowCount() {
  return ShellWindowRegistry::Get(browser()->profile())->
      shell_windows().size();
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

ShellWindow* PlatformAppBrowserTest::CreateShellWindow(
    const Extension* extension) {
  return CreateShellWindowFromParams(extension, ShellWindow::CreateParams());
}

ShellWindow* PlatformAppBrowserTest::CreateShellWindowFromParams(
    const Extension* extension, const ShellWindow::CreateParams& params) {
  ShellWindow* window = new ShellWindow(browser()->profile(),
                                        new ChromeShellWindowDelegate(),
                                        extension);
  window->Init(GURL(std::string()),
               new apps::AppWindowContents(window),
               params);
  return window;
}

void PlatformAppBrowserTest::CloseShellWindow(ShellWindow* window) {
  content::WebContentsDestroyedWatcher destroyed_watcher(
      window->web_contents());
  window->GetBaseWindow()->Close();
  destroyed_watcher.Wait();
}

void PlatformAppBrowserTest::CallAdjustBoundsToBeVisibleOnScreenForShellWindow(
    ShellWindow* window,
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

void ExperimentalPlatformAppBrowserTest::SetUpCommandLine(
    CommandLine* command_line) {
  PlatformAppBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
}

}  // namespace extensions
