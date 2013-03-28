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
#include "base/process.h"
#include "base/process_util.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "net/base/url_util.h"

namespace {

const int kShutdownTimeoutMs = 30 * 1000;

static const char16 kAutoRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

void ShutdownChrome(HANDLE process, DWORD thread_id) {
  if (::PostThreadMessage(thread_id, WM_QUIT, 0, 0) &&
      WAIT_OBJECT_0 == ::WaitForSingleObject(process, kShutdownTimeoutMs)) {
    return;
  }
  LOG(ERROR) << "Failed to shutdown process.";
  base::KillProcess(process, 0, true);
}

bool LaunchProcess(const CommandLine& cmdline,
                   base::ProcessHandle* process_handle,
                   DWORD* thread_id) {
  STARTUPINFO startup_info = {};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = SW_SHOW;

  base::win::ScopedProcessInformation process_info;
  if (!CreateProcess(NULL,
      const_cast<wchar_t*>(cmdline.GetCommandLineString().c_str()), NULL, NULL,
      FALSE, 0, NULL, NULL, &startup_info, process_info.Receive())) {
    return false;
  }

  if (process_handle)
    *process_handle = process_info.TakeProcessHandle();

  if (thread_id)
    *thread_id = process_info.thread_id();

  return true;
}

GURL GetCloudPrintServiceEnableURL(const std::string& proxy_id) {
  GURL url(
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kCloudPrintServiceURL));
  if (url.is_empty())
    url = GURL("https://www.google.com/cloudprint");
  url = net::AppendQueryParameter(url, "proxy", proxy_id);
  std::string url_path(url.path() + "/enable_chrome_connector/enable.html");
  GURL::Replacements replacements;
  replacements.SetPathStr(url_path);
  return url.ReplaceComponents(replacements);
}

GURL GetCloudPrintServiceEnableURLWithSignin(const std::string& proxy_id) {
  GURL url(GaiaUrls::GetInstance()->service_login_url());
  url = net::AppendQueryParameter(url, "service", "cloudprint");
  url = net::AppendQueryParameter(url, "sarp", "1");
  return net::AppendQueryParameter(
      url, "continue", GetCloudPrintServiceEnableURL(proxy_id).spec());
}

std::string UpdateServiceState(const std::string& json) {
  std::string result;

  scoped_ptr<base::Value> service_state(base::JSONReader::Read(json));
  base::DictionaryValue* dictionary = NULL;
  if (!service_state->GetAsDictionary(&dictionary) || !dictionary) {
    return result;
  }

  bool enabled = false;
  if (!dictionary->GetBoolean(prefs::kCloudPrintProxyEnabled, &enabled) ||
      !enabled) {
    return result;
  }

  // Remove everything except kCloudPrintRoot.
  base::Value* cloud_print_root = NULL;
  dictionary->Remove(prefs::kCloudPrintRoot, &cloud_print_root);
  dictionary->Clear();
  dictionary->Set(prefs::kCloudPrintRoot, cloud_print_root);

  dictionary->SetBoolean(prefs::kCloudPrintXmppPingEnabled, true);

  base::JSONWriter::Write(dictionary, &result);

  return result;
}

