// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#include "chrome/installer/setup/setup_util.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "third_party/bspatch/mbspatch.h"

namespace installer {

namespace {

// Launches |setup_exe| with |command_line|, save --install-archive and its
// value if present. Returns false if the process failed to launch. Otherwise,
// waits indefinitely for it to exit and populates |exit_code| as expected. On
// the off chance that waiting itself fails, |exit_code| is set to
// WAIT_FOR_EXISTING_FAILED.
bool LaunchAndWaitForExistingInstall(const base::FilePath& setup_exe,
                                     const CommandLine& command_line,
                                     int* exit_code) {
  DCHECK(exit_code);
  CommandLine new_cl(setup_exe);

  // Copy over all switches but --install-archive.
  CommandLine::SwitchMap switches(command_line.GetSwitches());
  switches.erase(switches::kInstallArchive);
  for (CommandLine::SwitchMap::const_iterator i = switches.begin();
       i != switches.end(); ++i) {
    if (i->second.empty())
      new_cl.AppendSwitch(i->first);
    else
      new_cl.AppendSwitchNative(i->first, i->second);
  }

  // Copy over all arguments.
  CommandLine::StringVector args(command_line.GetArgs());
  for (CommandLine::StringVector::const_iterator i = args.begin();
       i != args.end(); ++i) {
    new_cl.AppendArgNative(*i);
  }

  // Launch the process and wait for it to exit.
  VLOG(1) << "Launching existing installer with command: "
          << new_cl.GetCommandLineString();
  base::ProcessHandle handle = INVALID_HANDLE_VALUE;
  if (!base::LaunchProcess(new_cl, base::LaunchOptions(), &handle)) {
    PLOG(ERROR) << "Failed to launch existing installer with command: "
                << new_cl.GetCommandLineString();
    return false;
  }
  if (!base::WaitForExitCode(handle, exit_code)) {
    PLOG(DFATAL) << "Failed to get exit code from existing installer";
    *exit_code = WAIT_FOR_EXISTING_FAILED;
  } else {
    VLOG(1) << "Existing installer returned exit code " << *exit_code;
  }
  return true;
}

// Returns true if product |type| cam be meaningfully installed without the
// --multi-install flag.
bool SupportsSingleInstall(BrowserDistribution::Type type) {
  return (type == BrowserDistribution::CHROME_BROWSER ||
          type == BrowserDistribution::CHROME_FRAME);
}

}  // namespace

int CourgettePatchFiles(const base::FilePath& src,
                        const base::FilePath& patch,
                        const base::FilePath& dest) {
  VLOG(1) << "Applying Courgette patch " << patch.value()
          << " to file " << src.value()
          << " and generating file " << dest.value();

  if (src.empty() || patch.empty() || dest.empty())
    return installer::PATCH_INVALID_ARGUMENTS;

  const courgette::Status patch_status =
      courgette::ApplyEnsemblePatch(src.value().c_str(),
                                    patch.value().c_str(),
                                    dest.value().c_str());
  const int exit_code = (patch_status != courgette::C_OK) ?
      static_cast<int>(patch_status) + kCourgetteErrorOffset : 0;

  LOG_IF(ERROR, exit_code)
      << "Failed to apply Courgette patch " << patch.value()
      << " to file " << src.value() << " and generating file " << dest.value()
      << ". err=" << exit_code;

  return exit_code;
}

int BsdiffPatchFiles(const base::FilePath& src,
                     const base::FilePath& patch,
                     const base::FilePath& dest) {
  VLOG(1) << "Applying bsdiff patch " << patch.value()
          << " to file " << src.value()
          << " and generating file " << dest.value();

  if (src.empty() || patch.empty() || dest.empty())
    return installer::PATCH_INVALID_ARGUMENTS;

  const int patch_status = courgette::ApplyBinaryPatch(src, patch, dest);
  const int exit_code = patch_status != OK ?
                        patch_status + kBsdiffErrorOffset : 0;

  LOG_IF(ERROR, exit_code)
      << "Failed to apply bsdiff patch " << patch.value()
      << " to file " << src.value() << " and generating file " << dest.value()
      << ". err=" << exit_code;

  return exit_code;
}

Version* GetMaxVersionFromArchiveDir(const base::FilePath& chrome_path) {
  VLOG(1) << "Looking for Chrome version folder under " << chrome_path.value();
  base::FileEnumerator version_enum(chrome_path, false,
      base::FileEnumerator::DIRECTORIES);
  // TODO(tommi): The version directory really should match the version of
  // setup.exe.  To begin with, we should at least DCHECK that that's true.

  scoped_ptr<Version> max_version(new Version("0.0.0.0"));
  bool version_found = false;

  while (!version_enum.Next().empty()) {
    base::FileEnumerator::FileInfo find_data = version_enum.GetInfo();
    VLOG(1) << "directory found: " << find_data.GetName().value();

    scoped_ptr<Version> found_version(
        new Version(base::UTF16ToASCII(find_data.GetName().value())));
    if (found_version->IsValid() &&
        found_version->CompareTo(*max_version.get()) > 0) {
      max_version.reset(found_version.release());
      version_found = true;
    }
  }

  return (version_found ? max_version.release() : NULL);
}

base::FilePath FindArchiveToPatch(const InstallationState& original_state,
                                  const InstallerState& installer_state) {
  // Check based on the version number advertised to Google Update, since that
  // is the value used to select a specific differential update. If an archive
  // can't be found using that, fallback to using the newest version present.
  base::FilePath patch_source;
  const ProductState* product =
      original_state.GetProductState(installer_state.system_install(),
                                     installer_state.state_type());
  if (product) {
    patch_source = installer_state.GetInstallerDirectory(product->version())
        .Append(installer::kChromeArchive);
    if (base::PathExists(patch_source))
      return patch_source;
  }
  scoped_ptr<Version> version(
      installer::GetMaxVersionFromArchiveDir(installer_state.target_path()));
  if (version) {
    patch_source = installer_state.GetInstallerDirectory(*version)
        .Append(installer::kChromeArchive);
    if (base::PathExists(patch_source))
      return patch_source;
  }
  return base::FilePath();
}

bool DeleteFileFromTempProcess(const base::FilePath& path,
                               uint32 delay_before_delete_ms) {
  static const wchar_t kRunDll32Path[] =
      L"%SystemRoot%\\System32\\rundll32.exe";
  wchar_t rundll32[MAX_PATH];
  DWORD size =
      ExpandEnvironmentStrings(kRunDll32Path, rundll32, arraysize(rundll32));
  if (!size || size >= MAX_PATH)
    return false;

  STARTUPINFO startup = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION pi = {0};
  BOOL ok = ::CreateProcess(NULL, rundll32, NULL, NULL, FALSE, CREATE_SUSPENDED,
                            NULL, NULL, &startup, &pi);
  if (ok) {
    // We use the main thread of the new process to run:
    //   Sleep(delay_before_delete_ms);
    //   DeleteFile(path);
    //   ExitProcess(0);
    // This runs before the main routine of the process runs, so it doesn't
    // matter much which executable we choose except that we don't want to
    // use e.g. a console app that causes a window to be created.
    size = (path.value().length() + 1) * sizeof(path.value()[0]);
    void* mem = ::VirtualAllocEx(pi.hProcess, NULL, size, MEM_COMMIT,
                                 PAGE_READWRITE);
    if (mem) {
      SIZE_T written = 0;
      ::WriteProcessMemory(
          pi.hProcess, mem, path.value().c_str(),
          (path.value().size() + 1) * sizeof(path.value()[0]), &written);
      HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
      PAPCFUNC sleep = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "Sleep"));
      PAPCFUNC delete_file = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "DeleteFileW"));
      PAPCFUNC exit_process = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "ExitProcess"));
      if (!sleep || !delete_file || !exit_process) {
        NOTREACHED();
        ok = FALSE;
      } else {
        ::QueueUserAPC(sleep, pi.hThread, delay_before_delete_ms);
        ::QueueUserAPC(delete_file, pi.hThread,
                       reinterpret_cast<ULONG_PTR>(mem));
        ::QueueUserAPC(exit_process, pi.hThread, 0);
        ::ResumeThread(pi.hThread);
      }
    } else {
      PLOG(ERROR) << "VirtualAllocEx";
      ::TerminateProcess(pi.hProcess, ~static_cast<UINT>(0));
    }
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
  }

  return ok != FALSE;
}

