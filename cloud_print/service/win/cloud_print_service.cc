// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/cloud_print_service.h"

#define SECURITY_WIN32
#include <security.h>

#include <iomanip>
#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "cloud_print/service/service_state.h"
#include "cloud_print/service/service_switches.h"
#include "cloud_print/service/win/chrome_launcher.h"
#include "cloud_print/service/win/local_security_policy.h"
#include "cloud_print/service/win/resource.h"
#include "printing/backend/print_backend.h"

namespace {

const char kChromeIsNotAvalible[] = "\nChrome is not available\n";
const char kChromeIsAvalible[] = "\nChrome is available\n";
const wchar_t kRequirementsFileName[] = L"cloud_print_service_requirements.txt";
const wchar_t kServiceStateFileName[] = L"Service State";

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
  base::FilePath service_path;
  CHECK(PathService::Get(base::FILE_EXE, &service_path));

  std::cout << "Usage: ";
  std::cout << service_path.BaseName().value();
  std::cout << " [";
    std::cout << "[";
      std::cout << "[";
        std::cout << " -" << kInstallSwitch;
        std::cout << " -" << kUserDataDirSwitch << "=DIRECTORY";
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
    { kUninstallSwitch, "Uninstalls service." },
    { kStartSwitch, "Starts service. May be combined with installation." },
    { kStopSwitch, "Stops service." },
  };

  for (size_t i = 0; i < arraysize(kSwitchHelp); ++i) {
    std::cout << std::setiosflags(std::ios::left);
    std::cout << "  -" << std::setw(16) << kSwitchHelp[i].name;
    std::cout << kSwitchHelp[i].description << "\n";
  }
  std::cout << "\n";
}

std::string GetOption(const std::string& name, const std::string& default,
                      bool secure) {
  std::cout << name;
  if (!default.empty()) {
    std::cout << ", press [ENTER] to keep '";
    std::cout << default;
    std::cout << "'";
  }
  std::cout << ": ";
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

bool AskUser(const std::string& request) {
  for (;;) {
    std::cout << request << " [Y/n]:";
    std::string input;
    std::getline(std::cin, input);
    StringToLowerASCII(&input);
    if (input.empty() || input == "y") {
      return true;
    } else if (input == "n") {
      return false;
    }
  }
}

string16 GetCurrentUserName() {
  ULONG size = 0;
  string16 result;
  ::GetUserNameEx(::NameSamCompatible, NULL, &size);
  result.resize(size);
  if (result.empty())
    return result;
  if (!::GetUserNameEx(::NameSamCompatible, &result[0], &size))
    result.clear();
  result.resize(size);
  return result;
}

}  // namespace

