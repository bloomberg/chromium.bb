// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <security.h>

#include <iomanip>
#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "cloud_print/service/service_state.h"
#include "cloud_print/service/service_switches.h"
#include "cloud_print/service/win/chrome_launcher.h"
#include "cloud_print/service/win/service_controller.h"
#include "cloud_print/service/win/service_listener.h"
#include "cloud_print/service/win/service_utils.h"
#include "cloud_print/service/win/setup_listener.h"

namespace {

const wchar_t kAppDataSubDir[] = L"Google\\Cloud Printe Service";

void InvalidUsage() {
  base::FilePath service_path;
  CHECK(PathService::Get(base::FILE_EXE, &service_path));

  std::cout << "Usage: ";
  std::cout << service_path.BaseName().value();
  std::cout << " [";
    std::cout << "[";
      std::cout << "[";
        std::cout << " -" << kInstallSwitch;
        std::cout << " [ -" << switches::kUserDataDir << "=DIRECTORY ]";
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
    { switches::kUserDataDir,
      "User data directory with \"Service State\" file." },
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

}  // namespace


class CloudPrintServiceModule
    : public ATL::CAtlServiceModuleT<CloudPrintServiceModule, IDS_SERVICENAME> {
 public:
  typedef ATL::CAtlServiceModuleT<CloudPrintServiceModule,
                                  IDS_SERVICENAME> Base;

  CloudPrintServiceModule()
      : check_requirements_(false),
        controller_(new ServiceController(m_szServiceName)) {
  }

  static wchar_t* GetAppIdT() {
    return ServiceController::GetAppIdT();
  };

  HRESULT InitializeSecurity() {
    // TODO(gene): Check if we need to call CoInitializeSecurity and provide
    // the appropriate security settings for service.
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

    if (check_requirements_) {
      CheckRequirements();
    } else {
      HRESULT hr = StartConnector();
      if (FAILED(hr))
        return hr;
    }

    LogEvent(_T("Service started/resumed"));
    SetServiceStatus(SERVICE_RUNNING);

    return hr;
  }

  HRESULT PostMessageLoop() {
    StopConnector();
    setup_listener_.reset();
    return Base::PostMessageLoop();
  }

 private:
  HRESULT ParseCommandLine(const CommandLine& command_line, bool* is_service) {
    if (!is_service)
      return E_INVALIDARG;
    *is_service = false;

    user_data_dir_switch_ =
        command_line.GetSwitchValuePath(switches::kUserDataDir);
    file_util::AbsolutePath(&user_data_dir_switch_);
    if (command_line.HasSwitch(kStopSwitch))
      return controller_->StopService();

    if (command_line.HasSwitch(kUninstallSwitch))
      return controller_->UninstallService();

    if (command_line.HasSwitch(kInstallSwitch)) {
      string16 run_as_user;
      string16 run_as_password;
      base::FilePath user_data_dir;
      std::vector<std::string> printers;
      SelectWindowsAccount(&run_as_user, &run_as_password, &user_data_dir,
                           &printers);
      DCHECK(user_data_dir_switch_.empty() ||
             user_data_dir_switch_ == user_data_dir);

      HRESULT hr = SetupServiceState(user_data_dir, printers);
      if (FAILED(hr))
        return hr;

      hr = controller_->InstallService(run_as_user, run_as_password,
                                       kServiceSwitch, user_data_dir_switch_,
                                       true);
      if (SUCCEEDED(hr) && command_line.HasSwitch(kStartSwitch))
        return controller_->StartService();

      return hr;
    }

    if (command_line.HasSwitch(kStartSwitch))
      return controller_->StartService();

    if (command_line.HasSwitch(kConsoleSwitch)) {
      check_requirements_ = command_line.HasSwitch(kRequirementsSwitch);
      ::SetConsoleCtrlHandler(&ConsoleCtrlHandler, TRUE);
      HRESULT hr = Run();
      ::SetConsoleCtrlHandler(NULL, FALSE);
      return hr;
    }

    if (command_line.HasSwitch(kServiceSwitch) ||
        command_line.HasSwitch(kRequirementsSwitch)) {
      *is_service = true;
      check_requirements_ = command_line.HasSwitch(kRequirementsSwitch);
      return S_OK;
    }


    InvalidUsage();
    return S_FALSE;
  }

  void SelectWindowsAccount(string16* run_as_user, string16* run_as_password,
                            base::FilePath* user_data_dir,
                            std::vector<std::string>* printers) {
    *run_as_user = GetCurrentUserName();
    for (;;) {
      std::cout << "\nPlease provide Windows account to run service.\n";
      *run_as_user = ASCIIToWide(GetOption("Account as DOMAIN\\USERNAME",
                                            WideToASCII(*run_as_user), false));
      *run_as_password = ASCIIToWide(GetOption("Password", "", true));

      SetupListener setup(*run_as_user);
      if (FAILED(controller_->InstallService(*run_as_user, *run_as_password,
                                             kRequirementsSwitch,
                                             user_data_dir_switch_, false))) {
        LOG(ERROR) << "Failed to install service as " << *run_as_user << ".";
        continue;
      }

      bool service_started = SUCCEEDED(controller_->StartService());

      if (service_started &&
          !setup.WaitResponce(base::TimeDelta::FromSeconds(30))) {
        LOG(ERROR) << "Failed to check environment for user " << *run_as_user
                   << ".";
      }

      controller_->UninstallService();
      if (!service_started) {
        LOG(ERROR) << "Failed to start service as " << *run_as_user << ".";
        continue;
      }
      if (setup.user_data_dir().empty()) {
        LOG(ERROR) << "Service can't access user data dir.";
        continue;
      }
      if (setup.chrome_path().empty()) {
        LOG(ERROR) << "Chrome is not available  for " << *run_as_user << ".";
        continue;
      }
      if (!setup.is_xps_availible()) {
        LOG(ERROR) << "XPS pack is not installed.";
        continue;
      }

      std::cout << "\nService requirements check result: \n";
      std::cout << "Username: " << setup.user_name()<< "\n";
      std::cout << "Chrome: " << setup.chrome_path().value()<< "\n";
      std::cout << "UserDataDir: " << setup.user_data_dir().value()<< "\n";
      std::cout << "Printers:\n  ";
      std::ostream_iterator<std::string> cout_it(std::cout, "\n  ");
      std::copy(setup.printers().begin(), setup.printers().end(), cout_it);
      std::cout << "\n";

      if (AskUser("Do you want to use " + WideToASCII(*run_as_user) + "?")) {
        *user_data_dir = setup.user_data_dir();
        *printers = setup.printers();
        return;
      }
    }
  }

  HRESULT SetupServiceState(const base::FilePath& user_data_dir,
                            const std::vector<std::string>& printers) {
    base::FilePath file = user_data_dir.Append(chrome::kServiceStateFileName);

    for (size_t try_count = 0;; ++try_count) {
      std::string contents;
      ServiceState service_state;

      bool is_valid = file_util::ReadFileToString(file, &contents) &&
                      service_state.FromString(contents);
      std::string proxy_id = service_state.proxy_id();

      std::cout << "\nFile '" << file.value() << "' content:\n";
      std::cout << contents << "\n";

      if (!is_valid && try_count) {
        LOG(ERROR) << "Invalid or missing file: " << file.value();
      }

      if (is_valid) {
        if (AskUser("Do you want to use '" + WideToASCII(file.value()) + "'")) {
          return S_OK;
        } else {
          is_valid = false;
        }
      }

      while (!is_valid) {
        std::cout << "\nUse Chrome to setup Cloud Print Settings.\n";
        std::string new_contents =
            ChromeLauncher::CreateServiceStateFile(proxy_id, printers);
        is_valid = !new_contents.empty();
        if (is_valid) {
          if (new_contents != contents) {
            size_t  written = file_util::WriteFile(file, new_contents.c_str(),
                                                   new_contents.size());
            if (written != new_contents.size()) {
              LOG(ERROR) << "Failed to write file " << file.value() << ".";
              DWORD last_error = GetLastError();
              return last_error ? HRESULT_FROM_WIN32(last_error) : E_FAIL;
            }
          }
        }
      }
    }

    return S_OK;
  }

  void CheckRequirements() {
    setup_listener_.reset(new ServiceListener(GetUserDataDir()));
  }

  HRESULT StartConnector() {
    chrome_.reset(new ChromeLauncher(GetUserDataDir()));
    return chrome_->Start() ? S_OK : E_FAIL;
  }

  void StopConnector() {
    if (chrome_.get()) {
      chrome_->Stop();
      chrome_.reset();
    }
  }

  base::FilePath GetUserDataDir() const {
    if (!user_data_dir_switch_.empty())
      return user_data_dir_switch_;
    base::FilePath result;
    CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &result));
    return result.Append(kAppDataSubDir);
  }

  static BOOL WINAPI ConsoleCtrlHandler(DWORD type);

  bool check_requirements_;
  base::FilePath user_data_dir_switch_;
  scoped_ptr<ChromeLauncher> chrome_;
  scoped_ptr<ServiceController> controller_;
  scoped_ptr<ServiceListener> setup_listener_;
};

CloudPrintServiceModule _AtlModule;

BOOL CloudPrintServiceModule::ConsoleCtrlHandler(DWORD type) {
  PostThreadMessage(_AtlModule.m_dwThreadID, WM_QUIT, 0, 0);
  return TRUE;
}

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  base::AtExitManager at_exit;
  return _AtlModule.WinMain(0);
}

