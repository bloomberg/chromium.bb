// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/debug/debugger.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/service_process.h"
#include "content/public/common/main_function_params.h"
#include "net/url_request/url_request.h"

// Mainline routine for running as the service process.
int ServiceProcessMain(const content::MainFunctionParams& parameters) {
  // Chrome disallows cookies by default. All code paths that want to use
  // cookies should go through the browser process.
  net::URLRequest::SetDefaultCookiePolicyToBlock();

  base::MessageLoopForUI main_message_loop;
  main_message_loop.set_thread_name("MainThread");
  if (parameters.command_line.HasSwitch(switches::kWaitForDebugger)) {
    base::debug::WaitForDebugger(60, true);
  }

  VLOG(1) << "Service process launched: "
          << parameters.command_line.GetCommandLineString();

  base::PlatformThread::SetName("CrServiceMain");
  base::StatisticsRecorder::Initialize();

  // If there is already a service process running, quit now.
  scoped_ptr<ServiceProcessState> state(new ServiceProcessState);
  if (!state->Initialize())
    return 0;

  ServiceProcess service_process;
  if (service_process.Initialize(&main_message_loop,
                                 parameters.command_line,
                                 state.release())) {
    base::MessageLoop::current()->Run();
  } else {
    LOG(ERROR) << "Service process failed to initialize";
  }
  service_process.Teardown();
  return 0;
}