void DeleteAutorunKeys(const base::FilePath& user_data_dir) {
  base::win::RegKey key(HKEY_CURRENT_USER, kAutoRunKeyPath, KEY_SET_VALUE);
  if (!key.Valid())
    return;
  std::vector<string16> to_delete;

  base::FilePath abs_user_data_dir = user_data_dir;
  file_util::AbsolutePath(&abs_user_data_dir);

  {
    base::win::RegistryValueIterator value(HKEY_CURRENT_USER, kAutoRunKeyPath);
    for (; value.Valid(); ++value) {
      if (value.Type() == REG_SZ && value.Value()) {
        CommandLine cmd = CommandLine::FromString(value.Value());
        if (cmd.GetSwitchValueASCII(switches::kProcessType) ==
            switches::kServiceProcess &&
            cmd.HasSwitch(switches::kUserDataDir)) {
          base::FilePath path_from_reg =
              cmd.GetSwitchValuePath(switches::kUserDataDir);
          file_util::AbsolutePath(&path_from_reg);
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
      CopySwitchesFromCurrent(&cmd);

      // Required switches.
      cmd.AppendSwitchASCII(switches::kProcessType, switches::kServiceProcess);
      cmd.AppendSwitchPath(switches::kUserDataDir, user_data_);
      cmd.AppendSwitch(switches::kNoServiceAutorun);

      // Optional.
      cmd.AppendSwitch(switches::kAutoLaunchAtStartup);
      cmd.AppendSwitch(switches::kDisableBackgroundMode);
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
      LaunchProcess(cmd, chrome_handle.Receive(), &thread_id);
      int exit_code = 0;
      HANDLE handles[] = {stop_event_.handle(), chrome_handle};
      DWORD wait_result = ::WaitForMultipleObjects(arraysize(handles), handles,
                                                   FALSE, INFINITE);
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

void ChromeLauncher::CopySwitchesFromCurrent(CommandLine* destination) {
  static const char* const kSwitchesToCopy[] = {
    switches::kEnableLogging,
    switches::kV,
  };
  destination->CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                kSwitchesToCopy,
                                arraysize(kSwitchesToCopy));
}

std::string ChromeLauncher::CreateServiceStateFile(
    const std::string& proxy_id,
    const std::vector<std::string>& printers) {
  std::string result;

  base::ScopedTempDir temp_user_data;
  if (!temp_user_data.CreateUniqueTempDir()) {
    LOG(ERROR) << "Can't create temp dir.";
    return result;
  }

  base::FilePath chrome_path = chrome_launcher_support::GetAnyChromePath();

  if (chrome_path.empty()) {
    LOG(ERROR) << "Can't find Chrome.";
    return result;
  }

  base::FilePath printers_file = temp_user_data.path().Append(L"printers.json");

  base::ListValue printer_list;
  printer_list.AppendStrings(printers);
  std::string printers_json;
  base::JSONWriter::Write(&printer_list, &printers_json);
  size_t written = file_util::WriteFile(printers_file,
                                        printers_json.c_str(),
                                        printers_json.size());
  if (written != printers_json.size()) {
    LOG(ERROR) << "Can't write file.";
    return result;
  }

  CommandLine cmd(chrome_path);
  CopySwitchesFromCurrent(&cmd);
  cmd.AppendSwitchPath(switches::kUserDataDir, temp_user_data.path());
  cmd.AppendSwitchPath(switches::kCloudPrintSetupProxy, printers_file);
  cmd.AppendSwitch(switches::kNoServiceAutorun);

  // Optional.
  cmd.AppendSwitch(switches::kDisableBackgroundMode);
  cmd.AppendSwitch(switches::kDisableDefaultApps);
  cmd.AppendSwitch(switches::kDisableExtensions);
  cmd.AppendSwitch(switches::kDisableSync);
  cmd.AppendSwitch(switches::kNoDefaultBrowserCheck);
  cmd.AppendSwitch(switches::kNoFirstRun);

  cmd.AppendArg(GetCloudPrintServiceEnableURLWithSignin(proxy_id).spec());

  base::win::ScopedHandle chrome_handle;
  DWORD thread_id = 0;
  if (!LaunchProcess(cmd, chrome_handle.Receive(), &thread_id)) {
    LOG(ERROR) << "Unable to launch Chrome.";
    return result;
  }

  int exit_code = 0;
  DWORD wait_result = ::WaitForSingleObject(chrome_handle, INFINITE);
  if (wait_result != WAIT_OBJECT_0) {
    LOG(ERROR) << "Chrome launch failed.";
    return result;
  }

  std::string json;
  if (!file_util::ReadFileToString(
          temp_user_data.path().Append(chrome::kServiceStateFileName), &json)) {
    return result;
  }

  return UpdateServiceState(json);
}

