// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "base/message_loop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/service_process.h"

#if defined(OS_WIN)
#include "chrome/common/sandbox_policy.h"
#elif defined(OS_MACOSX)
#include "chrome/common/chrome_application_mac.h"
#endif  // defined(OS_WIN)

// Mainline routine for running as the service process.
int ServiceProcessMain(const MainFunctionParams& parameters) {
  MessageLoopForUI main_message_loop;
  main_message_loop.set_thread_name("MainThread");
  if (parameters.command_line_.HasSwitch(switches::kWaitForDebugger)) {
    base::debug::WaitForDebugger(60, true);
  }

  VLOG(1) << "Service process launched: "
          << parameters.command_line_.command_line_string();

#if defined(OS_MACOSX)
  chrome_application_mac::RegisterCrApp();
#endif

  base::PlatformThread::SetName("CrServiceMain");

  // If there is already a service process running, quit now.
  if (!ServiceProcessState::GetInstance()->Initialize())
    return 0;

#if defined(OS_WIN)
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services)
    sandbox::InitBrokerServices(broker_services);
#elif defined(OS_MACOSX)
  chrome_application_mac::RegisterCrApp();
#endif  // defined(OS_WIN)

  ServiceProcess service_process;
  if (service_process.Initialize(&main_message_loop,
                                 parameters.command_line_)) {
    MessageLoop::current()->Run();
  } else {
    LOG(ERROR) << "Service process failed to initialize";
  }
  service_process.Teardown();
  return 0;
}
