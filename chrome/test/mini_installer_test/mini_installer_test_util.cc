// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/mini_installer_test_util.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// Change current directory so that chrome.dll from current folder
// will not be used as fall back.
bool MiniInstallerTestUtil::ChangeCurrentDirectory(std::wstring *current_path) {
  wchar_t backup_path[MAX_PATH];
  DWORD ret = ::GetCurrentDirectory(MAX_PATH, backup_path);
  if (ret == 0 && ret > MAX_PATH)
    return false;
  current_path->assign(backup_path);
  file_util::UpOneDirectory(current_path);
  ::SetCurrentDirectory(current_path->c_str());
  current_path->assign(backup_path);
  return true;
}

// Checks for all requested running processes and kills them.
void MiniInstallerTestUtil::CloseProcesses(
    const std::wstring& executable_name) {
  int timer = 0;
  while ((base::GetProcessCount(executable_name, NULL) > 0) &&
         (timer < 20000)) {
    base::KillProcesses(executable_name, 1, NULL);
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  ASSERT_EQ(0, base::GetProcessCount(executable_name, NULL));
}

bool MiniInstallerTestUtil::CloseWindow(const wchar_t* window_name,
                                        UINT message) {
  int timer = 0;
  bool return_val = false;
  HWND hndl = FindWindow(NULL, window_name);
  while (hndl == NULL && (timer < 60000)) {
    hndl = FindWindow(NULL, window_name);
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  if (hndl != NULL) {
    LRESULT _result = SendMessage(hndl, message, 1, 0);
    return_val = true;
  }
  return return_val;
}

bool IsNewer(const FileInfo& file_rbegin, const FileInfo& file_rend) {
  return (file_rbegin.creation_time_ > file_rend.creation_time_);
}

bool MiniInstallerTestUtil::GetCommandForTagging(std::wstring *return_command) {
  FileInfoList file_details;
  MiniInstallerTestUtil::GetStandaloneInstallerFileName(&file_details);
  if (file_details.empty())
    return false;
  if (file_details.at(0).name_.empty())
    return false;
  std::wstring standalone_installer_path;
  standalone_installer_path.assign(
      mini_installer_constants::kChromeStandAloneInstallerLocation);
  standalone_installer_path.append(file_details.at(0).name_);
  return_command->append(mini_installer_constants::kChromeApplyTagExe);
  return_command->append(L" ");
  return_command->append(standalone_installer_path);
  return_command->append(L" ");
  return_command->append(mini_installer_constants::kStandaloneInstaller);
  return_command->append(L" ");
  return_command->append(mini_installer_constants::kChromeApplyTagParameters);
  LOG(INFO) << "Command to run Apply tag: " << return_command->c_str();
  return true;
}

std::wstring MiniInstallerTestUtil::GetFilePath(const wchar_t* exe_name) {
  FilePath installer_path;
  PathService::Get(base::DIR_EXE, &installer_path);
  installer_path = installer_path.Append(exe_name);
  LOG(INFO) << "Chrome exe path: " << installer_path.value().c_str();
  return installer_path.ToWStringHack();
}

// This method will first call GetLatestFile to get the list of all
// builds, sorted on creation time. Then goes through each build folder
// until it finds the installer file that matches the pattern argument.
bool MiniInstallerTestUtil::GetInstaller(const wchar_t* pattern,
    std::wstring *path, const wchar_t* channel_type) {
  FileInfoList builds_list;
  FileInfoList exe_list;
  std::wstring chrome_diff_installer(
      mini_installer_constants::kChromeDiffInstallerLocation);
  chrome_diff_installer.append(L"*");
  if (!GetLatestFile(chrome_diff_installer.c_str(),
                     channel_type, &builds_list))
    return false;
  FileInfoList::const_reverse_iterator builds_list_size = builds_list.rbegin();
  while (builds_list_size != builds_list.rend()) {
    path->assign(mini_installer_constants::kChromeDiffInstallerLocation);
    file_util::AppendToPath(path, builds_list_size->name_);
    if (channel_type == mini_installer_constants::kDevChannelBuild)
      file_util::AppendToPath(path, L"win");
    std::wstring installer_path(path->c_str());
    file_util::AppendToPath(&installer_path, L"*.exe");
    if (!GetLatestFile(installer_path.c_str(), pattern, &exe_list)) {
      ++builds_list_size;
    } else {
      file_util::AppendToPath(path, exe_list.at(0).name_.c_str());
      if (!file_util::PathExists(*path)) {
        ++builds_list_size;
      } else {
        break;
      }
    }
  }
  return (file_util::PathExists(path->c_str()));
}

// This method will get the latest installer filename from the directory.
bool MiniInstallerTestUtil::GetLatestFile(const wchar_t* file_name,
    const wchar_t* pattern, FileInfoList *file_details) {
  WIN32_FIND_DATA find_file_data;
  HANDLE file_handle = FindFirstFile(file_name, &find_file_data);
  if (file_handle == INVALID_HANDLE_VALUE) {
    LOG(INFO) << "Handle is invalid.";
    return false;
  }
  BOOL ret = TRUE;
  bool return_val = false;
  while (ret) {
    std::wstring search_path = find_file_data.cFileName;
    size_t position_found = search_path.find(pattern);
    if (position_found != -1) {
      std::wstring extension = file_util::GetFileExtensionFromPath(file_name);
      if ((base::strcasecmp(WideToUTF8(extension).c_str(), "exe")) == 0) {
        file_details->push_back(FileInfo(find_file_data.cFileName, 0));
        return_val = true;
        break;
      } else {
        FILETIME file_time = find_file_data.ftCreationTime;
        base::Time creation_time = base::Time::FromFileTime(file_time);
        file_details->push_back(FileInfo(find_file_data.cFileName,
                                static_cast<int>(creation_time.ToDoubleT())));
        return_val = true;
      }
    }
    ret = FindNextFile(file_handle, &find_file_data);
  }
  std::sort(file_details->rbegin(), file_details->rend(), &IsNewer);
  FindClose(file_handle);
  return return_val;
}

void MiniInstallerTestUtil::GetPreviousBuildNumber(const std::wstring& path,
    std::wstring *build_number, const wchar_t* channel_type) {
  std::wstring diff_installer_name = file_util::GetFilenameFromPath(path);
  std::wstring::size_type start_position = diff_installer_name.find(L"f");
  std::wstring::size_type end_position = diff_installer_name.find(L"_c");
  end_position = end_position - start_position;
  std::wstring file_name = diff_installer_name.substr(start_position,
                                                      end_position);
  file_name = file_name.substr(file_name.find(L'_')+1, file_name.size());
  file_name = channel_type + file_name;
  build_number->assign(file_name);
  LOG(INFO) << "Previous build number: " << file_name.c_str();
}

// This method will get the previous full installer path
// from given diff installer path. It will first get the
// filename from the diff installer path, gets the previous
// build information from the filename, then computes the
// path for previous full installer.
bool MiniInstallerTestUtil::GetPreviousFullInstaller(
    const std::wstring& diff_file_name, std::wstring *previous,
    const wchar_t* channel_type) {
  std::wstring diff_file = diff_file_name;
  std::wstring build_number;
  GetPreviousBuildNumber(diff_file, &build_number, channel_type);
  file_util::UpOneDirectory(&diff_file);
  file_util::UpOneDirectory(&diff_file);
  if (channel_type == mini_installer_constants::kDevChannelBuild)
    file_util::UpOneDirectory(&diff_file);
  file_util::AppendToPath(&diff_file, build_number.c_str());
  if (channel_type == mini_installer_constants::kDevChannelBuild)
    file_util::AppendToPath(&diff_file, L"win");
  previous->assign(diff_file);
  file_util::AppendToPath(&diff_file, L"*.exe");
  FileInfoList directory_list;
  if (!GetLatestFile(diff_file.c_str(),
                     mini_installer_constants::kFullInstallerPattern,
                     &directory_list))
    return false;
  file_util::AppendToPath(previous, directory_list.at(0).name_);
  return (file_util::PathExists(previous->c_str()));
}

bool MiniInstallerTestUtil::GetStandaloneInstallerFileName(
    FileInfoList *file_name) {
  std::wstring standalone_installer(
      mini_installer_constants::kChromeStandAloneInstallerLocation);
  standalone_installer.append(L"*.exe");
  return (GetLatestFile(standalone_installer.c_str(),
                        mini_installer_constants::kUntaggedInstallerPattern,
                        file_name));
}

bool MiniInstallerTestUtil::GetStandaloneVersion(
    std::wstring* return_file_name) {
  FileInfoList file_details;
  GetStandaloneInstallerFileName(&file_details);
  std::wstring file_name = file_details.at(0).name_;
  // Returned file name will have following convention:
  // ChromeStandaloneSetup_<build>_<patch>.exe
  // Following code will extract build, patch details from the file
  // and concatenate with 1.0 to form the build version.
  // Patteren followed: 1.0.<build>.<patch>htt
  file_name = file_name.substr(22, 25);
  std::wstring::size_type last_dot = file_name.find(L'.');
  file_name = file_name.substr(0, last_dot);
  std::wstring::size_type pos = file_name.find(L'_');
  file_name.replace(pos, 1, L".");
  file_name = L"2.0." + file_name;
  return_file_name->assign(file_name.c_str());
  LOG(INFO) << "Standalone installer version: " << file_name.c_str();
  return true;
}

void MiniInstallerTestUtil::SendEnterKeyToWindow() {
  INPUT key;
  key.type = INPUT_KEYBOARD;
  key.ki.wVk = VK_RETURN;
  key.ki.dwFlags = 0;
  key.ki.time = 0;
  key.ki.wScan = 0;
  key.ki.dwExtraInfo = 0;
  SendInput(1, &key, sizeof(INPUT));
  key.ki.dwExtraInfo = KEYEVENTF_KEYUP;
  SendInput(1, &key, sizeof(INPUT));
}

void MiniInstallerTestUtil::VerifyProcessLaunch(
    const wchar_t* process_name, bool expected_status) {
  int timer = 0, wait_time = 60000;
  if (!expected_status)
    wait_time = 8000;
  while ((base::GetProcessCount(process_name, NULL) == 0) &&
         (timer < wait_time)) {
    PlatformThread::Sleep(200);
    timer = timer + 200;
  }
  if (expected_status)
    ASSERT_NE(0, base::GetProcessCount(process_name, NULL));
  else
    ASSERT_EQ(0, base::GetProcessCount(process_name, NULL));
}

bool MiniInstallerTestUtil::VerifyProcessClose(
    const wchar_t* process_name) {
  int timer = 0;
  if (base::GetProcessCount(process_name, NULL) > 0) {
    LOG(INFO) << "Waiting for this process to end: " << process_name;
    while ((base::GetProcessCount(process_name, NULL) > 0) &&
           (timer < 60000)) {
      PlatformThread::Sleep(200);
      timer = timer + 200;
    }
  } else {
  if (base::GetProcessCount(process_name, NULL) != 0)
      return false;
  }
  return true;
}
