// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/test_launcher_utils.h"

namespace {

// TODO(phajdan.jr): remove this flag and fix its users.
// We should use base/test/test_timeouts and not custom flags.
static const char kTestTerminateTimeoutFlag[] = "test-terminate-timeout";

// A multiplier for slow tests. We generally avoid multiplying
// test timeouts by any constants. Here it is used as last resort
// to implement the SLOW_ test prefix.
static const int kSlowTestTimeoutMultiplier = 5;

}  // namespace

namespace test_launcher_utils {

void PrepareBrowserCommandLineForTests(CommandLine* command_line) {
  // Turn off tip loading for tests; see http://crbug.com/17725.
  command_line->AppendSwitch(switches::kDisableWebResources);

  // Turn off preconnects because they break the brittle python webserver;
  // see http://crbug.com/60035.
  command_line->AppendSwitch(switches::kDisablePreconnect);

  // Don't show the first run ui.
  command_line->AppendSwitch(switches::kNoFirstRun);

  // No default browser check, it would create an info-bar (if we are not the
  // default browser) that could conflicts with some tests expectations.
  command_line->AppendSwitch(switches::kNoDefaultBrowserCheck);

  // Enable warning level logging so that we can see when bad stuff happens.
  command_line->AppendSwitch(switches::kEnableLogging);
  command_line->AppendSwitchASCII(switches::kLoggingLevel, "1");  // warning

  // Disable safebrowsing autoupdate.
  command_line->AppendSwitch(switches::kSbDisableAutoUpdate);
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

bool OverrideGLImplementation(CommandLine* command_line,
                              const std::string& implementation_name) {
  if (command_line->HasSwitch(switches::kUseGL))
    return false;

  command_line->AppendSwitchASCII(switches::kUseGL, implementation_name);

  return true;
}

int GetTestTerminationTimeout(const std::string& test_name,
                              int default_timeout_ms) {
  int timeout_ms = default_timeout_ms;
  if (CommandLine::ForCurrentProcess()->HasSwitch(kTestTerminateTimeoutFlag)) {
    std::string timeout_str =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kTestTerminateTimeoutFlag);
    int timeout;
    if (base::StringToInt(timeout_str, &timeout)) {
      timeout_ms = std::max(timeout_ms, timeout);
    } else {
      LOG(ERROR) << "Invalid timeout (" << kTestTerminateTimeoutFlag << "): "
                 << timeout_str;
    }
  }

  // Make it possible for selected tests to request a longer timeout.
  // Generally tests should really avoid doing too much, and splitting
  // a test instead of using SLOW prefix is strongly preferred.
  if (test_name.find("SLOW_") != std::string::npos)
    timeout_ms *= kSlowTestTimeoutMultiplier;

  return timeout_ms;
}

}  // namespace test_launcher_utils
