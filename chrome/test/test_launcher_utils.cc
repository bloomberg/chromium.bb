// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/environment.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/test_launcher_utils.h"

namespace test_launcher_utils {

void PrepareBrowserCommandLineForTests(CommandLine* command_line) {
  // Turn off tip loading for tests; see http://crbug.com/17725
  command_line->AppendSwitch(switches::kDisableWebResources);

  // Turn off preconnects because they break the brittle python webserver.
  command_line->AppendSwitch(switches::kDisablePreconnect);

  // Don't show the first run ui.
  command_line->AppendSwitch(switches::kNoFirstRun);

  // No default browser check, it would create an info-bar (if we are not the
  // default browser) that could conflicts with some tests expectations.
  command_line->AppendSwitch(switches::kNoDefaultBrowserCheck);

  // Enable warning level logging so that we can see when bad stuff happens.
  command_line->AppendSwitch(switches::kEnableLogging);
  command_line->AppendSwitchASCII(switches::kLoggingLevel, "1");  // warning
}

bool OverrideUserDataDir(const FilePath& user_data_dir) {
  bool success = true;

  // PathService::Override() is the best way to change the user data directory.
  // This matches what is done in ChromeMain().
  success = PathService::Override(chrome::DIR_USER_DATA, user_data_dir);

#if defined(OS_LINUX)
  // Make sure the cache directory is inside our clear profile. Otherwise
  // the cache may contain data from earlier tests that could break the
  // current test.
  //
  // Note: we use an environment variable here, because we have to pass the
  // value to the child process. This is the simplest way to do it.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  success = success && env->SetVar("XDG_CACHE_HOME", user_data_dir.value());
#endif

  return success;
}

}  // namespace test_launcher_utils
