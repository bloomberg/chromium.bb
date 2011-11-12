// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace installer {
class Product;
}

class BrowserDistribution {
 public:
  enum Type {
    CHROME_BROWSER,
    CHROME_FRAME,
    CHROME_BINARIES,
    NUM_TYPES
  };

  // A struct for communicating what a UserExperiment contains. In these
  // experiments we show toasts to the user if they are inactive for a certain
  // amount of time.
  struct UserExperiment {
    std::wstring prefix;  // The experiment code prefix for this experiment,
                          // also known as the 'TV' part in 'TV80'.
    int flavor;           // The flavor index for this experiment.
    int heading;          // The heading resource ID to use for this experiment.
    int control_group;    // Size of the control group (in percentages). Control
                          // group is the group that qualifies for the
                          // experiment but does not participate.
  };

  // An array of the Types representing products;
  static const Type kProductTypes[];

  // The number of elements in the array |kProductTypes|.
  static const size_t kNumProductTypes;

  virtual ~BrowserDistribution() {}

  static BrowserDistribution* GetDistribution();

  static BrowserDistribution* GetSpecificDistribution(Type type);

  Type GetType() const { return type_; }

  virtual void DoPostUninstallOperations(const Version& version,
                                         const FilePath& local_data_path,
                                         const std::wstring& distribution_data);

  virtual std::wstring GetAppGuid();

  // Returns the name by which the program is registered with Default Programs.
  // This is not a localized string suitable for presenting to a user.
  virtual std::wstring GetApplicationName();

  // Returns the localized name of the program.
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

  virtual std::string GetNetworkStatsServer() const;

#if defined(OS_WIN)
  virtual std::wstring GetDistributionData(HKEY root_key);
#endif

  virtual std::wstring GetUninstallLinkName();

  virtual std::wstring GetUninstallRegPath();

  virtual std::wstring GetVersionKey();

  virtual bool CanSetAsDefault();

  virtual int GetIconIndex();

  virtual bool GetChromeChannel(std::wstring* channel);

  virtual void UpdateInstallStatus(bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status);

  // Gets the experiment details for a given language-brand combo. If |flavor|
  // is -1, then a flavor will be selected at random. |experiment| is the struct
  // you want to write the experiment information to. Returns false if no
  // experiment details could be gathered.
  virtual bool GetExperimentDetails(UserExperiment* experiment, int flavor);

  // After an install or upgrade the user might qualify to participate in an
  // experiment. This function determines if the user qualifies and if so it
  // sets the wheels in motion or in simple cases does the experiment itself.
  virtual void LaunchUserExperiment(const FilePath& setup_path,
                                    installer::InstallStatus status,
                                    const Version& version,
                                    const installer::Product& installation,
                                    bool system_level);

  // The user has qualified for the inactive user toast experiment and this
  // function just performs it.
  virtual void InactiveUserToastExperiment(int flavor,
      const std::wstring& experiment_group,
      const installer::Product& installation,
      const FilePath& application_path);

 protected:
  explicit BrowserDistribution(Type type);

  template<class DistributionClass>
  static BrowserDistribution* GetOrCreateBrowserDistribution(
      BrowserDistribution** dist);

  const Type type_;

 private:
  BrowserDistribution();

  DISALLOW_COPY_AND_ASSIGN(BrowserDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
