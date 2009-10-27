// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/utility/utility_thread.h"

#if defined(OS_WIN)
#include "chrome/common/sandbox_init_wrapper.h"
#include "sandbox/src/sandbox.h"
#endif

// Mainline routine for running as the utility process.
int UtilityMain(const MainFunctionParams& parameters) {
  // The main message loop of the utility process.
  MessageLoop main_message_loop;
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_UtilityMain").c_str());

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

  ChildProcess utility_process;
  utility_process.set_main_thread(new UtilityThread());
#if defined(OS_WIN)
  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();
  if (!target_services)
    return false;

  target_services->LowerToken();
#endif

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::string lang = command_line->GetSwitchValueASCII(switches::kLang);
  if (!lang.empty())
    extension_l10n_util::SetProcessLocale(lang);

  MessageLoop::current()->Run();

  return 0;
}
