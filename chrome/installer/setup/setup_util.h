// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

class CommandLine;
class Version;

namespace base {
class FilePath;
}

namespace installer {

class InstallationState;
class InstallerState;
class ProductState;

// Apply a diff patch to source file. First tries to apply it using courgette
// since it checks for courgette header and fails quickly. If that fails
// tries to apply the patch using regular bsdiff. Returns status code.
// The installer stage is updated if |installer_state| is non-NULL.
int ApplyDiffPatch(const base::FilePath& src,
                   const base::FilePath& patch,
                   const base::FilePath& dest,
                   const InstallerState* installer_state);

// Find the version of Chrome from an install source directory.
// Chrome_path should contain at least one version folder.
// Returns the maximum version found or NULL if no version is found.
Version* GetMaxVersionFromArchiveDir(const base::FilePath& chrome_path);

// Spawns a new process that waits for a specified amount of time before
// attempting to delete |path|.  This is useful for setup to delete the
// currently running executable or a file that we cannot close right away but
// estimate that it will be possible after some period of time.
// Returns true if a new process was started, false otherwise.  Note that
// given the nature of this function, it is not possible to know if the
// delete operation itself succeeded.
bool DeleteFileFromTempProcess(const base::FilePath& path,
                               uint32 delay_before_delete_ms);

// Returns true and populates |setup_exe| with the path to an existing product
// installer if one is found that is newer than the currently running installer
// (|installer_version|).
bool GetExistingHigherInstaller(const InstallationState& original_state,
                                bool system_install,
                                const Version& installer_version,
                                base::FilePath* setup_exe);

// Invokes the pre-existing |setup_exe| to handle the current operation (as
// dictated by |command_line|). An installerdata file, if specified, is first
// unconditionally copied into place so that it will be in effect in case the
// invoked |setup_exe| runs the newly installed product prior to exiting.
// Returns true if |setup_exe| was launched, false otherwise.
bool DeferToExistingInstall(const base::FilePath& setup_exe,
                            const CommandLine& command_line,
                            const InstallerState& installer_state,
                            const base::FilePath& temp_path,
                            InstallStatus* install_status);

// Returns true if the product |type| will be installed after the current
// setup.exe instance have carried out installation / uninstallation, at
// the level specified by |installer_state|.
// This function only returns meaningful results for install and update
// operations if called after CheckPreInstallConditions (see setup_main.cc).
bool WillProductBePresentAfterSetup(
    const installer::InstallerState& installer_state,
    const installer::InstallationState& machine_state,
    BrowserDistribution::Type type);

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
