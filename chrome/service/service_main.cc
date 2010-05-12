// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/main_function_params.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_process.h"

// Mainline routine for running as the service process.
int ServiceProcessMain(const MainFunctionParams& parameters) {
  MessageLoopForUI main_message_loop;
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_ServiceMain").c_str());

  ServiceProcess service_process;
  service_process.Initialize();
  if (parameters.command_line_.HasSwitch(switches::kEnableCloudPrintProxy)) {
    std::string lsid =
        parameters.command_line_.GetSwitchValueASCII(
            switches::kServiceAccountLsid);
    std::string proxy_id =
        parameters.command_line_.GetSwitchValueASCII(
            switches::kCloudPrintProxyId);
    service_process.cloud_print_proxy()->EnableForUser(lsid, proxy_id);
  }
  MessageLoop::current()->Run();
  service_process.Teardown();

  return 0;
}

