// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/ppapi_plugin/ppapi_thread.h"

// Main function for starting the PPAPI plugin process.
int PpapiPluginMain(const MainFunctionParams& parameters) {
#if defined(OS_LINUX)
  // On Linux we exec ourselves from /proc/self/exe, but that makes the
  // process name that shows up in "ps" etc. for this process show as
  // "exe" instead of "chrome" or something reasonable. Try to fix it.
  CommandLine::SetProcTitle();
#endif

  const CommandLine& command_line = parameters.command_line_;
  if (command_line.HasSwitch(switches::kPpapiStartupDialog)) {
    ChildProcess::WaitForDebugger(L"Ppapi");
  }

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  PlatformThread::SetName("CrPPAPIMain");

  ChildProcess ppapi_process;
  ppapi_process.set_main_thread(new PpapiThread());

  main_message_loop.Run();
  return 0;
}
