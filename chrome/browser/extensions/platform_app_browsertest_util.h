// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_BROWSERTEST_UTIL_H_


#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/extensions/shell_window.h"

namespace content {
class WebContents;
}

class CommandLine;
class ShellWindow;

namespace extensions {
class Extension;

class PlatformAppBrowserTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

 protected:
  // Runs the app named |name| out of the platform_apps subdirectory. Waits
  // until it is launched.
  const Extension* LoadAndLaunchPlatformApp(const char* name);

  // Installs and runs the app named |name| out of the platform_apps
  // subdirectory. Waits until it is launched.
  const Extension* InstallAndLaunchPlatformApp(const char* name);

  // Gets the WebContents associated with the first shell window that is found
  // (most tests only deal with one platform app window, so this is good
  // enough).
  content::WebContents* GetFirstShellWindowWebContents();

  // Gets the first shell window that is found (most tests only deal with one
  // platform app window, so this is good enough).
  ShellWindow* GetFirstShellWindow();

  // Runs chrome.windows.getAll for the given extension and returns the number
  // of windows that the function returns.
  size_t RunGetWindowsFunctionForExtension(const Extension* extension);

  // Runs chrome.windows.get(|window_id|) for the the given extension and
  // returns whether or not a window was found.
  bool RunGetWindowFunctionForExtension(int window_id,
                                        const Extension* extension);

  // Returns the number of shell windows.
  size_t GetShellWindowCount();

  // The command line already has an argument on it - about:blank, which
  // is set by InProcessBrowserTest::PrepareTestCommandLine. For platform app
  // launch tests we need to clear this.
  void ClearCommandLineArgs();

  // Sets up the command line for running platform apps.
  void SetCommandLineArg(const std::string& test_file);

  // Creates an empty shell window for |extension|.
  ShellWindow* CreateShellWindow(const Extension* extension);

  ShellWindow* CreateShellWindowFromParams(
      const Extension* extension, const ShellWindow::CreateParams& params);

  // Closes |window| and waits until it's gone.
  void CloseShellWindow(ShellWindow* window);
};

class ExperimentalPlatformAppBrowserTest : public PlatformAppBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
};

}

#endif  // CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_BROWSERTEST_UTIL_H_
