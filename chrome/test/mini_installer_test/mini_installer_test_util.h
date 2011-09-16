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

#include "base/basictypes.h"
#include "base/process.h"

class FilePath;

// This structure holds the name and creation time
// details of all the chrome nightly builds.
class FileInfo {
 public:
  FileInfo() {}
  FileInfo(const std::wstring& in_name, int in_creation_time) {
    name_.assign(in_name);
    creation_time_ = in_creation_time;
  }
  // This is a predicate to sort file information.
  bool IsNewer(const FileInfo& creation_time_begin,
               const FileInfo& creation_time_end);

  std::wstring name_;
  int creation_time_;
};
typedef std::vector<FileInfo> FileInfoList;

// This class maintains all the utility methods that are needed by mini
// installer test class.
class MiniInstallerTestUtil {
 public:
  // This method will change the current directory to one level up and
  // return the new current path.
  static bool ChangeCurrentDirectory(FilePath* current_path);

  // Closes specified process.
  static void CloseProcesses(const std::wstring& executable_name);

  // Close Window whose name is 'window_name', by sending Windows message
  // 'message' to it.
  static bool CloseWindow(const wchar_t* window_name, UINT message);

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
