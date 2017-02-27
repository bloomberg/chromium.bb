// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/installer/util/util_constants.h"

#if defined(OS_WIN)
#include <windows.h>  // NOLINT
#endif

class AppRegistrationData;

class BrowserDistribution {
 public:
  enum Subfolder {
    SUBFOLDER_CHROME,
    SUBFOLDER_APPS,
  };

  enum DefaultBrowserControlPolicy {
    DEFAULT_BROWSER_UNSUPPORTED,
    DEFAULT_BROWSER_OS_CONTROL_ONLY,
    DEFAULT_BROWSER_FULL_CONTROL
  };

  virtual ~BrowserDistribution();

  static BrowserDistribution* GetDistribution();

  // Getter and adaptors for the underlying |app_reg_data_|.
  const AppRegistrationData& GetAppRegistrationData() const;
  base::string16 GetStateKey() const;
  base::string16 GetStateMediumKey() const;
  base::string16 GetVersionKey() const;

  virtual void DoPostUninstallOperations(
      const base::Version& version,
      const base::FilePath& local_data_path,
      const base::string16& distribution_data);

  // Returns the GUID to be used when registering for Active Setup.
  virtual base::string16 GetActiveSetupGuid();

  // Returns the unsuffixed application name of this program.
  // This is the base of the name registered with Default Programs on Windows.
  // IMPORTANT: This should only be called by the installer which needs to make
  // decisions on the suffixing of the upcoming install, not by external callers
  // at run-time.
  virtual base::string16 GetBaseAppName();

  // Returns the localized display name of this distribution.
  virtual base::string16 GetDisplayName();

  // Returns the localized name of the Chrome shortcut for this distribution.
  virtual base::string16 GetShortcutName();

  // Returns the index of the Chrome icon for this distribution, inside the file
  // specified by GetIconFilename().
  virtual int GetIconIndex();

  // Returns the executable filename (not path) that contains the product icon.
  virtual base::string16 GetIconFilename();

  // Returns the localized name of the subfolder in the Start Menu identified by
  // |subfolder_type| that this distribution should create shortcuts in. For
  // SUBFOLDER_CHROME this returns GetShortcutName().
  virtual base::string16 GetStartMenuShortcutSubfolder(
      Subfolder subfolder_type);

  // Returns the unsuffixed appid of this program.
  // The AppUserModelId is a property of Windows programs.
  // IMPORTANT: This should only be called by ShellUtil::GetAppId as the appid
  // should be suffixed in all scenarios.
  virtual base::string16 GetBaseAppId();

  // Returns the Browser ProgId prefix (e.g. ChromeHTML, ChromiumHTM, etc...).
  // The full id is of the form |prefix|.|suffix| and is limited to a maximum
  // length of 39 characters including null-terminator.  See
  // http://msdn.microsoft.com/library/aa911706.aspx for details.  We define
  // |suffix| as a fixed-length 26-character alphanumeric identifier, therefore
  // the return value of this function must have a maximum length of
  // 39 - 1(null-term) - 26(|suffix|) - 1(dot separator) = 11 characters.
  virtual base::string16 GetBrowserProgIdPrefix();

  // Returns the Browser ProgId description.
  virtual base::string16 GetBrowserProgIdDesc();

  virtual base::string16 GetInstallSubDir();

  virtual base::string16 GetPublisherName();

  virtual base::string16 GetAppDescription();

  virtual base::string16 GetLongAppDescription();

  virtual std::string GetSafeBrowsingName();

#if defined(OS_WIN)
  virtual base::string16 GetDistributionData(HKEY root_key);
#endif

  // Returns the path "Software\<PRODUCT>". This subkey of HKEY_CURRENT_USER can
  // be used to save and restore state. With the exception of data that is used
  // by third parties (e.g., a subkey that specifies the location of a native
  // messaging host's manifest), state stored in this key is removed during
  // uninstall when the user chooses to also delete their browsing data.
  virtual base::string16 GetRegistryPath();

  virtual base::string16 GetUninstallRegPath();

  // Returns an enum specifying the different ways in which this distribution
  // is allowed to be set as default.
  virtual DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy();

  virtual bool CanCreateDesktopShortcuts();

  // Returns the CommandExecuteImpl class UUID (or empty string if this
  // distribution doesn't include a DelegateExecute verb handler).
  virtual base::string16 GetCommandExecuteImplClsid();

  virtual void UpdateInstallStatus(bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status);

  // Returns true if this distribution should set the Omaha experiment_labels
  // registry value.
  virtual bool ShouldSetExperimentLabels();

  virtual bool HasUserExperiments();

 protected:
  explicit BrowserDistribution(
      std::unique_ptr<AppRegistrationData> app_reg_data);

  template<class DistributionClass>
  static BrowserDistribution* GetOrCreateBrowserDistribution(
      BrowserDistribution** dist);

  std::unique_ptr<AppRegistrationData> app_reg_data_;

 private:
  BrowserDistribution();

  DISALLOW_COPY_AND_ASSIGN(BrowserDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
