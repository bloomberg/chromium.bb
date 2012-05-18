// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string16.h"
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
    string16 prefix;      // The experiment code prefix for this experiment,
                          // also known as the 'TV' part in 'TV80'.
    int flavor;           // The flavor index for this experiment.
    int heading;          // The heading resource ID to use for this experiment.
    bool compact_bubble;  // Whether to show the compact heading or not.
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
                                         const string16& distribution_data);

  virtual string16 GetAppGuid();

  // Returns the name by which the program is registered with Default Programs.
  // This is not a localized string suitable for presenting to a user.
  virtual string16 GetApplicationName();

  // Returns the localized name of the program.
  virtual string16 GetAppShortCutName();

  virtual string16 GetAlternateApplicationName();

  virtual string16 GetBrowserAppId();

  virtual string16 GetInstallSubDir();

  virtual string16 GetPublisherName();

  virtual string16 GetAppDescription();

  virtual string16 GetLongAppDescription();

  virtual std::string GetSafeBrowsingName();

  virtual string16 GetStateKey();

  virtual string16 GetStateMediumKey();

  virtual string16 GetStatsServerURL();

  virtual std::string GetNetworkStatsServer() const;

  virtual std::string GetHttpPipeliningTestServer() const;

#if defined(OS_WIN)
  virtual string16 GetDistributionData(HKEY root_key);
#endif

  virtual string16 GetUninstallLinkName();

  virtual string16 GetUninstallRegPath();

  virtual string16 GetVersionKey();

  virtual bool CanSetAsDefault();

  virtual bool CanCreateDesktopShortcuts();

  virtual int GetIconIndex();

  virtual bool GetChromeChannel(string16* channel);

  // Returns true if the distribution includes a DelegateExecute verb handler,
  // and provides the COM registration data if so:
  // |handler_class_uuid| is the CommandExecuteImpl class UUID.
  // |type_lib_uuid| and |type_lib_version| identify its type library.
  // |interface_uuid| is the ICommandExecuteImpl interface UUID.
  // Only non-null parameters will be set, others will be ignored.
  virtual bool GetDelegateExecuteHandlerData(string16* handler_class_uuid,
                                             string16* type_lib_uuid,
                                             string16* type_lib_version,
                                             string16* interface_uuid);

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
                                    const installer::Product& product,
                                    bool system_level);

  // The user has qualified for the inactive user toast experiment and this
  // function just performs it.
  virtual void InactiveUserToastExperiment(int flavor,
      const string16& experiment_group,
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
