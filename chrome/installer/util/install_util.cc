// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// See the corresponding header file for description of the functions in this
// file.

#include "chrome/installer/util/install_util.h"

#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/metro.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

using base::win::RegKey;
using installer::ProductState;

namespace {

const wchar_t kStageBinaryPatching[] = L"binary_patching";
const wchar_t kStageBuilding[] = L"building";
const wchar_t kStageConfiguringAutoLaunch[] = L"configuring_auto_launch";
const wchar_t kStageCopyingPreferencesFile[] = L"copying_prefs";
const wchar_t kStageCreatingShortcuts[] = L"creating_shortcuts";
const wchar_t kStageEnsemblePatching[] = L"ensemble_patching";
const wchar_t kStageExecuting[] = L"executing";
const wchar_t kStageFinishing[] = L"finishing";
const wchar_t kStagePreconditions[] = L"preconditions";
const wchar_t kStageRefreshingPolicy[] = L"refreshing_policy";
const wchar_t kStageRegisteringChrome[] = L"registering_chrome";
const wchar_t kStageRemovingOldVersions[] = L"removing_old_ver";
const wchar_t kStageRollingback[] = L"rollingback";
const wchar_t kStageUncompressing[] = L"uncompressing";
const wchar_t kStageUnpacking[] = L"unpacking";
const wchar_t kStageUpdatingChannels[] = L"updating_channels";
const wchar_t kStageCreatingVisualManifest[] = L"creating_visual_manifest";
const wchar_t kStageDeferringToHigherVersion[] = L"deferring_to_higher_version";

const wchar_t* const kStages[] = {
  NULL,
  kStagePreconditions,
  kStageUncompressing,
  kStageEnsemblePatching,
  kStageBinaryPatching,
  kStageUnpacking,
  kStageBuilding,
  kStageExecuting,
  kStageRollingback,
  kStageRefreshingPolicy,
  kStageUpdatingChannels,
  kStageCopyingPreferencesFile,
  kStageCreatingShortcuts,
  kStageRegisteringChrome,
  kStageRemovingOldVersions,
  kStageFinishing,
  kStageConfiguringAutoLaunch,
  kStageCreatingVisualManifest,
  kStageDeferringToHigherVersion,
};

COMPILE_ASSERT(installer::NUM_STAGES == arraysize(kStages),
               kStages_disagrees_with_Stage_comma_they_must_match_bang);

// Creates a zero-sized non-decorated foreground window that doesn't appear
// in the taskbar. This is used as a parent window for calls to ShellExecuteEx
// in order for the UAC dialog to appear in the foreground and for focus
// to be returned to this process once the UAC task is dismissed. Returns
// NULL on failure, a handle to the UAC window on success.
HWND CreateUACForegroundWindow() {
  HWND foreground_window = ::CreateWindowEx(WS_EX_TOOLWINDOW,
                                            L"STATIC",
                                            NULL,
                                            WS_POPUP | WS_VISIBLE,
                                            0, 0, 0, 0,
                                            NULL, NULL,
                                            ::GetModuleHandle(NULL),
                                            NULL);
  if (foreground_window) {
    HMONITOR monitor = ::MonitorFromWindow(foreground_window,
                                           MONITOR_DEFAULTTONEAREST);
    if (monitor) {
      MONITORINFO mi = {0};
      mi.cbSize = sizeof(mi);
      ::GetMonitorInfo(monitor, &mi);
      RECT screen_rect = mi.rcWork;
      int x_offset = (screen_rect.right - screen_rect.left) / 2;
      int y_offset = (screen_rect.bottom - screen_rect.top) / 2;
      ::MoveWindow(foreground_window,
                   screen_rect.left + x_offset,
                   screen_rect.top + y_offset,
                   0, 0, FALSE);
    } else {
      NOTREACHED() << "Unable to get default monitor";
    }
    ::SetForegroundWindow(foreground_window);
  }
  return foreground_window;
}

}  // namespace

string16 InstallUtil::GetActiveSetupPath(BrowserDistribution* dist) {
  static const wchar_t kInstalledComponentsPath[] =
      L"Software\\Microsoft\\Active Setup\\Installed Components\\";
  return kInstalledComponentsPath + dist->GetActiveSetupGuid();
}

