// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/chrome_launcher.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "cloud_print/common/win/cloud_print_utils.h"
#include "cloud_print/service/service_constants.h"
#include "cloud_print/service/win/service_utils.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

const int kShutdownTimeoutMs = 30 * 1000;
const int kUsageUpdateTimeoutMs = 6 * 3600 * 1000;  // 6 hours.

static const base::char16 kAutoRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// Terminates any process.
void ShutdownChrome(HANDLE process, DWORD thread_id) {
  if (::PostThreadMessage(thread_id, WM_QUIT, 0, 0) &&
      WAIT_OBJECT_0 == ::WaitForSingleObject(process, kShutdownTimeoutMs)) {
    return;
  }
  LOG(ERROR) << "Failed to shutdown process.";
  base::KillProcess(process, 0, true);
}

BOOL CALLBACK CloseIfPidEqual(HWND wnd, LPARAM lparam) {
  DWORD pid = 0;
  ::GetWindowThreadProcessId(wnd, &pid);
  if (pid == static_cast<DWORD>(lparam))
    ::PostMessage(wnd, WM_CLOSE, 0, 0);
  return TRUE;
}

void CloseAllProcessWindows(HANDLE process) {
  ::EnumWindows(&CloseIfPidEqual, GetProcessId(process));
}

// Close Chrome browser window.
void CloseChrome(HANDLE process, DWORD thread_id) {
  CloseAllProcessWindows(process);
  if (WAIT_OBJECT_0 == ::WaitForSingleObject(process, kShutdownTimeoutMs)) {
    return;
  }
  ShutdownChrome(process, thread_id);
}

bool LaunchProcess(const CommandLine& cmdline,
                   base::win::ScopedHandle* process_handle,
                   DWORD* thread_id) {
  STARTUPINFO startup_info = {};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = SW_SHOW;

  PROCESS_INFORMATION temp_process_info = {};
  base::FilePath::StringType writable_cmdline_str(
      cmdline.GetCommandLineString());
  if (!CreateProcess(NULL,
      &writable_cmdline_str[0], NULL, NULL,
      FALSE, 0, NULL, NULL, &startup_info, &temp_process_info)) {
    return false;
  }
  base::win::ScopedProcessInformation process_info(temp_process_info);

  if (process_handle)
    process_handle->Set(process_info.TakeProcessHandle());

  if (thread_id)
    *thread_id = process_info.thread_id();

  return true;
}

std::string ReadAndUpdateServiceState(const base::FilePath& directory,
                                      const std::string& proxy_id) {
  std::string json;
  base::FilePath file_path = directory.Append(chrome::kServiceStateFileName);
  if (!base::ReadFileToString(file_path, &json)) {
    return std::string();
  }

  scoped_ptr<base::Value> service_state(base::JSONReader::Read(json));
  base::DictionaryValue* dictionary = NULL;
  if (!service_state->GetAsDictionary(&dictionary) || !dictionary) {
    return std::string();
  }

  bool enabled = false;
  if (!dictionary->GetBoolean(prefs::kCloudPrintProxyEnabled, &enabled) ||
      !enabled) {
    return std::string();
  }

  std::string refresh_token;
  if (!dictionary->GetString(prefs::kCloudPrintRobotRefreshToken,
                             &refresh_token) ||
      refresh_token.empty()) {
    return std::string();
  }

  // Remove everything except kCloudPrintRoot.
  scoped_ptr<base::Value> cloud_print_root;
  dictionary->Remove(prefs::kCloudPrintRoot, &cloud_print_root);
  dictionary->Clear();
  dictionary->Set(prefs::kCloudPrintRoot, cloud_print_root.release());

  dictionary->SetBoolean(prefs::kCloudPrintXmppPingEnabled, true);
  if (!proxy_id.empty())  // Reuse proxy id if we already had one.
    dictionary->SetString(prefs::kCloudPrintProxyId, proxy_id);
  std::string result;
  base::JSONWriter::WriteWithOptions(dictionary,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &result);
  return result;
}

void DeleteAutorunKeys(const base::FilePath& user_data_dir) {
  base::win::RegKey key(HKEY_CURRENT_USER, kAutoRunKeyPath, KEY_SET_VALUE);
  if (!key.Valid())
    return;
  std::vector<base::string16> to_delete;

  base::FilePath abs_user_data_dir = base::MakeAbsoluteFilePath(user_data_dir);

  {
    base::win::RegistryValueIterator value(HKEY_CURRENT_USER, kAutoRunKeyPath);
    for (; value.Valid(); ++value) {
      if (value.Type() == REG_SZ && value.Value()) {
        CommandLine cmd = CommandLine::FromString(value.Value());
        if (cmd.GetSwitchValueASCII(switches::kProcessType) ==
            switches::kServiceProcess &&
            cmd.HasSwitch(switches::kUserDataDir)) {
          base::FilePath path_from_reg = base::MakeAbsoluteFilePath(
              cmd.GetSwitchValuePath(switches::kUserDataDir));
          if (path_from_reg == abs_user_data_dir) {
            to_delete.push_back(value.Name());
          }
        }
      }
    }
  }

  for (size_t i = 0; i < to_delete.size(); ++i) {
    key.DeleteValue(to_delete[i].c_str());
  }
}

}  // namespace

ChromeLauncher::ChromeLauncher(const base::FilePath& user_data)
    : stop_event_(true, true),
      user_data_(user_data) {
}

ChromeLauncher::~ChromeLauncher() {
}

bool ChromeLauncher::Start() {
  DeleteAutorunKeys(user_data_);
  stop_event_.Reset();
  thread_.reset(new base::DelegateSimpleThread(this, "chrome_launcher"));
  thread_->Start();
  return true;
}

