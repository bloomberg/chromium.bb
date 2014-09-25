// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/service_controller.h"

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_switches.h"
#include "cloud_print/common/win/cloud_print_utils.h"
#include "cloud_print/service/service_constants.h"
#include "cloud_print/service/service_switches.h"
#include "cloud_print/service/win/chrome_launcher.h"
#include "cloud_print/service/win/local_security_policy.h"
#include "cloud_print/service/win/service_utils.h"

namespace {

const wchar_t kServiceExeName[] = L"cloud_print_service.exe";

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
    return cloud_print::GetLastHResult();

  return S_OK;
}

HRESULT OpenService(const base::string16& name, DWORD access,
                    ServiceHandle* service) {
  if (!service)
    return E_POINTER;

  ServiceHandle scm;
  HRESULT hr = OpenServiceManager(&scm);
  if (FAILED(hr))
    return hr;

  service->Set(::OpenService(scm.Get(), name.c_str(), access));

  if (!service->IsValid())
    return cloud_print::GetLastHResult();

  return S_OK;
}

}  // namespace

ServiceController::ServiceController()
    : name_(cloud_print::LoadLocalString(IDS_SERVICE_NAME)),
      command_line_(CommandLine::NO_PROGRAM) {
}

ServiceController::~ServiceController() {
}

HRESULT ServiceController::StartService() {
  ServiceHandle service;
  HRESULT hr = OpenService(name_, SERVICE_START| SERVICE_QUERY_STATUS,
                           &service);
  if (FAILED(hr))
    return hr;
  if (!::StartService(service.Get(), 0, NULL))
    return cloud_print::GetLastHResult();
  SERVICE_STATUS status = {0};
  while (::QueryServiceStatus(service.Get(), &status) &&
          status.dwCurrentState == SERVICE_START_PENDING) {
    Sleep(100);
  }
  return S_OK;
}

HRESULT ServiceController::StopService() {
  ServiceHandle service;
  HRESULT hr = OpenService(name_, SERVICE_STOP | SERVICE_QUERY_STATUS,
                           &service);
  if (FAILED(hr))
    return hr;
  SERVICE_STATUS status = {0};
  if (!::ControlService(service.Get(), SERVICE_CONTROL_STOP, &status))
    return cloud_print::GetLastHResult();
  while (::QueryServiceStatus(service.Get(), &status) &&
          status.dwCurrentState > SERVICE_STOPPED) {
    Sleep(500);
    ::ControlService(service.Get(), SERVICE_CONTROL_STOP, &status);
  }
  return S_OK;
}

base::FilePath ServiceController::GetBinary() const {
  base::FilePath service_path;
  CHECK(PathService::Get(base::FILE_EXE, &service_path));
  return service_path.DirName().Append(base::FilePath(kServiceExeName));
}

HRESULT ServiceController::InstallConnectorService(
    const base::string16& user,
    const base::string16& password,
    const base::FilePath& user_data_dir,
    bool enable_logging) {
  return InstallService(user, password, true, kServiceSwitch, user_data_dir,
                        enable_logging);
}

HRESULT ServiceController::InstallCheckService(
    const base::string16& user,
    const base::string16& password,
    const base::FilePath& user_data_dir) {
  return InstallService(user, password, false, kRequirementsSwitch,
                        user_data_dir, true);
}

