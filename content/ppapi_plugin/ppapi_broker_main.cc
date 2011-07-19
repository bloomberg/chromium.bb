// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/content_switches.h"
#include "content/common/main_function_params.h"
#include "content/ppapi_plugin/ppapi_thread.h"

// Main function for starting the PPAPI broker process.
int PpapiBrokerMain(const MainFunctionParams& parameters) {
  const CommandLine& command_line = parameters.command_line_;
  if (command_line.HasSwitch(switches::kPpapiStartupDialog)) {
    ChildProcess::WaitForDebugger("PpapiBroker");
  }

  MessageLoop main_message_loop(MessageLoop::TYPE_DEFAULT);
  base::PlatformThread::SetName("CrPPAPIBrokerMain");

  ChildProcess ppapi_broker_process;
  ppapi_broker_process.set_main_thread(new PpapiThread(true));  // Broker.

  main_message_loop.Run();
  return 0;
}
