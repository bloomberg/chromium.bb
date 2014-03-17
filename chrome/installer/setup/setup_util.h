// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project. It also declares a
// few functions that the Chrome component updater uses for patching binary
// deltas.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class FilePath;
class Version;
}

namespace installer {

class InstallationState;
class InstallerState;
class ProductState;

// Applies a patch file to source file using Courgette. Returns 0 in case of
// success. In case of errors, it returns kCourgetteErrorOffset + a Courgette
// status code, as defined in courgette/courgette.h
int CourgettePatchFiles(const base::FilePath& src,
                        const base::FilePath& patch,
                        const base::FilePath& dest);

// Applies a patch file to source file using bsdiff. This function uses
// Courgette's flavor of bsdiff. Returns 0 in case of success, or
// kBsdiffErrorOffset + a bsdiff status code in case of errors.
// See courgette/third_party/bsdiff.h for details.
int BsdiffPatchFiles(const base::FilePath& src,
                     const base::FilePath& patch,
                     const base::FilePath& dest);

// Find the version of Chrome from an install source directory.
// Chrome_path should contain at least one version folder.
// Returns the maximum version found or NULL if no version is found.
Version* GetMaxVersionFromArchiveDir(const base::FilePath& chrome_path);

// Returns the uncompressed archive of the installed version that serves as the
// source for patching.
base::FilePath FindArchiveToPatch(const InstallationState& original_state,
                                  const InstallerState& installer_state);

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
                                const base::Version& installer_version,
                                base::FilePath* setup_exe);

// Invokes the pre-existing |setup_exe| to handle the current operation (as
// dictated by |command_line|). An installerdata file, if specified, is first
// unconditionally copied into place so that it will be in effect in case the
// invoked |setup_exe| runs the newly installed product prior to exiting.
// Returns true if |setup_exe| was launched, false otherwise.
bool DeferToExistingInstall(const base::FilePath& setup_exe,
                            const base::CommandLine& command_line,
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

// Drops the process down to background processing mode on supported OSes if it
// was launched below the normal process priority. Returns true when background
// procesing mode is entered.
bool AdjustProcessPriority();

// Makes registry adjustments to migrate the Google Update state of |to_migrate|
// from multi-install to single-install. This includes copying the usagestats
// value and adjusting the ap values of all multi-install products.
void MigrateGoogleUpdateStateMultiToSingle(
    bool system_level,
    BrowserDistribution::Type to_migrate,
    const installer::InstallationState& machine_state);

// Returns true if |install_status| represents a successful uninstall code.
bool IsUninstallSuccess(InstallStatus install_status);

// Returns true if |cmd_line| contains unsupported (legacy) switches.
bool ContainsUnsupportedSwitch(const base::CommandLine& cmd_line);

// Returns true if the processor is supported by chrome.
bool IsProcessorSupported();

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
