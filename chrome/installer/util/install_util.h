// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares utility functions for the installer. The original reason
// for putting these functions in installer\util library is so that we can
// separate out the critical logic and write unit tests for it.

#ifndef CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_
#define CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_

#include <tchar.h>
#include <windows.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

class Version;
class WorkItemList;

// This is a utility class that provides common installation related
// utility methods that can be used by installer and also unit tested
// independently.
class InstallUtil {
 public:
  // Get the path to this distribution's Active Setup registry entries.
  // e.g. Software\Microsoft\Active Setup\Installed Components\<dist_guid>
  static string16 GetActiveSetupPath(BrowserDistribution* dist);

  // Attempts to trigger the command that would be run by Active Setup for a
  // system-level Chrome. For use only when system-level Chrome is installed.
  static void TriggerActiveSetupCommand();

  // Launches given exe as admin on Vista.
  static bool ExecuteExeAsAdmin(const CommandLine& cmd, DWORD* exit_code);

  // Reads the uninstall command for Chromium from registry and returns it.
  // If system_install is true the command is read from HKLM, otherwise
  // from HKCU.
  static CommandLine GetChromeUninstallCmd(
      bool system_install,
      BrowserDistribution::Type distribution_type);

  // Find the version of Chrome installed on the system by checking the
  // Google Update registry key. Fills |version| with the version or a
  // default-constructed Version if no version is found.
  // system_install: if true, looks for version number under the HKLM root,
  //                 otherwise looks under the HKCU.
  static void GetChromeVersion(BrowserDistribution* dist,
                               bool system_install,
                               Version* version);

  // Find the last critical update (version) of Chrome. Fills |version| with the
  // version or a default-constructed Version if no version is found. A critical
  // update is a specially flagged version (by Google Update) that contains an
  // important security fix.
  // system_install: if true, looks for version number under the HKLM root,
  //                 otherwise looks under the HKCU.
  static void GetCriticalUpdateVersion(BrowserDistribution* dist,
                                       bool system_install,
                                       Version* version);

  // This function checks if the current OS is supported for Chromium.
  static bool IsOSSupported();

  // Adds work items to |install_list|, which should be a
  // NoRollbackWorkItemList, to set installer error information in the registry
  // for consumption by Google Update.  |state_key| must be the full path to an
  // app's ClientState key.  See InstallerState::WriteInstallerResult for more
  // details.
  static void AddInstallerResultItems(bool system_install,
                                      const string16& state_key,
                                      installer::InstallStatus status,
                                      int string_resource_id,
                                      const string16* const launch_cmd,
                                      WorkItemList* install_list);

  // Update the installer stage reported by Google Update.  |state_key_path|
  // should be obtained via the state_key method of an InstallerState instance
  // created before the machine state is modified by the installer.
  static void UpdateInstallerStage(bool system_install,
                                   const string16& state_key_path,
                                   installer::InstallerStage stage);

  // Returns true if this installation path is per user, otherwise returns
  // false (per machine install, meaning: the exe_path contains path to
  // Program Files).
  static bool IsPerUserInstall(const wchar_t* const exe_path);

  // Returns true if the installation represented by the pair of |dist| and
  // |system_level| is a multi install.
  static bool IsMultiInstall(BrowserDistribution* dist, bool system_install);

  // Returns true if this is running setup process for Chrome SxS (as
  // indicated by the presence of --chrome-sxs on the command line) or if this
  // is running Chrome process from the Chrome SxS installation (as indicated
  // by either --chrome-sxs or the executable path).
  static bool IsChromeSxSProcess();

  // Populates |path| with the path to |file| in the sentinel directory. This is
  // the application directory for user-level installs, and the default user
  // data dir for system-level installs. Returns false on error.
  static bool GetSentinelFilePath(const FilePath::CharType* file,
                                  BrowserDistribution* dist,
                                  FilePath* path);

  // Deletes the registry key at path key_path under the key given by root_key.
  static bool DeleteRegistryKey(HKEY root_key, const string16& key_path);

  // Deletes the registry value named value_name at path key_path under the key
  // given by reg_root.
  static bool DeleteRegistryValue(HKEY reg_root, const string16& key_path,
                                  const string16& value_name);

  // An interface to a predicate function for use by DeleteRegistryKeyIf and
  // DeleteRegistryValueIf.
  class RegistryValuePredicate {
   public:
    virtual ~RegistryValuePredicate() { }
    virtual bool Evaluate(const string16& value) const = 0;
  };

  // The result of a conditional delete operation (i.e., DeleteFOOIf).
  enum ConditionalDeleteResult {
    NOT_FOUND,      // The condition was not satisfied.
    DELETED,        // The condition was satisfied and the delete succeeded.
    DELETE_FAILED   // The condition was satisfied but the delete failed.
  };

  // Deletes the key |key_to_delete_path| under |root_key| iff the value
  // |value_name| in the key |key_to_test_path| under |root_key| satisfies
  // |predicate|.  |value_name| may be either NULL or an empty string to test
  // the key's default value.
  static ConditionalDeleteResult DeleteRegistryKeyIf(
      HKEY root_key,
      const string16& key_to_delete_path,
      const string16& key_to_test_path,
      const wchar_t* value_name,
      const RegistryValuePredicate& predicate);

  // Deletes the value |value_name| in the key |key_path| under |root_key| iff
  // its current value satisfies |predicate|.  |value_name| may be either NULL
  // or an empty string to test/delete the key's default value.
  static ConditionalDeleteResult DeleteRegistryValueIf(
      HKEY root_key,
      const wchar_t* key_path,
      const wchar_t* value_name,
      const RegistryValuePredicate& predicate);

  // A predicate that performs a case-sensitive string comparison.
  class ValueEquals : public RegistryValuePredicate {
   public:
    explicit ValueEquals(const string16& value_to_match)
        : value_to_match_(value_to_match) { }
    virtual bool Evaluate(const string16& value) const OVERRIDE;
   protected:
    string16 value_to_match_;
   private:
    DISALLOW_COPY_AND_ASSIGN(ValueEquals);
  };

  // Returns zero on install success, or an InstallStatus value otherwise.
  static int GetInstallReturnCode(installer::InstallStatus install_status);

  // Composes |program| and |arguments| into |command_line|.
  static void MakeUninstallCommand(const string16& program,
                                   const string16& arguments,
                                   CommandLine* command_line);

  // Returns a string in the form YYYYMMDD of the current date.
  static string16 GetCurrentDate();

  // A predicate that compares the program portion of a command line with a
  // given file path.  First, the file paths are compared directly.  If they do
  // not match, the filesystem is consulted to determine if the paths reference
  // the same file.
  class ProgramCompare : public RegistryValuePredicate {
   public:
    explicit ProgramCompare(const FilePath& path_to_match);
    virtual ~ProgramCompare();
    virtual bool Evaluate(const string16& value) const OVERRIDE;
    bool EvaluatePath(const FilePath& path) const;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallUtil);
};


#endif  // CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_
