// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/platform_app_browsertest_util.h"

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/api/tabs/tabs.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

using content::WebContents;

namespace utils = extension_function_test_utils;

namespace extensions {

void PlatformAppBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);

  // Make event pages get suspended quicker.
  command_line->AppendSwitchASCII(switches::kEventPageIdleTime, "1");
  command_line->AppendSwitchASCII(switches::kEventPageUnloadingTime, "1");
}

const Extension* PlatformAppBrowserTest::LoadAndLaunchPlatformApp(
    const char* name) {
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII(name));
  EXPECT_TRUE(extension);

  application_launch::OpenApplication(application_launch::LaunchParams(
          browser()->profile(), extension, extension_misc::LAUNCH_NONE,
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
  ShellWindowRegistry* app_registry =
      ShellWindowRegistry::Get(browser()->profile());
  ShellWindowRegistry::const_iterator iter;
  ShellWindowRegistry::ShellWindowSet shell_windows =
      app_registry->shell_windows();
  for (iter = shell_windows.begin(); iter != shell_windows.end(); ++iter) {
    return *iter;
  }

  return NULL;
}

size_t PlatformAppBrowserTest::RunGetWindowsFunctionForExtension(
    const Extension* extension) {
  scoped_refptr<GetAllWindowsFunction> function = new GetAllWindowsFunction();
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
  scoped_refptr<GetWindowFunction> function = new GetWindowFunction();
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
  FilePath test_doc(test_data_dir_.AppendASCII(test_file));
  test_doc = test_doc.NormalizePathSeparators();
  command_line->AppendArgPath(test_doc);
}

ShellWindow* PlatformAppBrowserTest::CreateShellWindow(
    const Extension* extension) {
  ShellWindow::CreateParams params;
  return ShellWindow::Create(
      browser()->profile(), extension, GURL(""), params);
}

void PlatformAppBrowserTest::CloseShellWindow(ShellWindow* window) {
  content::WindowedNotificationObserver destroyed_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::NotificationService::AllSources());
  window->GetBaseWindow()->Close();
  destroyed_observer.Wait();
}

}  // namespace extensions
