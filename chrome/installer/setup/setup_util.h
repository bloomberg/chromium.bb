// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#pragma once

#include "base/file_path.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/install_util.h"

namespace installer {

class InstallerState;

// Apply a diff patch to source file. First tries to apply it using courgette
// since it checks for courgette header and fails quickly. If that fails
// tries to apply the patch using regular bsdiff. Returns status code.
// The installer stage is updated if |installer_state| is non-NULL.
int ApplyDiffPatch(const FilePath& src,
                   const FilePath& patch,
                   const FilePath& dest,
                   const InstallerState* installer_state);

// Find the version of Chrome from an install source directory.
// Chrome_path should contain at least one version folder.
// Returns the maximum version found or NULL if no version is found.
Version* GetMaxVersionFromArchiveDir(const FilePath& chrome_path);

// Spawns a new process that waits for a specified amount of time before
// attempting to delete |path|.  This is useful for setup to delete the
// currently running executable or a file that we cannot close right away but
// estimate that it will be possible after some period of time.
// Returns true if a new process was started, false otherwise.  Note that
// given the nature of this function, it is not possible to know if the
// delete operation itself succeeded.
bool DeleteFileFromTempProcess(const FilePath& path,
                               uint32 delay_before_delete_ms);

// A predicate that compares the program portion of a command line with a given
// file path.  First, the file paths are compared directly.  If they do not
// match, the filesystem is consulted to determine if the paths reference the
// same file.
class ProgramCompare : public InstallUtil::RegistryValuePredicate {
 public:
  explicit ProgramCompare(const FilePath& path_to_match);
  virtual ~ProgramCompare();
  virtual bool Evaluate(const std::wstring& value) const OVERRIDE;

 protected:
  static bool OpenForInfo(const FilePath& path,
                          base::win::ScopedHandle* handle);
  static bool GetInfo(const base::win::ScopedHandle& handle,
                      BY_HANDLE_FILE_INFORMATION* info);

  FilePath path_to_match_;
  base::win::ScopedHandle file_handle_;
  BY_HANDLE_FILE_INFORMATION file_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProgramCompare);
};  // class ProgramCompare

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
