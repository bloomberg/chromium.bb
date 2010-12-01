// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/version.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace installer {
class Product;
}

class BrowserDistribution {
 public:
  virtual ~BrowserDistribution() {}

  typedef enum DistributionType {
    CHROME_BROWSER,
    CHROME_FRAME,
    CEEE,
  };

  static BrowserDistribution* GetDistribution();

  static BrowserDistribution* GetSpecificDistribution(DistributionType type);

  DistributionType GetType() const { return type_; }

  static int GetInstallReturnCode(installer_util::InstallStatus install_status);

  virtual void DoPostUninstallOperations(const installer::Version& version,
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

  virtual std::wstring GetEnvVersionKey();

  virtual bool CanSetAsDefault();

  virtual int GetIconIndex();

  virtual bool GetChromeChannel(std::wstring* channel);

  virtual void UpdateDiffInstallStatus(bool system_install,
      bool incremental_install, installer_util::InstallStatus install_status);

  // After an install or upgrade the user might qualify to participate in an
  // experiment. This function determines if the user qualifies and if so it
  // sets the wheels in motion or in simple cases does the experiment itself.
  virtual void LaunchUserExperiment(installer_util::InstallStatus status,
      const installer::Version& version,
      const installer::Product& installation,
      bool system_level);

  // The user has qualified for the inactive user toast experiment and this
  // function just performs it.
  virtual void InactiveUserToastExperiment(int flavor,
      const installer::Product& installation);

 protected:
  BrowserDistribution() : type_(CHROME_BROWSER) {}

  template<class DistributionClass>
  static BrowserDistribution* GetOrCreateBrowserDistribution(
      BrowserDistribution** dist);

  DistributionType type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
