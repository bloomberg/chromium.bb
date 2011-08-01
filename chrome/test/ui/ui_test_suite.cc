// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test_suite.h"

#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/test/automation/proxy_launcher.h"

UITestSuite::UITestSuite(int argc, char** argv)
    : ChromeTestSuite(argc, argv) {
#if defined(OS_WIN)
  crash_service_ = NULL;
#endif
}

void UITestSuite::Initialize() {
  ChromeTestSuite::Initialize();

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  ProxyLauncher::set_in_process_renderer(
      parsed_command_line.HasSwitch(switches::kSingleProcess));
  ProxyLauncher::set_no_sandbox(
      parsed_command_line.HasSwitch(switches::kNoSandbox));
  ProxyLauncher::set_full_memory_dump(
      parsed_command_line.HasSwitch(switches::kFullMemoryCrashReport));
  ProxyLauncher::set_dump_histograms_on_exit(
      parsed_command_line.HasSwitch(switches::kDumpHistogramsOnExit));
  ProxyLauncher::set_enable_dcheck(
      parsed_command_line.HasSwitch(switches::kEnableDCHECK));
  ProxyLauncher::set_silent_dump_on_dcheck(
      parsed_command_line.HasSwitch(switches::kSilentDumpOnDCHECK));
  ProxyLauncher::set_disable_breakpad(
      parsed_command_line.HasSwitch(switches::kDisableBreakpad));

  std::string js_flags =
    parsed_command_line.GetSwitchValueASCII(switches::kJavaScriptFlags);
  if (!js_flags.empty())
    ProxyLauncher::set_js_flags(js_flags);
  std::string log_level =
    parsed_command_line.GetSwitchValueASCII(switches::kLoggingLevel);
  if (!log_level.empty())
    ProxyLauncher::set_log_level(log_level);

#if defined(OS_WIN)
  LoadCrashService();
#endif
}

void UITestSuite::Shutdown() {
#if defined(OS_WIN)
  if (crash_service_)
    base::KillProcess(crash_service_, 0, false);
#endif

  ChromeTestSuite::Shutdown();
}

#if defined(OS_WIN)
void UITestSuite::LoadCrashService() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar(env_vars::kHeadless))
    return;

  if (base::GetProcessCount(L"crash_service.exe", NULL))
    return;

  FilePath exe_dir;
  if (!PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get path to DIR_EXE, "
               << "not starting crash_service.exe!";
    return;
  }

  FilePath crash_service = exe_dir.Append(L"crash_service.exe");
  if (!base::LaunchProcess(crash_service.value(), base::LaunchOptions(),
                           &crash_service_)) {
    LOG(ERROR) << "Couldn't start crash_service.exe, so this ui_tests run "
               << "won't tell you if any test crashes!";
    return;
  }
}
#endif
