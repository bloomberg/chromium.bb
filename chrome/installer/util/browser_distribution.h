// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
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
    CHROME_APP_HOST,
    NUM_TYPES
  };

  // Flags to control what to show in the UserExperiment dialog.
  enum ToastUIflags {
    kUninstall          = 1,    // Uninstall radio button.
    kDontBugMeAsButton  = 2,    // Don't bug me is a button, not a radio button.
    kWhyLink            = 4,    // Has the 'why I am seeing this' link.
    kMakeDefault        = 8     // Has the 'make it default' checkbox.
  };

  // A struct for communicating what a UserExperiment contains. In these
  // experiments we show toasts to the user if they are inactive for a certain
  // amount of time.
  struct UserExperiment {
    string16 prefix;      // The experiment code prefix for this experiment,
                          // also known as the 'TV' part in 'TV80'.
    int flavor;           // The flavor index for this experiment.
    int heading;          // The heading resource ID to use for this experiment.
    int flags;            // See ToastUIFlags above.
    int control_group;    // Size of the control group (in percentages). Control
                          // group is the group that qualifies for the
                          // experiment but does not participate.
  };

  virtual ~BrowserDistribution() {}

  static BrowserDistribution* GetDistribution();

  static BrowserDistribution* GetSpecificDistribution(Type type);

  Type GetType() const { return type_; }

  virtual void DoPostUninstallOperations(const Version& version,
                                         const base::FilePath& local_data_path,
                                         const string16& distribution_data);

  // Returns the GUID to be used when registering for Active Setup.
  virtual string16 GetActiveSetupGuid();

  virtual string16 GetAppGuid();

  // Returns the unsuffixed application name of this program.
  // This is the base of the name registered with Default Programs on Windows.
  // IMPORTANT: This should only be called by the installer which needs to make
  // decisions on the suffixing of the upcoming install, not by external callers
  // at run-time.
  virtual string16 GetBaseAppName();

  // Returns the localized name of the program.
  virtual string16 GetAppShortCutName();

  virtual string16 GetAlternateApplicationName();

  // Returns the unsuffixed appid of this program.
  // The AppUserModelId is a property of Windows programs.
  // IMPORTANT: This should only be called by ShellUtil::GetAppId as the appid
  // should be suffixed in all scenarios.
  virtual string16 GetBaseAppId();

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

  // Returns the executable filename (not path) that contains the product icon.
  virtual string16 GetIconFilename();

  // Returns the index of the icon for the product, inside the file specified by
  // GetIconFilename().
  virtual int GetIconIndex();

  virtual bool GetChromeChannel(string16* channel);

  // Returns true if this distribution includes a DelegateExecute verb handler,
  // and provides the CommandExecuteImpl class UUID if |handler_class_uuid| is
  // non-NULL.
  virtual bool GetCommandExecuteImplClsid(string16* handler_class_uuid);

  // Returns true if this distribution uses app_host.exe to run platform apps.
  virtual bool AppHostIsSupported();

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
  virtual void LaunchUserExperiment(const base::FilePath& setup_path,
                                    installer::InstallStatus status,
                                    const Version& version,
                                    const installer::Product& product,
                                    bool system_level);

  // The user has qualified for the inactive user toast experiment and this
  // function just performs it.
  virtual void InactiveUserToastExperiment(int flavor,
      const string16& experiment_group,
      const installer::Product& installation,
      const base::FilePath& application_path);

  // Returns true if this distribution should set the Omaha experiment_labels
  // registry value.
  virtual bool ShouldSetExperimentLabels();

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
