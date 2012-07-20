// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/browser_distribution.h"

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

// Get the path to this distribution's Active Setup registry entries.
// e.g. Software\Microsoft\Active Setup\Installed Components\<dist_guid>
string16 GetActiveSetupPath(BrowserDistribution* dist);

// This class will enable the privilege defined by |privilege_name| on the
// current process' token. The privilege will be disabled upon the
// ScopedTokenPrivilege's destruction (unless it was already enabled when the
// ScopedTokenPrivilege object was constructed).
// Some privileges might require admin rights to be enabled (check is_enabled()
// to know whether |privilege_name| was successfully enabled).
class ScopedTokenPrivilege {
 public:
  explicit ScopedTokenPrivilege(const wchar_t* privilege_name);
  ~ScopedTokenPrivilege();

  // Always returns true unless the privilege could not be enabled.
  bool is_enabled() const { return is_enabled_; }

 private:
  // Always true unless the privilege could not be enabled.
  bool is_enabled_;

  // A scoped handle to the current process' token. This will be closed
  // preemptively should enabling the privilege fail in the constructor.
  base::win::ScopedHandle token_;

  // The previous state of the privilege this object is responsible for. As set
  // by AdjustTokenPrivileges() upon construction.
  TOKEN_PRIVILEGES previous_privileges_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedTokenPrivilege);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