void InstallUtil::TriggerActiveSetupCommand() {
  string16 active_setup_reg(
      GetActiveSetupPath(BrowserDistribution::GetDistribution()));
  base::win::RegKey active_setup_key(
      HKEY_LOCAL_MACHINE, active_setup_reg.c_str(), KEY_QUERY_VALUE);
  string16 cmd_str;
  LONG read_status = active_setup_key.ReadValue(L"StubPath", &cmd_str);
  if (read_status != ERROR_SUCCESS) {
    LOG(ERROR) << active_setup_reg << ", " << read_status;
    // This should never fail if Chrome is registered at system-level, but if it
    // does there is not much else to be done.
    return;
  }

  CommandLine cmd(CommandLine::FromString(cmd_str));
  // Force creation of shortcuts as the First Run beacon might land between now
  // and the time setup.exe checks for it.
  cmd.AppendSwitch(installer::switches::kForceConfigureUserSettings);

  base::LaunchOptions launch_options;
  if (base::win::IsMetroProcess())
    launch_options.force_breakaway_from_job_ = true;
  if (!base::LaunchProcess(cmd.GetCommandLineString(), launch_options, NULL))
    PLOG(ERROR) << cmd.GetCommandLineString();
}

bool InstallUtil::ExecuteExeAsAdmin(const CommandLine& cmd, DWORD* exit_code) {
  base::FilePath::StringType program(cmd.GetProgram().value());
  DCHECK(!program.empty());
  DCHECK_NE(program[0], L'\"');

  CommandLine::StringType params(cmd.GetCommandLineString());
  if (params[0] == '"') {
    DCHECK_EQ('"', params[program.length() + 1]);
    DCHECK_EQ(program, params.substr(1, program.length()));
    params = params.substr(program.length() + 2);
  } else {
    DCHECK_EQ(program, params.substr(0, program.length()));
    params = params.substr(program.length());
  }

  TrimWhitespace(params, TRIM_ALL, &params);

  HWND uac_foreground_window = CreateUACForegroundWindow();

  SHELLEXECUTEINFO info = {0};
  info.cbSize = sizeof(SHELLEXECUTEINFO);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.hwnd = uac_foreground_window;
  info.lpVerb = L"runas";
  info.lpFile = program.c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_SHOW;

  bool success = false;
  if (::ShellExecuteEx(&info) == TRUE) {
    ::WaitForSingleObject(info.hProcess, INFINITE);
    DWORD ret_val = 0;
    if (::GetExitCodeProcess(info.hProcess, &ret_val)) {
      success = true;
      if (exit_code)
        *exit_code = ret_val;
    }
  }

  if (uac_foreground_window) {
    DestroyWindow(uac_foreground_window);
  }

  return success;
}

CommandLine InstallUtil::GetChromeUninstallCmd(
    bool system_install, BrowserDistribution::Type distribution_type) {
  ProductState state;
  if (state.Initialize(system_install, distribution_type)) {
    return state.uninstall_command();
  }
  return CommandLine(CommandLine::NO_PROGRAM);
}

void InstallUtil::GetChromeVersion(BrowserDistribution* dist,
                                   bool system_install,
                                   Version* version) {
  DCHECK(dist);
  RegKey key;
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  LONG result = key.Open(reg_root, dist->GetVersionKey().c_str(),
                         KEY_QUERY_VALUE);

  string16 version_str;
  if (result == ERROR_SUCCESS)
    result = key.ReadValue(google_update::kRegVersionField, &version_str);

  *version = Version();
  if (result == ERROR_SUCCESS && !version_str.empty()) {
    VLOG(1) << "Existing " << dist->GetAppShortCutName() << " version found "
            << version_str;
    *version = Version(base::WideToASCII(version_str));
  } else {
    DCHECK_EQ(ERROR_FILE_NOT_FOUND, result);
    VLOG(1) << "No existing " << dist->GetAppShortCutName()
            << " install found.";
  }
}

void InstallUtil::GetCriticalUpdateVersion(BrowserDistribution* dist,
                                           bool system_install,
                                           Version* version) {
  DCHECK(dist);
  RegKey key;
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  LONG result =
      key.Open(reg_root, dist->GetVersionKey().c_str(), KEY_QUERY_VALUE);

  string16 version_str;
  if (result == ERROR_SUCCESS)
    result = key.ReadValue(google_update::kRegCriticalVersionField,
                           &version_str);

  *version = Version();
  if (result == ERROR_SUCCESS && !version_str.empty()) {
    VLOG(1) << "Critical Update version for " << dist->GetAppShortCutName()
            << " found " << version_str;
    *version = Version(base::WideToASCII(version_str));
  } else {
    DCHECK_EQ(ERROR_FILE_NOT_FOUND, result);
    VLOG(1) << "No existing " << dist->GetAppShortCutName()
            << " install found.";
  }
}