bool GetExistingHigherInstaller(
    const InstallationState& original_state,
    bool system_install,
    const Version& installer_version,
    base::FilePath* setup_exe) {
  DCHECK(setup_exe);
  bool trying_single_browser = false;
  const ProductState* existing_state =
      original_state.GetProductState(system_install,
                                     BrowserDistribution::CHROME_BINARIES);
  if (!existing_state) {
    // The binaries aren't installed, but perhaps a single-install Chrome is.
    trying_single_browser = true;
    existing_state =
        original_state.GetProductState(system_install,
                                       BrowserDistribution::CHROME_BROWSER);
  }

  if (!existing_state ||
      existing_state->version().CompareTo(installer_version) <= 0) {
    return false;
  }

  *setup_exe = existing_state->GetSetupPath();

  VLOG_IF(1, !setup_exe->empty()) << "Found a higher version of "
      << (trying_single_browser ? "single-install Chrome."
          : "multi-install Chrome binaries.");

  return !setup_exe->empty();
}

bool DeferToExistingInstall(const base::FilePath& setup_exe,
                            const CommandLine& command_line,
                            const InstallerState& installer_state,
                            const base::FilePath& temp_path,
                            InstallStatus* install_status) {
  // Copy a master_preferences file if there is one.
  base::FilePath prefs_source_path(command_line.GetSwitchValueNative(
      switches::kInstallerData));
  base::FilePath prefs_dest_path(installer_state.target_path().AppendASCII(
      kDefaultMasterPrefs));
  scoped_ptr<WorkItem> copy_prefs(WorkItem::CreateCopyTreeWorkItem(
      prefs_source_path, prefs_dest_path, temp_path, WorkItem::ALWAYS,
      base::FilePath()));
  // There's nothing to rollback if the copy fails, so punt if so.
  if (!copy_prefs->Do())
    copy_prefs.reset();

  int exit_code = 0;
  if (!LaunchAndWaitForExistingInstall(setup_exe, command_line, &exit_code)) {
    if (copy_prefs)
      copy_prefs->Rollback();
    return false;
  }
  *install_status = static_cast<InstallStatus>(exit_code);
  return true;
}