void ChromeLauncher::Stop() {
  stop_event_.Signal();
  thread_->Join();
  thread_.reset();
}

void ChromeLauncher::Run() {
  const base::TimeDelta default_time_out = base::TimeDelta::FromSeconds(1);
  const base::TimeDelta max_time_out = base::TimeDelta::FromHours(1);

  for (base::TimeDelta time_out = default_time_out;;
       time_out = std::min(time_out * 2, max_time_out)) {
    base::FilePath chrome_path = chrome_launcher_support::GetAnyChromePath();

    if (!chrome_path.empty()) {
      CommandLine cmd(chrome_path);
      CopyChromeSwitchesFromCurrentProcess(&cmd);

      // Required switches.
      cmd.AppendSwitchASCII(switches::kProcessType, switches::kServiceProcess);
      cmd.AppendSwitchPath(switches::kUserDataDir, user_data_);
      cmd.AppendSwitch(switches::kNoServiceAutorun);

      // Optional.
      cmd.AppendSwitch(switches::kAutoLaunchAtStartup);
      cmd.AppendSwitch(switches::kDisableDefaultApps);
      cmd.AppendSwitch(switches::kDisableExtensions);
      cmd.AppendSwitch(switches::kDisableGpu);
      cmd.AppendSwitch(switches::kDisableSoftwareRasterizer);
      cmd.AppendSwitch(switches::kDisableSync);
      cmd.AppendSwitch(switches::kNoFirstRun);
      cmd.AppendSwitch(switches::kNoStartupWindow);

      base::win::ScopedHandle chrome_handle;
      base::Time started = base::Time::Now();
      DWORD thread_id = 0;
      LaunchProcess(cmd, &chrome_handle, &thread_id);

      HANDLE handles[] = {stop_event_.handle(), chrome_handle};
      DWORD wait_result = WAIT_TIMEOUT;
      while (wait_result == WAIT_TIMEOUT) {
        cloud_print::SetGoogleUpdateUsage(kGoogleUpdateId);
        wait_result = ::WaitForMultipleObjects(arraysize(handles), handles,
                                               FALSE, kUsageUpdateTimeoutMs);
      }
      if (wait_result == WAIT_OBJECT_0) {
        ShutdownChrome(chrome_handle, thread_id);
        break;
      } else if (wait_result == WAIT_OBJECT_0 + 1) {
        LOG(ERROR) << "Chrome process exited.";
      } else {
        LOG(ERROR) << "Error waiting Chrome (" << ::GetLastError() << ").";
      }
      if (base::Time::Now() - started > base::TimeDelta::FromHours(1)) {
        // Reset timeout because process worked long enough.
        time_out = default_time_out;
      }
    }
    if (stop_event_.TimedWait(time_out))
      break;
  }
}

std::string ChromeLauncher::CreateServiceStateFile(
    const std::string& proxy_id,
    const std::vector<std::string>& printers) {
  base::ScopedTempDir temp_user_data;
  if (!temp_user_data.CreateUniqueTempDir()) {
    LOG(ERROR) << "Can't create temp dir.";
    return std::string();
  }

  base::FilePath chrome_path = chrome_launcher_support::GetAnyChromePath();
  if (chrome_path.empty()) {
    LOG(ERROR) << "Can't find Chrome.";
    return std::string();
  }

  base::FilePath printers_file = temp_user_data.path().Append(L"printers.json");

  base::ListValue printer_list;
  printer_list.AppendStrings(printers);
  std::string printers_json;
  base::JSONWriter::Write(&printer_list, &printers_json);
  size_t written = base::WriteFile(printers_file,
                                   printers_json.c_str(),
                                   printers_json.size());
  if (written != printers_json.size()) {
    LOG(ERROR) << "Can't write file.";
    return std::string();
  }

  CommandLine cmd(chrome_path);
  CopyChromeSwitchesFromCurrentProcess(&cmd);
  cmd.AppendSwitchPath(switches::kUserDataDir, temp_user_data.path());
  cmd.AppendSwitchPath(switches::kCloudPrintSetupProxy, printers_file);
  cmd.AppendSwitch(switches::kNoServiceAutorun);

  // Optional.
  cmd.AppendSwitch(switches::kDisableDefaultApps);
  cmd.AppendSwitch(switches::kDisableExtensions);
  cmd.AppendSwitch(switches::kDisableSync);
  cmd.AppendSwitch(switches::kNoDefaultBrowserCheck);
  cmd.AppendSwitch(switches::kNoFirstRun);

  cmd.AppendArg(
      cloud_devices::GetCloudPrintEnableWithSigninURL(proxy_id).spec());

  base::win::ScopedHandle chrome_handle;
  DWORD thread_id = 0;
  if (!LaunchProcess(cmd, &chrome_handle, &thread_id)) {
    LOG(ERROR) << "Unable to launch Chrome.";
    return std::string();
  }

  for (;;) {
    DWORD wait_result = ::WaitForSingleObject(chrome_handle, 500);
    std::string json = ReadAndUpdateServiceState(temp_user_data.path(),
                                                 proxy_id);
    if (wait_result == WAIT_OBJECT_0) {
      // Return what we have because browser is closed.
      return json;
    }
    if (wait_result != WAIT_TIMEOUT) {
      LOG(ERROR) << "Chrome launch failed.";
      return std::string();
    }
    if (!json.empty()) {
      // Close chrome because Service State is ready.
      CloseChrome(chrome_handle, thread_id);
      return json;
    }
  }
}
