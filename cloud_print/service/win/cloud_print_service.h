// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_CLOUD_PRINT_SERVICE_H_
#define CLOUD_PRINT_SERVICE_CLOUD_PRINT_SERVICE_H_

#include <atlbase.h>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "cloud_print/service/win/resource.h"

class ChromeLauncher;
class CommandLine;
class ServiceController;

class CloudPrintService
    : public ATL::CAtlServiceModuleT<CloudPrintService, IDS_SERVICENAME> {
 public:
  typedef ATL::CAtlServiceModuleT<CloudPrintService, IDS_SERVICENAME> Base;

  CloudPrintService();
  ~CloudPrintService();

  // CAtlServiceModuleT ovverides.
  static wchar_t* GetAppIdT();
  HRESULT InitializeSecurity();
  bool ParseCommandLine(LPCTSTR lpCmdLine, HRESULT* pnRetCode);
  HRESULT PreMessageLoop(int nShowCmd);
  HRESULT PostMessageLoop();

 private:
  HRESULT ParseCommandLine(const CommandLine& command_line, bool* is_service);
  void SelectWindowsAccount(string16* run_as_user, string16* run_as_password);
  HRESULT SetupServiceState();
  HRESULT CheckRequirements();
  HRESULT StartConnector();
  void StopConnector();
  static BOOL WINAPI ConsoleCtrlHandler(DWORD type);

  bool check_requirements_;
  base::FilePath user_data_dir_;
  scoped_ptr<ChromeLauncher> chrome_;
  scoped_ptr<ServiceController> controller_;
};

extern CloudPrintService _AtlModule;

#endif  // CLOUD_PRINT_SERVICE_CLOUD_PRINT_SERVICE_H_