HRESULT ServiceController::InstallService(const base::string16& user,
                                          const base::string16& password,
                                          bool auto_start,
                                          const std::string& run_switch,
                                          const base::FilePath& user_data_dir,
                                          bool enable_logging) {
  // TODO(vitalybuka): consider "lite" version if we don't want unregister
  // printers here.
  HRESULT hr = UninstallService();
  if (FAILED(hr))
    return hr;

  hr = UpdateRegistryAppId(true);
  if (FAILED(hr))
    return hr;

  base::FilePath service_path = GetBinary();
  if (!base::PathExists(service_path))
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  CommandLine command_line(service_path);
  command_line.AppendSwitch(run_switch);
  if (!user_data_dir.empty())
    command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  if (enable_logging) {
    command_line.AppendSwitch(switches::kEnableLogging);
    command_line.AppendSwitchASCII(switches::kV, "1");
  }

  CopyChromeSwitchesFromCurrentProcess(&command_line);

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

  base::string16 display_name =
      cloud_print::LoadLocalString(IDS_SERVICE_DISPLAY_NAME);
  ServiceHandle service(
      ::CreateService(
          scm.Get(), name_.c_str(), display_name.c_str(), SERVICE_ALL_ACCESS,
          SERVICE_WIN32_OWN_PROCESS,
          auto_start ? SERVICE_AUTO_START : SERVICE_DEMAND_START,
          SERVICE_ERROR_NORMAL, command_line.GetCommandLineString().c_str(),
          NULL, NULL, NULL, user.empty() ? NULL : user.c_str(),
          password.empty() ? NULL : password.c_str()));

  if (!service.IsValid()) {
    LOG(ERROR) << "Failed to install service as " << user << ".";
    return cloud_print::GetLastHResult();
  }

  base::string16 description_string =
      cloud_print::LoadLocalString(IDS_SERVICE_DESCRIPTION);
  SERVICE_DESCRIPTION description = {0};
  description.lpDescription = const_cast<wchar_t*>(description_string.c_str());
  ::ChangeServiceConfig2(service.Get(), SERVICE_CONFIG_DESCRIPTION,
                         &description);

  return S_OK;
}

HRESULT ServiceController::UninstallService() {
  StopService();

  ServiceHandle service;
  OpenService(name_, SERVICE_STOP | DELETE, &service);
  HRESULT hr = S_FALSE;
  if (service.IsValid()) {
    if (!::DeleteService(service.Get())) {
      LOG(ERROR) << "Failed to uninstall service";
      hr = cloud_print::GetLastHResult();
    }
  }
  UpdateRegistryAppId(false);
  return hr;
}

HRESULT ServiceController::UpdateBinaryPath() {
  UpdateState();
  ServiceController::State origina_state = state();
  if (origina_state < ServiceController::STATE_STOPPED)
    return S_FALSE;

  ServiceHandle service;
  HRESULT hr = OpenService(name_, SERVICE_CHANGE_CONFIG, &service);
  if (FAILED(hr))
    return hr;

  base::FilePath service_path = GetBinary();
  if (!base::PathExists(service_path))
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

  command_line_.SetProgram(service_path);
  if (!::ChangeServiceConfig(service.Get(), SERVICE_NO_CHANGE,
                             SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
                             command_line_.GetCommandLineString().c_str(), NULL,
                             NULL, NULL, NULL, NULL, NULL)) {
    return cloud_print::GetLastHResult();
  }

  if (origina_state != ServiceController::STATE_RUNNING)
    return S_OK;

  hr = StopService();
  if (FAILED(hr))
    return hr;

  hr = StartService();
  if (FAILED(hr))
    return hr;

  return S_OK;
}

void ServiceController::UpdateState() {
  state_ = STATE_NOT_FOUND;
  user_.clear();
  is_logging_enabled_ = false;

  ServiceHandle service;
  HRESULT hr = OpenService(name_, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG,
                           &service);
  if (FAILED(hr))
    return;

  state_ = STATE_STOPPED;
  SERVICE_STATUS status = {0};
  if (::QueryServiceStatus(service.Get(), &status) &&
      status.dwCurrentState == SERVICE_RUNNING) {
    state_ = STATE_RUNNING;
  }

  DWORD config_size = 0;
  ::QueryServiceConfig(service.Get(), NULL, 0, &config_size);
  if (!config_size)
    return;

  std::vector<uint8> buffer(config_size, 0);
  QUERY_SERVICE_CONFIG* config =
      reinterpret_cast<QUERY_SERVICE_CONFIG*>(&buffer[0]);
  if (!::QueryServiceConfig(service.Get(), config, buffer.size(),
                            &config_size) ||
      config_size != buffer.size()) {
    return;
  }

  command_line_ = CommandLine::FromString(config->lpBinaryPathName);
  if (!command_line_.HasSwitch(kServiceSwitch)) {
    state_ = STATE_NOT_FOUND;
    return;
  }
  is_logging_enabled_ = command_line_.HasSwitch(switches::kEnableLogging);
  user_ = config->lpServiceStartName;
}

bool ServiceController::is_logging_enabled() const {
  return command_line_.HasSwitch(switches::kEnableLogging);
}
