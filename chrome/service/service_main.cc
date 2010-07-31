// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/sandbox_policy.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_process.h"

#if defined(ENABLE_REMOTING)
#include "remoting/host/json_host_config.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"

// This method is called as a signal that the Chromoting Host Process is
// shutting down because of failure or a request made by the user.
// We'll then post a task to |message_loop| to stop the chromoting host
// context to finish the final cleanup.
static void OnChromotingHostShutdown(
    MessageLoop* message_loop, remoting::ChromotingHostContext* context) {
  message_loop->PostTask(FROM_HERE,
      NewRunnableMethod(context, &remoting::ChromotingHostContext::Stop));
}
#endif

// Mainline routine for running as the service process.
int ServiceProcessMain(const MainFunctionParams& parameters) {
  MessageLoopForUI main_message_loop;
  PlatformThread::SetName("CrServiceMain");

#if defined(OS_WIN)
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services)
    sandbox::InitBrokerServices(broker_services);
#endif   // defined(OS_WIN)

  ServiceProcess service_process;
  service_process.Initialize();

  // Enable Cloud Print if needed.
  if (parameters.command_line_.HasSwitch(switches::kEnableCloudPrintProxy)) {
    std::string lsid =
        parameters.command_line_.GetSwitchValueASCII(
            switches::kServiceAccountLsid);
    service_process.GetCloudPrintProxy()->EnableForUser(lsid);
  }

#if defined(ENABLE_REMOTING)
  // Enable Chromoting Host if needed.
  // TODO(hclam): Merge this config file with Cloud Printing.
  // TODO(hclam): There is only start but not stop of the chromoting host
  // process.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  FilePath chromoting_config_path =
      user_data_dir.Append(FILE_PATH_LITERAL(".ChromotingConfig.json"));
  scoped_refptr<remoting::JsonHostConfig> chromoting_config;
  scoped_ptr<remoting::ChromotingHostContext> chromoting_context;
  scoped_refptr<remoting::ChromotingHost> chromoting_host;
  if (parameters.command_line_.HasSwitch(switches::kEnableChromoting)) {
    chromoting_config = new remoting::JsonHostConfig(
        chromoting_config_path,
        service_process.file_thread()->message_loop_proxy());
    if (!chromoting_config->Read()) {
      LOG(ERROR) << "Failed to read chromoting config file.";
    } else {
      chromoting_context.reset(new remoting::ChromotingHostContext());

      // Create the Chromoting Host Process with the context and config.
      chromoting_host = service_process.CreateChromotingHost(
          chromoting_context.get(), chromoting_config);

      // And start the context and the host process.
      chromoting_context->Start();

      // When ChromotingHost is shutdown because of failure or a request that
      // we made. ShutdownChromotingTask() is calls.
      // ShutdownChromotingTask() will then post a task to
      // |main_message_loop| to shutdown the chromoting context.
      chromoting_host->Start(
          NewRunnableFunction(&OnChromotingHostShutdown,
                              &main_message_loop, chromoting_context.get()));
    }
  }
#endif

  MessageLoop::current()->Run();
  service_process.Teardown();

  return 0;
}

