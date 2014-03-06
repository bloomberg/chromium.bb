// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <security.h>

#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "cloud_print/common/win/cloud_print_utils.h"
#include "cloud_print/service/service_constants.h"
#include "cloud_print/service/service_state.h"
#include "cloud_print/service/service_switches.h"
#include "cloud_print/service/win/chrome_launcher.h"
#include "cloud_print/service/win/service_controller.h"
#include "cloud_print/service/win/service_listener.h"
#include "cloud_print/service/win/service_utils.h"
#include "cloud_print/service/win/setup_listener.h"

namespace {

void InvalidUsage() {
  base::FilePath service_path;
  CHECK(PathService::Get(base::FILE_EXE, &service_path));

  std::cout << cloud_print::LoadLocalString(IDS_COMMAND_LINE_HELP_TITLE);
  std::cout << " " << service_path.BaseName().value();
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
  std::cout << cloud_print::LoadLocalString(IDS_COMMAND_LINE_DESCRIPTION);
  std::cout << "\n\n";

  struct {
    const char* name;
    int description;
  } kSwitchHelp[] = {{
    kInstallSwitch, IDS_SWITCH_HELP_INSTALL
  }, {
    switches::kUserDataDir, IDS_SWITCH_HELP_DATA_DIR
  }, {
    kUninstallSwitch, IDS_SWITCH_HELP_UNINSTALL
  }, {
    kStartSwitch, IDS_SWITCH_HELP_START
  }, {
    kStopSwitch, IDS_SWITCH_HELP_STOP
  }};

  for (size_t i = 0; i < arraysize(kSwitchHelp); ++i) {
    std::cout << std::setiosflags(std::ios::left);
    std::cout << "  -" << std::setw(16) << kSwitchHelp[i].name;
    std::cout << cloud_print::LoadLocalString(kSwitchHelp[i].description);
    std::cout << "\n";
  }
  std::cout << "\n";
}

base::string16 GetOption(int string_id, const base::string16& default,
                   bool secure) {
  base::string16 prompt_format = cloud_print::LoadLocalString(string_id);
  std::vector<base::string16> substitutions(1, default);
  std::cout << ReplaceStringPlaceholders(prompt_format, substitutions, NULL);
  base::string16 tmp;
  if (secure) {
    DWORD saved_mode = 0;
    // Don't close.
    HANDLE stdin_handle = ::GetStdHandle(STD_INPUT_HANDLE);
    ::GetConsoleMode(stdin_handle, &saved_mode);
    ::SetConsoleMode(stdin_handle, saved_mode & ~ENABLE_ECHO_INPUT);
    std::getline(std::wcin, tmp);
    ::SetConsoleMode(stdin_handle, saved_mode);
    std::cout << "\n";
  } else {
    std::getline(std::wcin, tmp);
  }
  if (tmp.empty())
    return default;
  return tmp;
}

HRESULT ReportError(HRESULT hr, int string_id) {
  LOG(ERROR) << cloud_print::GetErrorMessage(hr);
  std::cerr << cloud_print::LoadLocalString(string_id);
  std::cerr << "\n";
  return hr;
}

base::string16 StateAsString(ServiceController::State state) {
  DWORD string_id = 0;
  switch(state) {
  case ServiceController::STATE_NOT_FOUND:
    string_id = IDS_SERVICE_NOT_FOUND;
    break;
  case ServiceController::STATE_STOPPED:
    string_id = IDS_SERVICE_STOPPED;
    break;
  case ServiceController::STATE_RUNNING:
    string_id = IDS_SERVICE_RUNNING;
    break;
  }
  return string_id ? cloud_print::LoadLocalString(string_id) : base::string16();
}

}  // namespace


