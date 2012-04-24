// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/ppapi_plugin/ppapi_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "ppapi/proxy/proxy_module.h"

#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

#if defined(OS_LINUX)
#include "content/public/common/sandbox_init.h"
#endif

#if defined(OS_WIN)
sandbox::TargetServices* g_target_services = NULL;
#else
void* g_target_services = 0;
#endif

// Main function for starting the PPAPI plugin process.
int PpapiPluginMain(const content::MainFunctionParams& parameters) {
  const CommandLine& command_line = parameters.command_line;

#if defined(OS_WIN)
  g_target_services = parameters.sandbox_info->target_services;
#endif

  // If |g_target_services| is not null this process is sandboxed. One side
  // effect is that we can't pop dialogs like ChildProcess::WaitForDebugger()
  // does.
  if (command_line.HasSwitch(switches::kPpapiStartupDialog)) {
    if (g_target_services)
      base::debug::WaitForDebugger(2*60, false);
    else
      ChildProcess::WaitForDebugger("Ppapi");
  }

  MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrPPAPIMain");

#if defined(OS_LINUX)
  content::InitializeSandbox();
#endif

  ChildProcess ppapi_process;
  ppapi_process.set_main_thread(
      new PpapiThread(parameters.command_line, false));  // Not a broker.

  main_message_loop.Run();
  return 0;
}