bool InstallUtil::IsOSSupported() {
  // We do not support Win2K or older, or XP without service pack 2.
  VLOG(1) << base::SysInfo::OperatingSystemName() << ' '
          << base::SysInfo::OperatingSystemVersion();
  base::win::Version version = base::win::GetVersion();
  return (version > base::win::VERSION_XP) ||
      ((version == base::win::VERSION_XP) &&
       (base::win::OSInfo::GetInstance()->service_pack().major >= 2));
}

void InstallUtil::AddInstallerResultItems(bool system_install,
                                          const string16& state_key,
                                          installer::InstallStatus status,
                                          int string_resource_id,
                                          const string16* const launch_cmd,
                                          WorkItemList* install_list) {
  DCHECK(install_list);
  const HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  DWORD installer_result = (GetInstallReturnCode(status) == 0) ? 0 : 1;
  install_list->AddCreateRegKeyWorkItem(root, state_key);
  install_list->AddSetRegValueWorkItem(root, state_key,
                                       installer::kInstallerResult,
                                       installer_result, true);
  install_list->AddSetRegValueWorkItem(root, state_key,
                                       installer::kInstallerError,
                                       static_cast<DWORD>(status), true);
  if (string_resource_id != 0) {
    string16 msg = installer::GetLocalizedString(string_resource_id);
    install_list->AddSetRegValueWorkItem(root, state_key,
        installer::kInstallerResultUIString, msg, true);
  }
  if (launch_cmd != NULL && !launch_cmd->empty()) {
    install_list->AddSetRegValueWorkItem(root, state_key,
        installer::kInstallerSuccessLaunchCmdLine, *launch_cmd, true);
  }
}

void InstallUtil::UpdateInstallerStage(bool system_install,
                                       const string16& state_key_path,
                                       installer::InstallerStage stage) {
  DCHECK_LE(static_cast<installer::InstallerStage>(0), stage);
  DCHECK_GT(installer::NUM_STAGES, stage);
  const HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey state_key;
  LONG result = state_key.Open(root, state_key_path.c_str(),
                               KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (result == ERROR_SUCCESS) {
    if (stage == installer::NO_STAGE) {
      result = state_key.DeleteValue(installer::kInstallerExtraCode1);
      LOG_IF(ERROR, result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
          << "Failed deleting installer stage from " << state_key_path
          << "; result: " << result;
    } else {
      const DWORD extra_code_1 = static_cast<DWORD>(stage);
      result = state_key.WriteValue(installer::kInstallerExtraCode1,
                                    extra_code_1);
      LOG_IF(ERROR, result != ERROR_SUCCESS)
          << "Failed writing installer stage to " << state_key_path
          << "; result: " << result;
    }
    // TODO(grt): Remove code below here once we're convinced that our use of
    // Google Update's new InstallerExtraCode1 value is good.
    installer::ChannelInfo channel_info;
    // This will return false if the "ap" value isn't present, which is fine.
    channel_info.Initialize(state_key);
    if (channel_info.SetStage(kStages[stage]) &&
        !channel_info.Write(&state_key)) {
      LOG(ERROR) << "Failed writing installer stage to " << state_key_path;
    }
  } else {
    LOG(ERROR) << "Failed opening " << state_key_path
               << " to update installer stage; result: " << result;
  }
}

bool InstallUtil::IsPerUserInstall(const wchar_t* const exe_path) {
  wchar_t program_files_path[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                SHGFP_TYPE_CURRENT, program_files_path))) {
    return !StartsWith(exe_path, program_files_path, false);
  } else {
    NOTREACHED();
  }
  return true;
}

bool InstallUtil::IsMultiInstall(BrowserDistribution* dist,
                                 bool system_install) {
  DCHECK(dist);
  ProductState state;
  return state.Initialize(system_install, dist->GetType()) &&
         state.is_multi_install();
}

bool CheckIsChromeSxSProcess() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  CHECK(command_line);

  if (command_line->HasSwitch(installer::switches::kChromeSxS))
    return true;

  // Also return true if we are running from Chrome SxS installed path.
  base::FilePath exe_dir;
  PathService::Get(base::DIR_EXE, &exe_dir);
  string16 chrome_sxs_dir(installer::kGoogleChromeInstallSubDir2);
  chrome_sxs_dir.append(installer::kSxSSuffix);
  return base::FilePath::CompareEqualIgnoreCase(
          exe_dir.BaseName().value(), installer::kInstallBinaryDir) &&
      base::FilePath::CompareEqualIgnoreCase(
          exe_dir.DirName().BaseName().value(), chrome_sxs_dir);
}

