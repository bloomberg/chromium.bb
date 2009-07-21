// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include "base/process_util.h"

static const char* GetDesktopName() {
#if defined(GOOGLE_CHROME_BUILD)
  return "google-chrome.desktop";
#else  // CHROMIUM_BUILD
  static const char* name = NULL;
  if (!name) {
    // Allow $CHROME_DESKTOP to override the built-in value, so that development
    // versions can set themselves as the default without interfering with
    // non-official, packaged versions using the built-in value.
    name = getenv("CHROME_DESKTOP");
    if (!name)
      name = "chromium-browser.desktop";
  }
  return name;
#endif
}

// We delegate the difficult of setting the default browser in Linux desktop
// environments to a new xdg utility, xdg-settings. We'll have to include a copy
// of it for this to work, obviously, but that's actually the suggested approach
// for xdg utilities anyway.

bool ShellIntegration::SetAsDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("set");
  argv.push_back("default-web-browser");
  argv.push_back(GetDesktopName());

  // xdg-settings internally runs xdg-mime, which uses mv to move newly-created
  // files on top of originals after making changes to them. In the event that
  // the original files are owned by another user (e.g. root, which can happen
  // if they are updated within sudo), mv will prompt the user to confirm if
  // standard input is a terminal (otherwise it just does it). So make sure it's
  // not, to avoid locking everything up waiting for mv.
  int devnull = open("/dev/null", O_RDONLY);
  if (devnull < 0)
    return false;
  base::file_handle_mapping_vector no_stdin;
  no_stdin.push_back(std::make_pair(devnull, STDIN_FILENO));

  base::ProcessHandle handle;
  if (!base::LaunchApp(argv, no_stdin, false, &handle)) {
    close(devnull);
    return false;
  }
  close(devnull);

  int success_code;
  base::WaitForExitCode(handle, &success_code);
  return success_code == EXIT_SUCCESS;
}

static bool GetDefaultBrowser(std::string* result) {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("get");
  argv.push_back("default-web-browser");
  return base::GetAppOutput(CommandLine(argv), result);
}

bool ShellIntegration::IsDefaultBrowser() {
  std::string browser;
  if (!GetDefaultBrowser(&browser)) {
    // If GetDefaultBrowser() fails, we assume xdg-settings does not work for
    // some reason, and that we should pretend we're the default browser to
    // avoid giving repeated prompts to set ourselves as the default browser.
    // TODO: Really, being the default browser should be a ternary query: yes,
    // no, and "don't know" so that the UI can reflect this more accurately.
    return true;
  }
  // Allow for an optional newline at the end.
  if (browser.length() > 0 && browser[browser.length() - 1] == '\n')
    browser.resize(browser.length() - 1);
  return !browser.compare(GetDesktopName());
}

bool ShellIntegration::IsFirefoxDefaultBrowser() {
  std::string browser;
  // We don't care about the return value here.
  GetDefaultBrowser(&browser);
  return browser.find("irefox") != std::string::npos;
}