class CloudPrintServiceModule
    : public ATL::CAtlServiceModuleT<CloudPrintServiceModule,
                                     IDS_SERVICE_NAME> {
 public:
  typedef ATL::CAtlServiceModuleT<CloudPrintServiceModule,
                                  IDS_SERVICE_NAME> Base;

  CloudPrintServiceModule()
      : check_requirements_(false),
        controller_(new ServiceController()) {
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

    LOG(INFO) << command_line.GetCommandLineString();

    bool is_service = false;
    *pnRetCode = ParseCommandLine(command_line, &is_service);
    if (FAILED(*pnRetCode)) {
      ReportError(*pnRetCode, IDS_OPERATION_FAILED_TITLE);
    }
    if (!is_service) {
      controller_->UpdateState();
      std::cout << cloud_print::LoadLocalString(IDS_STATE_LABEL);
      std::cout << " " << StateAsString(controller_->state());
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
    if (!user_data_dir_switch_.empty())
      user_data_dir_switch_ = base::MakeAbsoluteFilePath(user_data_dir_switch_);

    if (command_line.HasSwitch(kStopSwitch))
      return controller_->StopService();

    if (command_line.HasSwitch(kUninstallSwitch))
      return controller_->UninstallService();

    if (command_line.HasSwitch(kInstallSwitch)) {
      base::string16 run_as_user;
      base::string16 run_as_password;
      base::FilePath user_data_dir;
      std::vector<std::string> printers;
      HRESULT hr = SelectWindowsAccount(&run_as_user, &run_as_password,
                                        &user_data_dir, &printers);
      if (FAILED(hr))
        return hr;

      DCHECK(user_data_dir_switch_.empty() ||
             user_data_dir_switch_ == user_data_dir);

      hr = SetupServiceState(user_data_dir, printers);
      if (FAILED(hr))
        return hr;

      hr = controller_->InstallConnectorService(
          run_as_user, run_as_password, user_data_dir_switch_,
          command_line.HasSwitch(switches::kEnableLogging));
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

  HRESULT SelectWindowsAccount(base::string16* run_as_user,
                               base::string16* run_as_password,
                               base::FilePath* user_data_dir,
                               std::vector<std::string>* printers) {
    *run_as_user = GetCurrentUserName();
    std::cout << cloud_print::LoadLocalString(IDS_WINDOWS_USER_PROMPT1) << "\n";
    *run_as_user = GetOption(IDS_WINDOWS_USER_PROMPT2, *run_as_user, false);
    *run_as_user = ReplaceLocalHostInName(*run_as_user);
    *run_as_password = GetOption(IDS_WINDOWS_PASSWORD_PROMPT, L"", true);
    SetupListener setup(*run_as_user);
    HRESULT hr = controller_->InstallCheckService(*run_as_user,
                                                  *run_as_password,
                                                  user_data_dir_switch_);
    if (FAILED(hr)) {
      return ReportError(hr, IDS_ERROR_FAILED_INSTALL_SERVICE);
    }

    {
      // Always uninstall service after requirements check.
      base::ScopedClosureRunner scoped_uninstall(
          base::Bind(base::IgnoreResult(&ServiceController::UninstallService),
                     base::Unretained(controller_.get())));

      hr = controller_->StartService();
      if (FAILED(hr)) {
        return ReportError(hr, IDS_ERROR_FAILED_START_SERVICE);
      }

      if (!setup.WaitResponce(base::TimeDelta::FromSeconds(30))) {
        return ReportError(E_FAIL, IDS_ERROR_FAILED_START_SERVICE);
      }
    }

    if (setup.user_data_dir().empty()) {
      return ReportError(E_FAIL, IDS_ERROR_NO_DATA_DIR);
    }

    if (setup.chrome_path().empty()) {
      return ReportError(E_FAIL, IDS_ERROR_NO_CHROME);
    }

    if (!setup.is_xps_available()) {
      return ReportError(E_FAIL, IDS_ERROR_NO_XPS);
    }

    std::cout << "\n";
    std::cout << cloud_print::LoadLocalString(IDS_SERVICE_ENV_CHECK);
    std::cout << "\n";
    std::cout << cloud_print::LoadLocalString(IDS_SERVICE_ENV_USER);
    std::cout << "\n  " << setup.user_name() << "\n";
    std::cout << cloud_print::LoadLocalString(IDS_SERVICE_ENV_CHROME);
    std::cout << "\n  " << setup.chrome_path().value() << "\n";
    std::cout << cloud_print::LoadLocalString(IDS_SERVICE_ENV_DATADIR);
    std::cout << "\n  " << setup.user_data_dir().value() << "\n";
    std::cout << cloud_print::LoadLocalString(IDS_SERVICE_ENV_PRINTERS);
    std::cout << "\n  ";
    std::ostream_iterator<std::string> cout_it(std::cout, "\n  ");
    std::copy(setup.printers().begin(), setup.printers().end(), cout_it);
    std::cout << "\n";

    *user_data_dir = setup.user_data_dir();
    *printers = setup.printers();
    return S_OK;
  }

  HRESULT SetupServiceState(const base::FilePath& user_data_dir,
                            const std::vector<std::string>& printers) {
    base::FilePath file = user_data_dir.Append(chrome::kServiceStateFileName);

    std::string contents;
    ServiceState service_state;

    bool is_valid = base::ReadFileToString(file, &contents) &&
                    service_state.FromString(contents);
    std::string proxy_id = service_state.proxy_id();

    LOG(INFO) << file.value() << ": " << contents;

    base::string16 message =
        cloud_print::LoadLocalString(IDS_ADD_PRINTERS_USING_CHROME);
    std::cout << "\n" << message.c_str() << "\n" ;
    std::string new_contents =
        ChromeLauncher::CreateServiceStateFile(proxy_id, printers);

    if (new_contents.empty()) {
      return ReportError(E_FAIL, IDS_ERROR_FAILED_CREATE_CONFIG);
    }

    if (new_contents != contents) {
      size_t  written = base::WriteFile(file, new_contents.c_str(),
                                              new_contents.size());
      if (written != new_contents.size()) {
        return ReportError(cloud_print::GetLastHResult(),
                           IDS_ERROR_FAILED_CREATE_CONFIG);
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
    return result.Append(kSubDirectory);
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

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  logging::SetMinLogLevel(
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableLogging) ?
      logging::LOG_INFO : logging::LOG_FATAL);

  return _AtlModule.WinMain(0);
}

