// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#include "chrome/installer/setup/setup_util.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/version.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "courgette/courgette.h"
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

int ApplyDiffPatch(const base::FilePath& src,
                   const base::FilePath& patch,
                   const base::FilePath& dest,
                   const InstallerState* installer_state) {
  VLOG(1) << "Applying patch " << patch.value() << " to file " << src.value()
          << " and generating file " << dest.value();

  if (installer_state != NULL)
    installer_state->UpdateStage(installer::ENSEMBLE_PATCHING);

  // Try Courgette first.  Courgette checks the patch file first and fails
  // quickly if the patch file does not have a valid Courgette header.
  courgette::Status patch_status =
      courgette::ApplyEnsemblePatch(src.value().c_str(),
                                    patch.value().c_str(),
                                    dest.value().c_str());
  if (patch_status == courgette::C_OK)
    return 0;

  VLOG(1) << "Failed to apply patch " << patch.value()
          << " using courgette. err=" << patch_status;

  // If we ran out of memory or disk space, then these are likely the errors
  // we will see.  If we run into them, return an error and stay on the
  // 'ENSEMBLE_PATCHING' update stage.
  if (patch_status == courgette::C_DISASSEMBLY_FAILED ||
      patch_status == courgette::C_STREAM_ERROR) {
    return MEM_ERROR;
  }

  if (installer_state != NULL)
    installer_state->UpdateStage(installer::BINARY_PATCHING);

  return ApplyBinaryPatch(src.value().c_str(), patch.value().c_str(),
                          dest.value().c_str());
}

Version* GetMaxVersionFromArchiveDir(const base::FilePath& chrome_path) {
  VLOG(1) << "Looking for Chrome version folder under " << chrome_path.value();
  Version* version = NULL;
  file_util::FileEnumerator version_enum(chrome_path, false,
      file_util::FileEnumerator::DIRECTORIES);
  // TODO(tommi): The version directory really should match the version of
  // setup.exe.  To begin with, we should at least DCHECK that that's true.

  scoped_ptr<Version> max_version(new Version("0.0.0.0"));
  bool version_found = false;

  while (!version_enum.Next().empty()) {
    file_util::FileEnumerator::FindInfo find_data = {0};
    version_enum.GetFindInfo(&find_data);
    VLOG(1) << "directory found: " << find_data.cFileName;

    scoped_ptr<Version> found_version(
        new Version(WideToASCII(find_data.cFileName)));
    if (found_version->IsValid() &&
        found_version->CompareTo(*max_version.get()) > 0) {
      max_version.reset(found_version.release());
      version_found = true;
    }
  }

  return (version_found ? max_version.release() : NULL);
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
      ::TerminateProcess(pi.hProcess, ~0);
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
  bool is_present = false;
  if (product_state != NULL) {
    if (type == BrowserDistribution::CHROME_FRAME) {
      is_present = !product_state->uninstall_command().HasSwitch(
                        switches::kChromeFrameReadyMode);
    } else {
      is_present = true;
    }
  }

  bool is_uninstall = installer_state.operation() == InstallerState::UNINSTALL;

  // Determine if current operation affects the product.
  bool is_affected = false;
  const Product* product = installer_state.FindProduct(type);
  if (product != NULL) {
    if (type == BrowserDistribution::CHROME_FRAME) {
      // If Chrome Frame is being uninstalled, we don't bother to check
      // !HasOption(kOptionReadyMode) since CF would not have been installed
      // in the first place. If for some odd reason it weren't, we would be
      // conservative, and cause false to be retruned since CF should not be
      // installed then (so is_uninstall = true and is_affected = true).
      is_affected = is_uninstall || !product->HasOption(kOptionReadyMode);
    } else {
      is_affected = true;
    }
  }

  // Decide among {(1),(2),(3),(4)}.
  return is_affected ? !is_uninstall : is_present;
}

ScopedTokenPrivilege::ScopedTokenPrivilege(const wchar_t* privilege_name)
    : is_enabled_(false) {
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          token_.Receive())) {
    return;
  }

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
  if (!::AdjustTokenPrivileges(token_, FALSE, &tp, sizeof(TOKEN_PRIVILEGES),
                               &previous_privileges_, &return_length)) {
    token_.Close();
    return;
  }

  is_enabled_ = true;
}

ScopedTokenPrivilege::~ScopedTokenPrivilege() {
  if (is_enabled_ && previous_privileges_.PrivilegeCount != 0) {
    ::AdjustTokenPrivileges(token_, FALSE, &previous_privileges_,
                            sizeof(TOKEN_PRIVILEGES), NULL, NULL);
  }
}

}  // namespace installer
