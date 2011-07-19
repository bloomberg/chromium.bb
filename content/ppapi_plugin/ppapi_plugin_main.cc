// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/content_switches.h"
#include "content/common/main_function_params.h"
#include "content/ppapi_plugin/ppapi_thread.h"
#include "ppapi/proxy/proxy_module.h"

#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

#if defined(OS_WIN)
sandbox::TargetServices* g_target_services = NULL;
#else
void* g_target_services = 0;
#endif

// Main function for starting the PPAPI plugin process.
int PpapiPluginMain(const MainFunctionParams& parameters) {
  const CommandLine& command_line = parameters.command_line_;

#if defined(OS_WIN)
  g_target_services = parameters.sandbox_info_.TargetServices();
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

  MessageLoop main_message_loop(MessageLoop::TYPE_IO);
  base::PlatformThread::SetName("CrPPAPIMain");

  ChildProcess ppapi_process;
  ppapi_process.set_main_thread(new PpapiThread(false));  // Not a broker.

  pp::proxy::ProxyModule::GetInstance()->SetFlashCommandLineArgs(
      command_line.GetSwitchValueASCII(switches::kPpapiFlashArgs));

  main_message_loop.Run();
  return 0;
}
