// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/sandbox_policy.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_process.h"

#if defined(OS_MACOSX)
#include "chrome/common/chrome_application_mac.h"
#endif

// Mainline routine for running as the service process.
int ServiceProcessMain(const MainFunctionParams& parameters) {
  // If there is already a service process running, quit now.
  if (!ServiceProcessState::GetInstance()->Initialize())
    return 0;

  MessageLoopForUI main_message_loop;
  if (parameters.command_line_.HasSwitch(switches::kWaitForDebugger)) {
    base::debug::WaitForDebugger(60, true);
  }

#if defined(OS_MACOSX)
  chrome_application_mac::RegisterCrApp();
#endif

  base::PlatformThread::SetName("CrServiceMain");

#if defined(OS_WIN)
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services)
    sandbox::InitBrokerServices(broker_services);
#endif   // defined(OS_WIN)

  ServiceProcess service_process;
  if (!service_process.Initialize(&main_message_loop,
                                  parameters.command_line_)) {
    LOG(ERROR) << "Service process failed to initialize";
    return 0;
  }

  MessageLoop::current()->Run();
  service_process.Teardown();
  return 0;
}
