// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_pref_store.h"
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

  // TODO(sanjeevr): The interface to start individual services such as the
  // cloud print proxy needs to change from a command-line interface to an
  // IPC interface. There will be eventually only one service process to handle
  // requests from multiple Chrome browser profiles. The path of the user data
  // directory will be passed in to the command and there will be one instance
  // of services such as the cloud print proxy per requesting profile.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  FilePath pref_path = user_data_dir.Append(chrome::kServiceStateFileName);
  scoped_ptr<JsonPrefStore> service_prefs(
      new JsonPrefStore(
          pref_path,
          service_process.file_thread()->message_loop_proxy()));
  service_prefs->ReadPrefs();

  if (parameters.command_line_.HasSwitch(switches::kEnableCloudPrintProxy)) {
    std::string lsid =
        parameters.command_line_.GetSwitchValueASCII(
            switches::kServiceAccountLsid);
    CloudPrintProxy* cloud_print_proxy =
        service_process.CreateCloudPrintProxy(service_prefs.get());
    cloud_print_proxy->EnableForUser(lsid);
  }
  MessageLoop::current()->Run();
  service_prefs->WritePrefs();
  service_process.Teardown();

  return 0;
}

