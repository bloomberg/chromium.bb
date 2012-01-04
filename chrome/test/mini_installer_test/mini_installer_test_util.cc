// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

using base::TimeDelta;

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
  TimeDelta timer;
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(200);
  while ((base::GetProcessCount(executable_name, NULL) > 0) &&
         (timer < TimeDelta::FromSeconds(20))) {
    base::KillProcesses(executable_name, 1, NULL);
    base::PlatformThread::Sleep(kDelay);
    timer += kDelay;
  }
  ASSERT_EQ(0, base::GetProcessCount(executable_name, NULL));
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
  LOG(INFO) << "Verifying process is launched: " << process_name;
  TimeDelta timer;
  TimeDelta waitTime = TimeDelta::FromMinutes(1);
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(200);
  if (!expected_status)
    waitTime = TimeDelta::FromSeconds(8);

  while ((base::GetProcessCount(process_name, NULL) == 0) &&
         (timer < waitTime)) {
    base::PlatformThread::Sleep(kDelay);
    timer += kDelay;
  }

  if (expected_status)
    ASSERT_NE(0, base::GetProcessCount(process_name, NULL));
  else
    ASSERT_EQ(0, base::GetProcessCount(process_name, NULL));
}

bool MiniInstallerTestUtil::VerifyProcessClose(
    const wchar_t* process_name) {
  TimeDelta timer;
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(200);
  if (base::GetProcessCount(process_name, NULL) > 0) {
    LOG(INFO) << "Waiting for this process to end: " << process_name;
    while ((base::GetProcessCount(process_name, NULL) > 0) &&
           (timer < TimeDelta::FromMilliseconds(
               TestTimeouts::large_test_timeout_ms()))) {
      base::PlatformThread::Sleep(kDelay);
      timer += kDelay;
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
