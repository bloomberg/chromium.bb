// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/cloud_print_service.h"

#include <atlsecurity.h>
#include <iomanip>
#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "cloud_print/service/service_state.h"
#include "cloud_print/service/service_switches.h"
#include "cloud_print/service/win/chrome_launcher.h"
#include "cloud_print/service/win/resource.h"

namespace {

const wchar_t kServiceStateFileName[] = L"Service State";

const wchar_t kUserToRunService[] = L"NT AUTHORITY\\LocalService";

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

std::string GetOption(const std::string& name, const std::string& default,
                      bool secure) {
  std::cout << "Input \'" << name << "\'";
  if (!default.empty()) {
    std::cout << ", press [ENTER] to keep '";
    std::cout << default;
    std::cout << "'";
  }
  std::cout << ":";
  std::string tmp;

  if (secure) {
    DWORD saved_mode = 0;
    // Don't close.
    HANDLE stdin_handle = ::GetStdHandle(STD_INPUT_HANDLE);
    ::GetConsoleMode(stdin_handle, &saved_mode);
    ::SetConsoleMode(stdin_handle, saved_mode & ~ENABLE_ECHO_INPUT);
    std::getline(std::cin, tmp);
    ::SetConsoleMode(stdin_handle, saved_mode);
    std::cout << "\n";
  } else {
    std::getline(std::cin, tmp);
  }
  if (tmp.empty())
    return default;
  return tmp;
}

HRESULT WriteFileAsUser(const FilePath& path, const wchar_t* user,
                        const char* data, int size) {
    ATL::CAccessToken thread_token;
    if (!thread_token.OpenThreadToken(TOKEN_DUPLICATE | TOKEN_IMPERSONATE)) {
      LOG(ERROR) << "Failed to open thread token.";
      return HResultFromLastError();
    }

    ATL::CSid local_service;
    if (!local_service.LoadAccount(user)) {
      LOG(ERROR) << "Failed create SID.";
      return HResultFromLastError();
    }

    ATL::CAccessToken token;
    ATL::CTokenGroups group;
    group.Add(local_service, 0);

    const ATL::CTokenGroups empty_group;
    if (!thread_token.CreateRestrictedToken(&token, empty_group, group)) {
      LOG(ERROR) << "Failed to create restricted token for " << user << ".";
      return HResultFromLastError();
    }

    if (!token.Impersonate()) {
      LOG(ERROR) << "Failed to impersonate " << user << ".";
      return HResultFromLastError();
    }

    ATL::CAutoRevertImpersonation auto_revert(&token);
    if (file_util::WriteFile(path, data, size) != size) {
      LOG(ERROR) << "Failed to write file " << path.value() << ".";
      return HResultFromLastError();
    }
    return S_OK;
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

  HRESULT InstallService() {
    using namespace chrome_launcher_support;

    // TODO(vitalybuka): consider "lite" version if we don't want unregister
    // printers here.
    HRESULT hr = UninstallService();
    if (FAILED(hr))
      return hr;

    if (GetChromePathForInstallationLevel(SYSTEM_LEVEL_INSTALLATION).empty()) {
      LOG(ERROR) << "Found no Chrome installed for all users.";
      return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    hr = UpdateRegistryAppId(true);
    if (FAILED(hr))
      return hr;

    FilePath service_path;
    CHECK(PathService::Get(base::FILE_EXE, &service_path));
    CommandLine command_line(service_path);
    command_line.AppendSwitch(kServiceSwitch);
    command_line.AppendSwitchPath(kUserDataDirSwitch, user_data_dir_);

    ServiceHandle scm;
    hr = OpenServiceManager(&scm);
    if (FAILED(hr))
      return hr;

    ServiceHandle service(
        ::CreateService(
            scm, m_szServiceName, m_szServiceName, SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
            command_line.GetCommandLineString().c_str(), NULL, NULL, NULL,
            kUserToRunService, NULL));

    if (!service.IsValid())
      return HResultFromLastError();

    return S_OK;
  }

  HRESULT UninstallService() {
    if (!Uninstall())
      return E_FAIL;
    return UpdateRegistryAppId(false);
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

    user_data_dir_ = command_line.GetSwitchValuePath(kUserDataDirSwitch);
    if (command_line.HasSwitch(kStopSwitch))
      return StopService();

    if (command_line.HasSwitch(kUninstallSwitch))
      return UninstallService();

    if (command_line.HasSwitch(kInstallSwitch)) {
      if (!command_line.HasSwitch(kUserDataDirSwitch)) {
        InvalidUsage();
        return S_FALSE;
      }

      HRESULT hr = ValidateUserDataDir();
      if (FAILED(hr))
        return hr;

      hr = ProcessServiceState(command_line.HasSwitch(kQuietSwitch));
      if (FAILED(hr))
        return hr;

      hr = InstallService();
      if (SUCCEEDED(hr) && command_line.HasSwitch(kStartSwitch))
        return StartService();

      return hr;
    }

    if (command_line.HasSwitch(kStartSwitch))
      return StartService();

    if (command_line.HasSwitch(kServiceSwitch)) {
      *is_service = true;
      return S_OK;
    }

    if (command_line.HasSwitch(kConsoleSwitch)) {
      ::SetConsoleCtrlHandler(&ConsoleCtrlHandler, TRUE);
      HRESULT hr = Run();
      ::SetConsoleCtrlHandler(NULL, FALSE);
      return hr;
    }

    InvalidUsage();
    return S_FALSE;
  }

  HRESULT ValidateUserDataDir() {
    FilePath temp_file;
    const char some_data[] = "1234";
    if (!file_util::CreateTemporaryFileInDir(user_data_dir_, &temp_file))
      return E_FAIL;
    HRESULT hr = WriteFileAsUser(temp_file, kUserToRunService, some_data,
                                 sizeof(some_data));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to write user data. Make sure that account \'" <<
          kUserToRunService << "\'has full access to \'" <<
          user_data_dir_.value() << "\'.";
    }
    file_util::Delete(temp_file, false);
    return hr;
  }

  HRESULT ProcessServiceState(bool quiet) {
    FilePath file = user_data_dir_.Append(kServiceStateFileName);

    for (;;) {
      std::string contents;
      ServiceState service_state;

      bool is_valid = file_util::ReadFileToString(file, &contents) &&
                      service_state.FromString(contents);

      if (!quiet) {
        std::cout << file.value() << ":\n";
        std::cout << contents << "\n";
      }

      if (!is_valid)
        LOG(ERROR) << "Invalid file: " << file.value();

      if (quiet)
        return is_valid ? S_OK : HRESULT_FROM_WIN32(ERROR_FILE_INVALID);

      if (!contents.empty()) {
        std::cout << "Do you want to use this file [y/n]:";
        for (;;) {
          std::string input;
          std::getline(std::cin, input);
          StringToLowerASCII(&input);
          if (input == "y") {
            return S_OK;
          } else if (input == "n") {
            is_valid = false;
            break;
          }
        }
      }

      while (!is_valid) {
        std::string email = GetOption("email", service_state.email(), false);
        std::string password = GetOption("password", "", true);
        std::string proxy_id = GetOption("connector_id",
                                         service_state.proxy_id(), false);
        is_valid = service_state.Configure(email, password, proxy_id);
        if (is_valid) {
          std::string new_contents = service_state.ToString();
          if (new_contents != contents) {
            HRESULT hr = WriteFileAsUser(file, kUserToRunService,
                                         new_contents.c_str(),
                                         new_contents.size());
            if (FAILED(hr))
              return hr;
          }
        }
      }
    }

    return S_OK;
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
    chrome_.reset(new ChromeLauncher(user_data_dir_));
    return chrome_->Start() ? S_OK : E_FAIL;
  }

  void StopConnector() {
    chrome_->Stop();
    chrome_.reset();
  }

  static BOOL WINAPI ConsoleCtrlHandler(DWORD type);

  FilePath user_data_dir_;
  scoped_ptr<ChromeLauncher> chrome_;
};

CloudPrintServiceModule _AtlModule;

BOOL CloudPrintServiceModule::ConsoleCtrlHandler(DWORD type) {
  PostThreadMessage(_AtlModule.m_dwThreadID, WM_QUIT, 0, 0);
  return TRUE;
}

int main() {
  base::AtExitManager at_exit;
  return _AtlModule.WinMain(0);
}