class CloudPrintServiceModule
    : public ATL::CAtlServiceModuleT<CloudPrintServiceModule, IDS_SERVICENAME> {
 public:
  typedef ATL::CAtlServiceModuleT<CloudPrintServiceModule,
                                  IDS_SERVICENAME> Base;

  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CLOUDPRINTSERVICE,
                                    "{8013FB7C-2E3E-4992-B8BD-05C0C4AB0627}")

  CloudPrintServiceModule() : check_requirements_(false) {
  }

  HRESULT InitializeSecurity() {
    // TODO(gene): Check if we need to call CoInitializeSecurity and provide
    // the appropriate security settings for service.
    return S_OK;
  }

  HRESULT InstallService(const string16& user,
                         const string16& password,
                         const char* run_switch) {
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
    command_line.AppendSwitchPath(kUserDataDirSwitch, user_data_dir_);

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
            scm, m_szServiceName, m_szServiceName, SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
            command_line.GetCommandLineString().c_str(), NULL, NULL, NULL,
            user.empty() ? NULL : user.c_str(),
            password.empty() ? NULL : password.c_str()));

    if (!service.IsValid()) {
      LOG(ERROR) << "Failed to install service as " << user << ".";
      return HResultFromLastError();
    }

    return S_OK;
  }

  HRESULT UninstallService() {
    StopService();
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

    if (check_requirements_) {
      hr = CheckRequirements();
      if (FAILED(hr))
        return hr;
      // Don't run message loop and stop service.
      return S_FALSE;
    } else {
      hr = StartConnector();
      if (FAILED(hr))
        return hr;
    }

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

      string16 run_as_user;
      string16 run_as_password;
      SelectWindowsAccount(&run_as_user, &run_as_password);

      HRESULT hr = SetupServiceState();
      if (FAILED(hr))
        return hr;

      hr = InstallService(run_as_user.c_str(), run_as_password.c_str(),
                          kServiceSwitch);
      if (SUCCEEDED(hr) && command_line.HasSwitch(kStartSwitch))
        return StartService();

      return hr;
    }

    if (command_line.HasSwitch(kStartSwitch))
      return StartService();

    if (command_line.HasSwitch(kServiceSwitch) ||
        command_line.HasSwitch(kRequirementsSwitch)) {
      *is_service = true;
      check_requirements_ = command_line.HasSwitch(kRequirementsSwitch);
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

  void SelectWindowsAccount(string16* run_as_user,
                            string16* run_as_password) {
    *run_as_user = GetCurrentUserName();
    for (;;) {
      std::cout << "\nPlease provide Windows account to run service.\n";
      *run_as_user = ASCIIToWide(GetOption("Account as DOMAIN\\USERNAME",
                                           WideToASCII(*run_as_user), false));
      *run_as_password = ASCIIToWide(GetOption("Password", "", true));

      base::FilePath requirements_filename(user_data_dir_);
      requirements_filename =
          requirements_filename.Append(kRequirementsFileName);

      file_util::Delete(requirements_filename, false);
      if (file_util::PathExists(requirements_filename)) {
        LOG(ERROR) << "Unable to delete " <<
            requirements_filename.value() << ".";
        continue;
      }
      if (FAILED(InstallService(run_as_user->c_str(),
                                run_as_password->c_str(),
                                kRequirementsSwitch))) {
        continue;
      }
      bool service_started = SUCCEEDED(StartService());
      UninstallService();
      if (!service_started) {
        LOG(ERROR) << "Failed to start service as " << *run_as_user << ".";
        continue;
      }
      std::string printers;
      if (!file_util::PathExists(requirements_filename) ||
          !file_util::ReadFileToString(requirements_filename, &printers)) {
        LOG(ERROR) << "Service can't create " << requirements_filename.value();
        continue;
      }

      if (EndsWith(printers, kChromeIsNotAvalible, true)) {
        LOG(ERROR) << kChromeIsNotAvalible << " for " << *run_as_user << ".";
        continue;
      }

      std::cout << "\nService requirements check result: \n";
      std::cout << printers << "\n";
      file_util::Delete(requirements_filename, false);

      if (AskUser("Do you want to use " + WideToASCII(*run_as_user) + "?"))
        return;
    }
  }

  HRESULT SetupServiceState() {
    base::FilePath file = user_data_dir_.Append(kServiceStateFileName);

    for (;;) {
      std::string contents;
      ServiceState service_state;

      bool is_valid = file_util::ReadFileToString(file, &contents) &&
                      service_state.FromString(contents);

      std::cout << "\nFile '" << file.value() << "' content:\n";
      std::cout << contents << "\n";

      if (!is_valid)
        LOG(ERROR) << "Invalid file: " << file.value();

      if (!contents.empty()) {
        if (AskUser("Do you want to use '" + WideToASCII(file.value()) + "'")) {
          return S_OK;
        } else {
          is_valid = false;
        }
      }

      while (!is_valid) {
        std::cout << "\nPlease provide Cloud Print Settings.\n";
        std::string email = GetOption("email", service_state.email(), false);
        std::string password = GetOption("password", "", true);
        std::string proxy_id = service_state.proxy_id();
        if (proxy_id.empty())
          proxy_id = base::GenerateGUID();
        proxy_id = GetOption("connector_id", proxy_id, false);
        is_valid = service_state.Configure(email, password, proxy_id);
        if (is_valid) {
          std::string new_contents = service_state.ToString();
          if (new_contents != contents) {
            size_t  written = file_util::WriteFile(file, new_contents.c_str(),
                                                   new_contents.size());
            if (written != new_contents.size()) {
              LOG(ERROR) << "Failed to write file " << file.value() << ".";
              return HResultFromLastError();
            }
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
    HRESULT hr = OpenService(SERVICE_STOP | SERVICE_QUERY_STATUS, &service);
    if (FAILED(hr))
      return hr;
    SERVICE_STATUS status = {0};
    if (!::ControlService(service, SERVICE_CONTROL_STOP, &status))
      return HResultFromLastError();
    while (::QueryServiceStatus(service, &status) &&
           status.dwCurrentState > SERVICE_STOPPED) {
      Sleep(500);
    }
    return S_OK;
  }

  HRESULT CheckRequirements() {
    base::FilePath requirements_filename(user_data_dir_);
    requirements_filename = requirements_filename.Append(kRequirementsFileName);
    std::string output;
    output.append("Printers available for " +
                  WideToASCII(GetCurrentUserName()) + ":\n");
    scoped_refptr<printing::PrintBackend> backend(
        printing::PrintBackend::CreateInstance(NULL));
    printing::PrinterList printer_list;
    backend->EnumeratePrinters(&printer_list);
    for (size_t i = 0; i < printer_list.size(); ++i) {
      output += " ";
      output += printer_list[i].printer_name;
      output += "\n";
    }
    base::FilePath chrome = chrome_launcher_support::GetAnyChromePath();
    output.append(chrome.empty() ? kChromeIsNotAvalible : kChromeIsAvalible);
    file_util::WriteFile(requirements_filename, output.c_str(), output.size());
    return S_OK;
  }

  HRESULT StartConnector() {
    chrome_.reset(new ChromeLauncher(user_data_dir_));
    return chrome_->Start() ? S_OK : E_FAIL;
  }

  void StopConnector() {
    if (chrome_.get()) {
      chrome_->Stop();
      chrome_.reset();
    }
  }

  static BOOL WINAPI ConsoleCtrlHandler(DWORD type);

  bool check_requirements_;
  base::FilePath user_data_dir_;
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