// There are 4 disjoint cases => return values {false,true}:
// (1) Product is being uninstalled => false.
// (2) Product is being installed => true.
// (3) Current operation ignores product, product is absent => false.
// (4) Current operation ignores product, product is present => true.
bool WillProductBePresentAfterSetup(
    const installer::InstallerState& installer_state,
    const installer::InstallationState& machine_state,
    BrowserDistribution::Type type) {
  DCHECK(SupportsSingleInstall(type) || installer_state.is_multi_install());

  const ProductState* product_state =
      machine_state.GetProductState(installer_state.system_install(), type);

  // Determine if the product is present prior to the current operation.
  bool is_present = (product_state != NULL);
  bool is_uninstall = installer_state.operation() == InstallerState::UNINSTALL;

  // Determine if current operation affects the product.
  const Product* product = installer_state.FindProduct(type);
  bool is_affected = (product != NULL);

  // Decide among {(1),(2),(3),(4)}.
  return is_affected ? !is_uninstall : is_present;
}

bool AdjustProcessPriority() {
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    DWORD priority_class = ::GetPriorityClass(::GetCurrentProcess());
    if (priority_class == 0) {
      PLOG(WARNING) << "Failed to get the process's priority class.";
    } else if (priority_class == BELOW_NORMAL_PRIORITY_CLASS ||
               priority_class == IDLE_PRIORITY_CLASS) {
      BOOL result = ::SetPriorityClass(::GetCurrentProcess(),
                                       PROCESS_MODE_BACKGROUND_BEGIN);
      PLOG_IF(WARNING, !result) << "Failed to enter background mode.";
      return !!result;
    }
  }
  return false;
}

