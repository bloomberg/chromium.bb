// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares utility functions for mini installer tests.
// The reason for putting these functions in different class is to separate out
// the critical logic from utility methods.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_UTIL_H_
#define CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_UTIL_H_
#pragma once

#include <vector>

#include "base/process.h"

class FilePath;

// This class maintains all the utility methods that are needed by mini
// installer test class.
class MiniInstallerTestUtil {
 public:
  // This method will change the current directory to one level up and
  // return the new current path.
  static bool ChangeCurrentDirectory(FilePath* current_path);

  // Closes specified process.
  static void CloseProcesses(const std::wstring& executable_name);

  // Returns the directory containing exe_name.
  static FilePath GetFilePath(const wchar_t* exe_name);

  // This method will send enter key to window in the foreground.
  static void SendEnterKeyToWindow();

  // Verifies if the given process starts running.
  static void VerifyProcessLaunch(const wchar_t* process_name,
                                  bool expected_status);

  // Verifies if the given process stops running.
  static bool VerifyProcessClose(const wchar_t* process_name);

  // Waits on the given process name until it returns or until a timeout is
  // reached.
  static bool VerifyProcessHandleClosed(base::ProcessHandle handle);

 private:
  DISALLOW_COPY_AND_ASSIGN(MiniInstallerTestUtil);
};

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_UTIL_H_
