// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/test/mini_installer_test/mini_installer_test_util.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"


// Change current directory so that chrome.dll from current folder
// will not be used as fall back.
bool MiniInstallerTestUtil::ChangeCurrentDirectory(FilePath* current_path) {
  FilePath backup_path;
  if (!file_util::GetCurrentDirectory(&backup_path))
    return false;

  if (!file_util::SetCurrentDirectory(backup_path.DirName()))
    return false;
  *current_path = backup_path;
  return true;
}

// Checks for all requested running processes and kills them.
void MiniInstallerTestUtil::CloseProcesses(
    const std::wstring& executable_name) {
  int timer = 0;
  while ((base::GetProcessCount(executable_name, NULL) > 0) &&
         (timer < 20000)) {
    base::KillProcesses(executable_name, 1, NULL);
    base::PlatformThread::Sleep(200);
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
    base::PlatformThread::Sleep(200);
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

FilePath MiniInstallerTestUtil::GetFilePath(const wchar_t* exe_name) {
  FilePath installer_path;
  PathService::Get(base::DIR_EXE, &installer_path);
  installer_path = installer_path.Append(exe_name);
  return installer_path;
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
    base::PlatformThread::Sleep(200);
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
           (timer < TestTimeouts::large_test_timeout_ms())) {
      base::PlatformThread::Sleep(200);
      timer = timer + 200;
    }
  } else {
    if (base::GetProcessCount(process_name, NULL) != 0)
      return false;
  }
  return true;
}

bool MiniInstallerTestUtil::VerifyProcessHandleClosed(
    base::ProcessHandle handle) {
  DWORD result = WaitForSingleObject(handle,
      TestTimeouts::large_test_timeout_ms());
  return result == WAIT_OBJECT_0;
}
