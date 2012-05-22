// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/cloud_print_service.h"

#include <iomanip>
#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/win/scoped_handle.h"
#include "cloud_print/service/win/resource.h"

namespace {

const char kInstallSwitch[] = "install";
const char kUninstallSwitch[] = "uninstall";
const char kStartSwitch[] = "start";
const char kStopSwitch[] = "stop";

const char kServiceSwitch[] = "service";

const char kUserDataDirSwitch[] = "user-data-dir";
const char kQuietSwitch[] = "quiet";

// The traits class for Windows Service.
class ServiceHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  // Closes the handle.
  static bool CloseHandle(Handle handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  static bool IsHandleValid(Handle handle) {
    return handle != NULL;
  }

  static Handle NullHandle() {
    return NULL;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceHandleTraits);
};

typedef base::win::GenericScopedHandle<ServiceHandleTraits> ServiceHandle;

HRESULT HResultFromLastError() {
  HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
  // Something already failed if function called.
  if (SUCCEEDED(hr))
    hr = E_FAIL;
  return hr;
}

void InvalidUsage() {
  FilePath service_path;
  CHECK(PathService::Get(base::FILE_EXE, &service_path));

  std::cout << "Usage: ";
  std::cout << service_path.BaseName().value();
  std::cout << " [";
    std::cout << "[";
      std::cout << "[";
        std::cout << " -" << kInstallSwitch;
        std::cout << " -" << kUserDataDirSwitch << "=DIRECTORY";
        std::cout << " [ -" << kQuietSwitch << " ]";
      std::cout << "]";
    std::cout << "]";
    std::cout << " | -" << kUninstallSwitch;
    std::cout << " | -" << kStartSwitch;
    std::cout << " | -" << kStopSwitch;
  std::cout << " ]\n";
  std::cout << "Manages cloud print windows service.\n\n";

  static const struct {
    const char* name;
    const char* description;
  } kSwitchHelp[] = {
    { kInstallSwitch, "Installs cloud print as service." },
    { kUserDataDirSwitch, "User data directory with \"Service State\" file." },
    { kQuietSwitch, "Fails without questions if something wrong." },
    { kUninstallSwitch, "Uninstalls service." },
    { kStartSwitch, "Starts service. May be combined with installation." },
    { kStopSwitch, "Stops service." },
  };

  for (size_t i = 0; i < arraysize(kSwitchHelp); ++i) {
    std::cout << std::setiosflags(std::ios::left);
    std::cout << "  -" << std::setw(15) << kSwitchHelp[i].name;
    std::cout << kSwitchHelp[i].description << "\n";
  }
  std::cout << "\n";
}

}  // namespace

