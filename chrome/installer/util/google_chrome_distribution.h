// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends generic BrowserDistribution class to declare Google Chrome
// specific implementation.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_

#include "base/string16.h"
#include "base/gtest_prod_util.h"
#include "chrome/installer/util/browser_distribution.h"

namespace base {
class FilePath;
}

class GoogleChromeDistribution : public BrowserDistribution {
 public:
  // Opens the Google Chrome uninstall survey window.
  // version refers to the version of Chrome being uninstalled.
  // local_data_path is the path of the file containing json metrics that
  //   will be parsed. If this file indicates that the user has opted in to
  //   providing anonymous usage data, then some additional statistics will
  //   be added to the survey url.
  // distribution_data contains Google Update related data that will be
  //   concatenated to the survey url if the file in local_data_path indicates
  //   the user has opted in to providing anonymous usage data.
  virtual void DoPostUninstallOperations(
      const Version& version,
      const base::FilePath& local_data_path,
      const string16& distribution_data) OVERRIDE;

  virtual string16 GetActiveSetupGuid() OVERRIDE;

  virtual string16 GetAppGuid() OVERRIDE;

  virtual string16 GetBaseAppName() OVERRIDE;

  virtual string16 GetAppShortCutName() OVERRIDE;

  virtual string16 GetAlternateApplicationName() OVERRIDE;

  virtual string16 GetBaseAppId() OVERRIDE;

  virtual string16 GetInstallSubDir() OVERRIDE;

  virtual string16 GetPublisherName() OVERRIDE;

  virtual string16 GetAppDescription() OVERRIDE;

  virtual std::string GetSafeBrowsingName() OVERRIDE;

  virtual string16 GetStateKey() OVERRIDE;

  virtual string16 GetStateMediumKey() OVERRIDE;

  virtual string16 GetStatsServerURL() OVERRIDE;

  virtual std::string GetNetworkStatsServer() const OVERRIDE;

  virtual std::string GetHttpPipeliningTestServer() const OVERRIDE;

  // This method reads data from the Google Update ClientState key for
  // potential use in the uninstall survey. It must be called before the
  // key returned by GetVersionKey() is deleted.
  virtual string16 GetDistributionData(HKEY root_key) OVERRIDE;

  virtual string16 GetUninstallLinkName() OVERRIDE;

  virtual string16 GetUninstallRegPath() OVERRIDE;

  virtual string16 GetVersionKey() OVERRIDE;

  virtual string16 GetIconFilename() OVERRIDE;

  virtual bool GetCommandExecuteImplClsid(
      string16* handler_class_uuid) OVERRIDE;

  virtual bool AppHostIsSupported() OVERRIDE;

  virtual void UpdateInstallStatus(
      bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status) OVERRIDE;

  virtual bool ShouldSetExperimentLabels() OVERRIDE;

  virtual bool HasUserExperiments() OVERRIDE;

  const string16& product_guid() { return product_guid_; }

 protected:
  void set_product_guid(const string16& guid) { product_guid_ = guid; }

  // Disallow construction from others.
  GoogleChromeDistribution();

 private:
  friend class BrowserDistribution;

  // The product ID for Google Update.
  string16 product_guid_;
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
