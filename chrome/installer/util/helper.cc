// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"
#include "chrome/installer/util/work_item_list.h"

namespace {

FilePath GetChromeInstallBasePath(bool system_install,
                                  const wchar_t* subpath) {
  FilePath install_path;
  if (system_install) {
    PathService::Get(base::DIR_PROGRAM_FILES, &install_path);
  } else {
    PathService::Get(base::DIR_LOCAL_APP_DATA, &install_path);
  }
  if (!install_path.empty()) {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    install_path = install_path.Append(dist->GetInstallSubDir());
    install_path = install_path.Append(subpath);
  }
  return install_path;
}

}  // namespace

FilePath installer::GetChromeInstallPath(bool system_install) {
  return GetChromeInstallBasePath(system_install,
                                  installer_util::kInstallBinaryDir);
}

FilePath installer::GetChromeUserDataPath() {
  return GetChromeInstallBasePath(false, installer_util::kInstallUserDataDir);
}

bool installer::LaunchChrome(bool system_install) {
  FilePath chrome_exe(FILE_PATH_LITERAL("\""));
  chrome_exe = chrome_exe.Append(installer::GetChromeInstallPath(
      system_install));
  chrome_exe = chrome_exe.Append(installer_util::kChromeExe);
  chrome_exe = chrome_exe.Append(FILE_PATH_LITERAL("\""));
  return base::LaunchApp(chrome_exe.value(), false, false, NULL);
}

bool installer::LaunchChromeAndWaitForResult(bool system_install,
                                             const std::wstring& options,
                                             int32* exit_code) {
  FilePath chrome_exe(installer::GetChromeInstallPath(system_install));
  if (chrome_exe.empty())
    return false;
  chrome_exe = chrome_exe.Append(installer_util::kChromeExe);

  std::wstring command_line(L"\"" + chrome_exe.value() + L"\"");
  command_line.append(options);
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcessW(chrome_exe.value().c_str(),
                        const_cast<wchar_t*>(command_line.c_str()),
                        NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                        &si, &pi)) {
    return false;
  }

  DWORD wr = ::WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD ret;
  if (::GetExitCodeProcess(pi.hProcess, &ret) == 0)
    return false;

  if (exit_code)
    *exit_code = ret;

  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);
  return true;
}

void installer::RemoveOldVersionDirs(const FilePath& chrome_path,
                                     const std::wstring& latest_version_str) {
  FilePath search_path(chrome_path.AppendASCII("*"));

  WIN32_FIND_DATA find_file_data;
  HANDLE file_handle = FindFirstFile(search_path.value().c_str(),
                                     &find_file_data);
  if (file_handle == INVALID_HANDLE_VALUE)
    return;

  BOOL ret = TRUE;
  scoped_ptr<installer::Version> version;
  scoped_ptr<installer::Version> latest_version(
      installer::Version::GetVersionFromString(latest_version_str));

  // We try to delete all directories whose versions are lower than
  // latest_version.
  while (ret) {
    if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      LOG(INFO) << "directory found: " << find_file_data.cFileName;
      version.reset(
          installer::Version::GetVersionFromString(find_file_data.cFileName));
      if (version.get() && latest_version->IsHigherThan(version.get())) {
        FilePath remove_dir(chrome_path.Append(find_file_data.cFileName));
        FilePath chrome_dll_path(remove_dir.Append(installer_util::kChromeDll));
        LOG(INFO) << "deleting directory " << remove_dir.value();
        scoped_ptr<DeleteTreeWorkItem> item;
        item.reset(WorkItem::CreateDeleteTreeWorkItem(remove_dir,
                                                      chrome_dll_path));
        item->Do();
      }
    }
    ret = FindNextFile(file_handle, &find_file_data);
  }

  FindClose(file_handle);
}