class CloudPrintServiceModule
    : public ATL::CAtlServiceModuleT<CloudPrintServiceModule, IDS_SERVICENAME> {
 public:
  typedef ATL::CAtlServiceModuleT<CloudPrintServiceModule,
                                  IDS_SERVICENAME> Base;

  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CLOUDPRINTSERVICE,
                                    "{8013FB7C-2E3E-4992-B8BD-05C0C4AB0627}")

  HRESULT InitializeSecurity() {
    // TODO(gene): Check if we need to call CoInitializeSecurity and provide
    // the appropriate security settings for service.
    return S_OK;
  }

  HRESULT Install(const FilePath& user_data_dir) {
    // TODO(vitalybuka): consider "lite" version if we don't want unregister
    // printers here.
    if (!Uninstall())
      return E_FAIL;

    FilePath service_path;
    CHECK(PathService::Get(base::FILE_EXE, &service_path));
    CommandLine command_line(service_path);
    command_line.AppendSwitch(kServiceSwitch);
    command_line.AppendSwitchPath(kUserDataDirSwitch, user_data_dir);

    ServiceHandle scm;
    HRESULT hr = OpenServiceManager(&scm);
    if (FAILED(hr))
      return hr;

    ServiceHandle service(
        ::CreateService(
            scm, m_szServiceName, m_szServiceName, SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
            command_line.GetCommandLineString().c_str(), NULL, NULL, NULL,
            L"NT AUTHORITY\\LocalService", NULL));

    if (!service.IsValid())
      return HResultFromLastError();

    return S_OK;
  }

  bool ParseCommandLine(LPCTSTR lpCmdLine, HRESULT* pnRetCode) {
    CHECK(pnRetCode);
    CommandLine command_line(CommandLine::NO_PROGRAM);
    command_line.ParseFromString(lpCmdLine);
    bool is_service = false;
    *pnRetCode = ParseCommandLine(command_line, &is_service);
    if (FAILED(*pnRetCode)) {
      LOG(ERROR) << "Operation failed. 0x" << std::setw(8) <<
          std::setbase(16) << *pnRetCode;
    }
    return is_service;
  }

  HRESULT PreMessageLoop(int nShowCmd) {
    HRESULT hr = Base::PreMessageLoop(nShowCmd);
    if (FAILED(hr))
      return hr;

    hr = StartConnector();
    if (FAILED(hr))
      return hr;

    LogEvent(_T("Service started/resumed"));
    SetServiceStatus(SERVICE_RUNNING);
    return hr;
  }

  HRESULT PostMessageLoop() {
    StopConnector();
    return Base::PostMessageLoop();
  }

 private:
  HRESULT ParseCommandLine(const CommandLine& command_line, bool* is_service) {
    if (!is_service)
      return E_INVALIDARG;
    *is_service = false;

    if (command_line.HasSwitch(kStopSwitch))
      return StopService();

    if (command_line.HasSwitch(kUninstallSwitch))
      return Uninstall() ? S_OK : E_FAIL;

    if (command_line.HasSwitch(kInstallSwitch)) {
      if (!command_line.HasSwitch(kUserDataDirSwitch)) {
        InvalidUsage();
        return S_FALSE;
      }

      FilePath data_dir = command_line.GetSwitchValuePath(kUserDataDirSwitch);
      HRESULT hr = Install(data_dir);
      if (SUCCEEDED(hr) && command_line.HasSwitch(kStartSwitch))
        return StartService();

      return hr;
    }

    if (command_line.HasSwitch(kStartSwitch))
      return StartService();

    if (command_line.HasSwitch(kServiceSwitch)) {
      user_data_dir_ = command_line.GetSwitchValuePath(kUserDataDirSwitch);
      *is_service = true;
      return S_OK;
    }

    InvalidUsage();
    return S_FALSE;
  }

  HRESULT OpenServiceManager(ServiceHandle* service_manager) {
    if (!service_manager)
      return E_POINTER;

    service_manager->Set(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!service_manager->IsValid())
      return HResultFromLastError();

    return S_OK;
  }

  HRESULT OpenService(DWORD access, ServiceHandle* service) {
    if (!service)
      return E_POINTER;

    ServiceHandle scm;
    HRESULT hr = OpenServiceManager(&scm);
    if (FAILED(hr))
      return hr;

    service->Set(::OpenService(scm, m_szServiceName, access));

    if (!service->IsValid())
      return HResultFromLastError();

    return S_OK;
  }

  HRESULT StartService() {
    ServiceHandle service;
    HRESULT hr = OpenService(SERVICE_START, &service);
    if (FAILED(hr))
      return hr;
    if (!::StartService(service, 0, NULL))
      return HResultFromLastError();
    return S_OK;
  }

  HRESULT StopService() {
    ServiceHandle service;
    HRESULT hr = OpenService(SERVICE_STOP, &service);
    if (FAILED(hr))
      return hr;
    SERVICE_STATUS status = {0};
    if (!::ControlService(service, SERVICE_CONTROL_STOP, &status))
      return HResultFromLastError();
    return S_OK;
  }

  HRESULT StartConnector() {
    return S_OK;
  }

  void StopConnector() {
  }

  FilePath user_data_dir_;
};

CloudPrintServiceModule _AtlModule;

int main() {
  base::AtExitManager at_exit;
  return _AtlModule.WinMain(0);
}