bool InstallUtil::IsChromeSxSProcess() {
  static bool sxs = CheckIsChromeSxSProcess();
  return sxs;
}

bool InstallUtil::GetSentinelFilePath(const base::FilePath::CharType* file,
                                      BrowserDistribution* dist,
                                      base::FilePath* path) {
  base::FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return false;

  if (IsPerUserInstall(exe_path.value().c_str())) {
    const base::FilePath maybe_product_dir(exe_path.DirName().DirName());
    if (file_util::PathExists(exe_path.Append(installer::kChromeExe))) {
      // DIR_EXE is most likely Chrome's directory in which case |exe_path| is
      // the user-level sentinel path.
      *path = exe_path;
    } else if (file_util::PathExists(
                   maybe_product_dir.Append(installer::kChromeExe))) {
      // DIR_EXE can also be the Installer directory if this is called from a
      // setup.exe running from Application\<version>\Installer (see
      // InstallerState::GetInstallerDirectory) in which case Chrome's directory
      // is two levels up.
      *path = maybe_product_dir;
    } else {
      NOTREACHED();
      return false;
    }
  } else {
    std::vector<base::FilePath> user_data_dir_paths;
    installer::GetChromeUserDataPaths(dist, &user_data_dir_paths);

    if (!user_data_dir_paths.empty())
      *path = user_data_dir_paths[0];
    else
      return false;
  }

  *path = path->Append(file);
  return true;
}

// This method tries to delete a registry key and logs an error message
// in case of failure. It returns true if deletion is successful (or the key did
// not exist), otherwise false.
bool InstallUtil::DeleteRegistryKey(HKEY root_key,
                                    const string16& key_path) {
  VLOG(1) << "Deleting registry key " << key_path;
  LONG result = ::SHDeleteKey(root_key, key_path.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete registry key: " << key_path
               << " error: " << result;
    return false;
  }
  return true;
}

// This method tries to delete a registry value and logs an error message
// in case of failure. It returns true if deletion is successful (or the key did
// not exist), otherwise false.
bool InstallUtil::DeleteRegistryValue(HKEY reg_root,
                                      const string16& key_path,
                                      const string16& value_name) {
  RegKey key;
  LONG result = key.Open(reg_root, key_path.c_str(), KEY_SET_VALUE);
  if (result == ERROR_SUCCESS)
    result = key.DeleteValue(value_name.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete registry value: " << value_name
               << " error: " << result;
    return false;
  }
  return true;
}

// static
InstallUtil::ConditionalDeleteResult InstallUtil::DeleteRegistryKeyIf(
    HKEY root_key,
    const string16& key_to_delete_path,
    const string16& key_to_test_path,
    const wchar_t* value_name,
    const RegistryValuePredicate& predicate) {
  DCHECK(root_key);
  ConditionalDeleteResult delete_result = NOT_FOUND;
  RegKey key;
  string16 actual_value;
  if (key.Open(root_key, key_to_test_path.c_str(),
               KEY_QUERY_VALUE) == ERROR_SUCCESS &&
      key.ReadValue(value_name, &actual_value) == ERROR_SUCCESS &&
      predicate.Evaluate(actual_value)) {
    key.Close();
    delete_result = DeleteRegistryKey(root_key, key_to_delete_path)
        ? DELETED : DELETE_FAILED;
  }
  return delete_result;
}

// static
InstallUtil::ConditionalDeleteResult InstallUtil::DeleteRegistryValueIf(
    HKEY root_key,
    const wchar_t* key_path,
    const wchar_t* value_name,
    const RegistryValuePredicate& predicate) {
  DCHECK(root_key);
  DCHECK(key_path);
  ConditionalDeleteResult delete_result = NOT_FOUND;
  RegKey key;
  string16 actual_value;
  if (key.Open(root_key, key_path,
               KEY_QUERY_VALUE | KEY_SET_VALUE) == ERROR_SUCCESS &&
      key.ReadValue(value_name, &actual_value) == ERROR_SUCCESS &&
      predicate.Evaluate(actual_value)) {
    LONG result = key.DeleteValue(value_name);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete registry value: "
                 << (value_name ? value_name : L"(Default)")
                 << " error: " << result;
      delete_result = DELETE_FAILED;
    }
    delete_result = DELETED;
  }
  return delete_result;
}