void MigrateGoogleUpdateStateMultiToSingle(
    bool system_level,
    BrowserDistribution::Type to_migrate,
    const installer::InstallationState& machine_state) {
  const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  const ProductState* product = NULL;
  BrowserDistribution* dist = NULL;
  LONG result = ERROR_SUCCESS;
  base::win::RegKey state_key;

  Product product_to_migrate(
      BrowserDistribution::GetSpecificDistribution(to_migrate));

  // Copy usagestats from the binaries to the product's ClientState key.
  product = machine_state.GetProductState(system_level,
                                          BrowserDistribution::CHROME_BINARIES);
  DWORD usagestats = 0;
  if (product && product->GetUsageStats(&usagestats)) {
    dist = product_to_migrate.distribution();
    result = state_key.Open(root, dist->GetStateKey().c_str(),
                            KEY_SET_VALUE);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed opening ClientState key for "
                 << dist->GetDisplayName() << " to migrate usagestats.";
    } else {
      state_key.WriteValue(google_update::kRegUsageStatsField, usagestats);
    }
  }

  // Remove the migrating product from the "ap" value of other multi-install
  // products.
  for (int i = 0; i < BrowserDistribution::NUM_TYPES; ++i) {
    BrowserDistribution::Type type =
        static_cast<BrowserDistribution::Type>(i);
    if (type == to_migrate)
      continue;
    product = machine_state.GetProductState(system_level, type);
    if (product && product->is_multi_install()) {
      installer::ChannelInfo channel_info;
      dist = BrowserDistribution::GetSpecificDistribution(type);
      result = state_key.Open(root, dist->GetStateKey().c_str(),
                              KEY_QUERY_VALUE | KEY_SET_VALUE);
      if (result == ERROR_SUCCESS &&
          channel_info.Initialize(state_key) &&
          product_to_migrate.SetChannelFlags(false, &channel_info)) {
        VLOG(1) << "Moving " << dist->GetDisplayName()
                << " to channel: " << channel_info.value();
        channel_info.Write(&state_key);
      }
    }
  }

  // Remove -multi, all product modifiers, and everything else but the channel
  // name from the "ap" value of the product to migrate.
  dist = product_to_migrate.distribution();
  result = state_key.Open(root, dist->GetStateKey().c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (result == ERROR_SUCCESS) {
    installer::ChannelInfo channel_info;
    if (!channel_info.Initialize(state_key)) {
      LOG(ERROR) << "Failed reading " << dist->GetDisplayName()
                 << " channel info.";
    } else if (channel_info.RemoveAllModifiersAndSuffixes()) {
      VLOG(1) << "Moving " << dist->GetDisplayName()
              << " to channel: " << channel_info.value();
      channel_info.Write(&state_key);
    }
  }
}

bool IsUninstallSuccess(InstallStatus install_status) {
  // The following status values represent failed uninstalls:
  // 15: CHROME_NOT_INSTALLED
  // 20: UNINSTALL_FAILED
  // 21: UNINSTALL_CANCELLED
  return (install_status == UNINSTALL_SUCCESSFUL ||
          install_status == UNINSTALL_REQUIRES_REBOOT);
}

bool ContainsUnsupportedSwitch(const CommandLine& cmd_line) {
  static const char* const kLegacySwitches[] = {
    // Chrome Frame ready-mode.
    "ready-mode",
    "ready-mode-opt-in",
    "ready-mode-temp-opt-out",
    "ready-mode-end-temp-opt-out",
    // Chrome Frame quick-enable.
    "quick-enable-cf",
    // Installation of Chrome Frame.
    "chrome-frame",
    "migrate-chrome-frame",
  };
  for (size_t i = 0; i < arraysize(kLegacySwitches); ++i) {
    if (cmd_line.HasSwitch(kLegacySwitches[i]))
      return true;
  }
  return false;
}

bool IsProcessorSupported() {
  return base::CPU().has_sse2();
}

ScopedTokenPrivilege::ScopedTokenPrivilege(const wchar_t* privilege_name)
    : is_enabled_(false) {
  HANDLE temp_handle;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &temp_handle)) {
    return;
  }
  token_.Set(temp_handle);

  LUID privilege_luid;
  if (!::LookupPrivilegeValue(NULL, privilege_name, &privilege_luid)) {
    token_.Close();
    return;
  }

  // Adjust the token's privileges to enable |privilege_name|. If this privilege
  // was already enabled, |previous_privileges_|.PrivilegeCount will be set to 0
  // and we then know not to disable this privilege upon destruction.
  TOKEN_PRIVILEGES tp;
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Luid = privilege_luid;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  DWORD return_length;
  if (!::AdjustTokenPrivileges(token_.Get(), FALSE, &tp,
                               sizeof(TOKEN_PRIVILEGES),
                               &previous_privileges_, &return_length)) {
    token_.Close();
    return;
  }

  is_enabled_ = true;
}

ScopedTokenPrivilege::~ScopedTokenPrivilege() {
  if (is_enabled_ && previous_privileges_.PrivilegeCount != 0) {
    ::AdjustTokenPrivileges(token_.Get(), FALSE, &previous_privileges_,
                            sizeof(TOKEN_PRIVILEGES), NULL, NULL);
  }
}

}  // namespace installer
