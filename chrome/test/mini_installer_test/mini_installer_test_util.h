// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares utility functions for mini installer tests.
// The reason for putting these functions in different class is to separate out
// the critical logic from utility methods.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_UTIL_H_
#define CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_UTIL_H_

#include <windows.h>
#include <vector>

#include "base/basictypes.h"

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
  static bool ChangeCurrentDirectory(std::wstring *current_path);

  // Closes specified process.
  static void CloseProcesses(const std::wstring& executable_name);

  // Close Window whose name is 'window_name', by sending Windows message
  // 'message' to it.
  static bool CloseWindow(const wchar_t* window_name, UINT message);

  // This method will get the latest installer based on the passed 'pattern' and
  // 'channel_type' arguments. The 'pattern' argument decides if the requested
  // installer is full or differential. The 'channel_type' parameter decides if
  // the build is stable/dev/beta.
  static bool GetInstaller(const wchar_t* pattern, std::wstring *name,
                           const wchar_t* channel_type);

  // This method will create a command line to run apply tag.
  static bool GetCommandForTagging(std::wstring *return_command);

  // Returns the directory containing exe_name.
  static std::wstring GetFilePath(const wchar_t* exe_name);


  // This method will get the list of all folders or files based on the passed
  // 'path' and 'pattern' argument. The 'pattern' argument decides if the
  // requested file is a full or a differential installer.
  static bool GetLatestFile(const wchar_t* path, const wchar_t* pattern,
                            FileInfoList *file_name);

  // This method retrieves the previous build version for the given diff
  // installer path.
  static bool GetPreviousBuildNumber(const std::wstring& path,
      std::wstring *build_number);

  // This method will get the previous full installer based on 'diff_file'
  // and 'channel_type' arguments. The 'channel_type'
  // parameter decides if the build is stable/dev/beta. The 'diff_file'
  // parameter will hold the latest diff installer name.
  static bool GetPreviousFullInstaller(const std::wstring& diff_file,
      std::wstring *previous);

  // This method will return standalone installer file name.
  static bool GetStandaloneInstallerFileName(FileInfoList *file_name);

  // This method will get the version number from the filename.
  static bool GetStandaloneVersion(std::wstring* version);

  // This method will send enter key to window in the foreground.
  static void SendEnterKeyToWindow();

  // Verifies if the given process starts running.
  static void VerifyProcessLaunch(const wchar_t* process_name,
                                  bool expected_status);

  // Verifies if the given process stops running.
  static bool VerifyProcessClose(const wchar_t* process_name);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(MiniInstallerTestUtil);
};

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_MINI_INSTALLER_TEST_UTIL_H_