bool InstallUtil::ValueEquals::Evaluate(const string16& value) const {
  return value == value_to_match_;
}

// static
int InstallUtil::GetInstallReturnCode(installer::InstallStatus status) {
  switch (status) {
    case installer::FIRST_INSTALL_SUCCESS:
    case installer::INSTALL_REPAIRED:
    case installer::NEW_VERSION_UPDATED:
    case installer::IN_USE_UPDATED:
      return 0;
    default:
      return status;
  }
}

// static
void InstallUtil::MakeUninstallCommand(const string16& program,
                                       const string16& arguments,
                                       CommandLine* command_line) {
  *command_line = CommandLine::FromString(L"\"" + program + L"\" " + arguments);
}

// static
string16 InstallUtil::GetCurrentDate() {
  static const wchar_t kDateFormat[] = L"yyyyMMdd";
  wchar_t date_str[arraysize(kDateFormat)] = {0};
  int len = GetDateFormatW(LOCALE_INVARIANT, 0, NULL, kDateFormat,
                           date_str, arraysize(date_str));
  if (len) {
    --len;  // Subtract terminating \0.
  } else {
    PLOG(DFATAL) << "GetDateFormat";
  }

  return string16(date_str, len);
}

// Open |path| with minimal access to obtain information about it, returning
// true and populating |handle| on success.
// static
bool InstallUtil::ProgramCompare::OpenForInfo(const base::FilePath& path,
                                              base::win::ScopedHandle* handle) {
  DCHECK(handle);
  handle->Set(base::CreatePlatformFile(path, base::PLATFORM_FILE_OPEN, NULL,
                                       NULL));
  return handle->IsValid();
}

// Populate |info| for |handle|, returning true on success.
// static
bool InstallUtil::ProgramCompare::GetInfo(const base::win::ScopedHandle& handle,
                                          BY_HANDLE_FILE_INFORMATION* info) {
  DCHECK(handle.IsValid());
  return GetFileInformationByHandle(
      const_cast<base::win::ScopedHandle&>(handle), info) != 0;
}

InstallUtil::ProgramCompare::ProgramCompare(const base::FilePath& path_to_match)
    : path_to_match_(path_to_match),
      file_handle_(base::kInvalidPlatformFileValue),
      file_info_() {
  DCHECK(!path_to_match_.empty());
  if (!OpenForInfo(path_to_match_, &file_handle_)) {
    PLOG(WARNING) << "Failed opening " << path_to_match_.value()
                  << "; falling back to path string comparisons.";
  } else if (!GetInfo(file_handle_, &file_info_)) {
    PLOG(WARNING) << "Failed getting information for "
                  << path_to_match_.value()
                  << "; falling back to path string comparisons.";
    file_handle_.Close();
  }
}

InstallUtil::ProgramCompare::~ProgramCompare() {
}

bool InstallUtil::ProgramCompare::Evaluate(const string16& value) const {
  // Suss out the exe portion of the value, which is expected to be a command
  // line kinda (or exactly) like:
  // "c:\foo\bar\chrome.exe" -- "%1"
  base::FilePath program(CommandLine::FromString(value).GetProgram());
  if (program.empty()) {
    LOG(WARNING) << "Failed to parse an executable name from command line: \""
                 << value << "\"";
    return false;
  }

  return EvaluatePath(program);
}

bool InstallUtil::ProgramCompare::EvaluatePath(
    const base::FilePath& path) const {
  // Try the simple thing first: do the paths happen to match?
  if (base::FilePath::CompareEqualIgnoreCase(path_to_match_.value(),
                                             path.value()))
    return true;

  // If the paths don't match and we couldn't open the expected file, we've done
  // our best.
  if (!file_handle_.IsValid())
    return false;

  // Open the program and see if it references the expected file.
  base::win::ScopedHandle handle;
  BY_HANDLE_FILE_INFORMATION info = {};

  return (OpenForInfo(path, &handle) &&
          GetInfo(handle, &info) &&
          info.dwVolumeSerialNumber == file_info_.dwVolumeSerialNumber &&
          info.nFileIndexHigh == file_info_.nFileIndexHigh &&
          info.nFileIndexLow == file_info_.nFileIndexLow);
}
