// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/version.h"
#include "chrome/installer/util/util_constants.h"

#if defined(OS_WIN)
#include <windows.h>  // NOLINT
#endif

class CommandLine;

namespace installer {
class Product;
}
namespace installer {
class MasterPreferences;
}

class BrowserDistribution {
 public:
  virtual ~BrowserDistribution() {}

  enum Type {
    CHROME_BROWSER,
    CHROME_FRAME,
  };

  static BrowserDistribution* GetDistribution();

  static BrowserDistribution* GetSpecificDistribution(
      Type type, const installer::MasterPreferences& prefs);

  Type GetType() const { return type_; }

  virtual void DoPostUninstallOperations(const Version& version,
                                         const FilePath& local_data_path,
                                         const std::wstring& distribution_data);

  virtual std::wstring GetAppGuid();

  virtual std::wstring GetApplicationName();

  virtual std::wstring GetAppShortCutName();

  virtual std::wstring GetAlternateApplicationName();

  virtual std::wstring GetBrowserAppId();

  virtual std::wstring GetInstallSubDir();

  virtual std::wstring GetPublisherName();

  virtual std::wstring GetAppDescription();

  virtual std::wstring GetLongAppDescription();

  virtual std::string GetSafeBrowsingName();

  virtual std::wstring GetStateKey();

  virtual std::wstring GetStateMediumKey();

  virtual std::wstring GetStatsServerURL();

#if defined(OS_WIN)
  virtual std::wstring GetDistributionData(HKEY root_key);
#endif

  virtual std::wstring GetUninstallLinkName();

  virtual std::wstring GetUninstallRegPath();

  virtual std::wstring GetVersionKey();

  virtual bool CanSetAsDefault();

  virtual int GetIconIndex();

  virtual bool GetChromeChannel(std::wstring* channel);

  virtual void UpdateDiffInstallStatus(bool system_install,
      bool incremental_install, installer::InstallStatus install_status);

  // After an install or upgrade the user might qualify to participate in an
  // experiment. This function determines if the user qualifies and if so it
  // sets the wheels in motion or in simple cases does the experiment itself.
  virtual void LaunchUserExperiment(installer::InstallStatus status,
      const Version& version, const installer::Product& installation,
      bool system_level);

  // The user has qualified for the inactive user toast experiment and this
  // function just performs it.
  virtual void InactiveUserToastExperiment(int flavor,
      const installer::Product& installation);

  // A key-file is a file such as a DLL on Windows that is expected to be
  // in use when the product is being used.  For example "chrome.dll" for
  // Chrome.  Before attempting to delete an installation directory during
  // an uninstallation, the uninstaller will check if any one of a potential
  // set of key files is in use and if they are, abort the delete operation.
  // Only if none of the key files are in use, can the folder be deleted.
  // Note that this function does not return a full path to the key file(s),
  // only (a) file name(s).
  virtual std::vector<FilePath> GetKeyFiles();

  // Returns the list of Com Dlls that this product cares about having
  // registered and unregistered. The list may be empty.
  virtual std::vector<FilePath> GetComDllList();

  // Given a command line, appends the set of uninstall flags the uninstaller
  // for this distribution will require.
  virtual void AppendUninstallCommandLineFlags(CommandLine* cmd_line);

  // Returns true if install should create an uninstallation entry in the
  // Add/Remove Programs dialog for this distribution.
  virtual bool ShouldCreateUninstallEntry();

 protected:
  explicit BrowserDistribution(const installer::MasterPreferences& prefs);

  template<class DistributionClass>
  static BrowserDistribution* GetOrCreateBrowserDistribution(
      const installer::MasterPreferences& prefs,
      BrowserDistribution** dist);

  Type type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
