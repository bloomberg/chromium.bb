// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/service_controller.h"

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/scoped_handle.h"
#include "cloud_print/service/service_switches.h"
#include "cloud_print/service/win/local_security_policy.h"

namespace {

HRESULT HResultFromLastError() {
  return GetLastError() ? HRESULT_FROM_WIN32(GetLastError()) : E_FAIL;
}

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

typedef base::win::GenericScopedHandle<
    ServiceHandleTraits, base::win::DummyVerifierTraits> ServiceHandle;

HRESULT OpenServiceManager(ServiceHandle* service_manager) {
  if (!service_manager)
    return E_POINTER;

  service_manager->Set(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
  if (!service_manager->IsValid())
    return HResultFromLastError();

  return S_OK;
}

HRESULT OpenService(const string16& name, DWORD access,
                    ServiceHandle* service) {
  if (!service)
    return E_POINTER;

  ServiceHandle scm;
  HRESULT hr = OpenServiceManager(&scm);
  if (FAILED(hr))
    return hr;

  service->Set(::OpenService(scm, name.c_str(), access));

  if (!service->IsValid())
    return HResultFromLastError();

  return S_OK;
}

}  // namespace

ServiceController::ServiceController(const string16& name)
    : name_(name) {
}

ServiceController::~ServiceController() {
}

HRESULT ServiceController::StartService() {
  ServiceHandle service;
  HRESULT hr = OpenService(name_, SERVICE_START, &service);
  if (FAILED(hr))
    return hr;
  if (!::StartService(service, 0, NULL))
    return HResultFromLastError();
  return S_OK;
}

HRESULT ServiceController::StopService() {
  ServiceHandle service;
  HRESULT hr = OpenService(name_, SERVICE_STOP | SERVICE_QUERY_STATUS,
                           &service);
  if (FAILED(hr))
    return hr;
  SERVICE_STATUS status = {0};
  if (!::ControlService(service, SERVICE_CONTROL_STOP, &status))
    return HResultFromLastError();
  while (::QueryServiceStatus(service, &status) &&
          status.dwCurrentState > SERVICE_STOPPED) {
    Sleep(500);
    ::ControlService(service, SERVICE_CONTROL_STOP, &status);
  }
  return S_OK;
}

HRESULT ServiceController::InstallService(const string16& user,
                                          const string16& password,
                                          const std::string& run_switch,
                                          const base::FilePath& user_data_dir,
                                          bool auto_start) {
  // TODO(vitalybuka): consider "lite" version if we don't want unregister
  // printers here.
  HRESULT hr = UninstallService();
  if (FAILED(hr))
    return hr;

  hr = UpdateRegistryAppId(true);
  if (FAILED(hr))
    return hr;

  base::FilePath service_path;
  CHECK(PathService::Get(base::FILE_EXE, &service_path));
  CommandLine command_line(service_path);
  command_line.AppendSwitch(run_switch);
  command_line.AppendSwitchPath(kUserDataDirSwitch, user_data_dir);

  LocalSecurityPolicy local_security_policy;
  if (local_security_policy.Open()) {
    if (!local_security_policy.IsPrivilegeSet(user, kSeServiceLogonRight)) {
      LOG(WARNING) << "Setting " << kSeServiceLogonRight << " for " << user;
      if (!local_security_policy.SetPrivilege(user, kSeServiceLogonRight)) {
        LOG(ERROR) << "Failed to set" << kSeServiceLogonRight;
        LOG(ERROR) << "Make sure you can run the service as " << user << ".";
      }
    }
  } else {
    LOG(ERROR) << "Failed to open security policy.";
  }

  ServiceHandle scm;
  hr = OpenServiceManager(&scm);
  if (FAILED(hr))
    return hr;

  ServiceHandle service(
      ::CreateService(
          scm, name_.c_str(), name_.c_str(), SERVICE_ALL_ACCESS,
          SERVICE_WIN32_OWN_PROCESS,
          auto_start ? SERVICE_AUTO_START : SERVICE_DEMAND_START,
          SERVICE_ERROR_NORMAL, command_line.GetCommandLineString().c_str(),
          NULL, NULL, NULL, user.empty() ? NULL : user.c_str(),
          password.empty() ? NULL : password.c_str()));

  if (!service.IsValid()) {
    LOG(ERROR) << "Failed to install service as " << user << ".";
    return HResultFromLastError();
  }

  return S_OK;
}

HRESULT ServiceController::UninstallService() {
  StopService();

  ServiceHandle service;
  OpenService(name_, SERVICE_STOP | DELETE, &service);
  HRESULT hr = S_FALSE;
  if (service) {
    if (!::DeleteService(service)) {
      LOG(ERROR) << "Failed to uninstall service";
      hr = HResultFromLastError();
    }
  }
  UpdateRegistryAppId(false);
  return hr;
}

