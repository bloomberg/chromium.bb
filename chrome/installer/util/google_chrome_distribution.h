// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends generic BrowserDistribution class to declare Google Chrome
// specific implementation.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

class DictionaryValue;
class FilePath;

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
  virtual void DoPostUninstallOperations(const Version& version,
                                         const FilePath& local_data_path,
                                         const std::wstring& distribution_data);

  virtual std::wstring GetAppGuid();

  virtual std::wstring GetApplicationName();

  virtual std::wstring GetAlternateApplicationName();

  virtual std::wstring GetBrowserAppId();

  virtual std::wstring GetInstallSubDir();

  virtual std::wstring GetPublisherName();

  virtual std::wstring GetAppDescription();

  virtual std::string GetSafeBrowsingName();

  virtual std::wstring GetStateKey();

  virtual std::wstring GetStateMediumKey();

  virtual std::wstring GetStatsServerURL();

  // This method reads data from the Google Update ClientState key for
  // potential use in the uninstall survey. It must be called before the
  // key returned by GetVersionKey() is deleted.
  virtual std::wstring GetDistributionData(HKEY root_key);

  virtual std::wstring GetUninstallLinkName();

  virtual std::wstring GetUninstallRegPath();

  virtual std::wstring GetVersionKey();

  virtual void UpdateInstallStatus(bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status);

  virtual void LaunchUserExperiment(installer::InstallStatus status,
      const Version& version,
      const installer::Product& installation,
      bool system_level);

  // Assuming that the user qualifies, this function performs the inactive user
  // toast experiment. It will use chrome to show the UI and it will record the
  // outcome in the registry.
  virtual void InactiveUserToastExperiment(int flavor,
      const installer::Product& installation,
      const FilePath& application_path);

  const std::wstring& product_guid() { return product_guid_; }

 protected:
  void set_product_guid(const std::wstring& guid) { product_guid_ = guid; }

  // Disallow construction from others.
  GoogleChromeDistribution();

 private:
  friend class BrowserDistribution;

  FRIEND_TEST_ALL_PREFIXES(GoogleChromeDistTest, TestExtractUninstallMetrics);

  // Extracts uninstall metrics from the JSON file located at file_path.
  // Returns them in a form suitable for appending to a url that already
  // has GET parameters, i.e. &metric1=foo&metric2=bar.
  // Returns true if uninstall_metrics has been successfully populated with
  // the uninstall metrics, false otherwise.
  virtual bool ExtractUninstallMetricsFromFile(
      const FilePath& file_path, std::wstring* uninstall_metrics);

  // Extracts uninstall metrics from the given JSON value.
  virtual bool ExtractUninstallMetrics(const DictionaryValue& root,
                                       std::wstring* uninstall_metrics);

  // Given a DictionaryValue containing a set of uninstall metrics,
  // this builds a URL parameter list of all the contained metrics.
  // Returns true if at least one uninstall metric was found in
  // uninstall_metrics_dict, false otherwise.
  virtual bool BuildUninstallMetricsString(
      DictionaryValue* uninstall_metrics_dict, std::wstring* metrics);

  // The product ID for Google Update.
  std::wstring product_guid_;
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
