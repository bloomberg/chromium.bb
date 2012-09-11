// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#include "chrome/installer/setup/setup_util.h"

#include <windows.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/util_constants.h"
#include "courgette/courgette.h"
#include "third_party/bspatch/mbspatch.h"

namespace installer {

int ApplyDiffPatch(const FilePath& src,
                   const FilePath& patch,
                   const FilePath& dest,
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

Version* GetMaxVersionFromArchiveDir(const FilePath& chrome_path) {
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

bool DeleteFileFromTempProcess(const FilePath& path,
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

string16 GetActiveSetupPath(BrowserDistribution* dist) {
  static const wchar_t kInstalledComponentsPath[] =
      L"Software\\Microsoft\\Active Setup\\Installed Components\\";
  return kInstalledComponentsPath + dist->GetAppGuid();
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
