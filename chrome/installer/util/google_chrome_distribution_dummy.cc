// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines dummy implementation of several functions from the
// BrowserDistribution class for Google Chrome. These functions allow 64-bit
// Windows Chrome binary to build successfully. Since this binary is only used
// for Native Client support, most of the install/uninstall functionality is not
// necessary there.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/values.h"

GoogleChromeDistribution::GoogleChromeDistribution()
    : BrowserDistribution(CHROME_BROWSER) {
}

void GoogleChromeDistribution::DoPostUninstallOperations(
    const Version& version,
    const base::FilePath& local_data_path,
    const string16& distribution_data) {
}

string16 GoogleChromeDistribution::GetActiveSetupGuid() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetAppGuid() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetBaseAppName() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetAppShortCutName() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetAlternateApplicationName() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetBaseAppId() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetInstallSubDir() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetPublisherName() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetAppDescription() {
  NOTREACHED();
  return string16();
}

std::string GoogleChromeDistribution::GetSafeBrowsingName() {
  NOTREACHED();
  return std::string();
}

string16 GoogleChromeDistribution::GetStateKey() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetStateMediumKey() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetStatsServerURL() {
  NOTREACHED();
  return string16();
}

std::string GoogleChromeDistribution::GetNetworkStatsServer() const {
  NOTREACHED();
  return std::string();
}

std::string GoogleChromeDistribution::GetHttpPipeliningTestServer() const {
  NOTREACHED();
  return std::string();
}

string16 GoogleChromeDistribution::GetDistributionData(HKEY root_key) {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetUninstallLinkName() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetUninstallRegPath() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetVersionKey() {
  NOTREACHED();
  return string16();
}

string16 GoogleChromeDistribution::GetIconFilename() {
  NOTREACHED();
  return string16();
}

bool GoogleChromeDistribution::GetCommandExecuteImplClsid(
    string16* handler_class_uuid) {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::AppHostIsSupported() {
  NOTREACHED();
  return false;
}

void GoogleChromeDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
  NOTREACHED();
}

bool GoogleChromeDistribution::GetExperimentDetails(
    UserExperiment* experiment, int flavor) {
  NOTREACHED();
  return false;
}

void GoogleChromeDistribution::LaunchUserExperiment(
    const base::FilePath& setup_path, installer::InstallStatus status,
    const Version& version, const installer::Product& product,
    bool system_level) {
  NOTREACHED();
}

void GoogleChromeDistribution::InactiveUserToastExperiment(int flavor,
    const string16& experiment_group,
    const installer::Product& installation,
    const base::FilePath& application_path) {
  NOTREACHED();
}

bool GoogleChromeDistribution::ExtractUninstallMetricsFromFile(
    const base::FilePath& file_path, string16* uninstall_metrics_string) {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::ExtractUninstallMetrics(
    const DictionaryValue& root, string16* uninstall_metrics_string) {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::BuildUninstallMetricsString(
    const DictionaryValue* uninstall_metrics_dict, string16* metrics) {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::ShouldSetExperimentLabels() {
  NOTREACHED();
  return false;
}
