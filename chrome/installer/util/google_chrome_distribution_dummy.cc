// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#include "base/file_path.h"
#include "base/logging.h"

GoogleChromeDistribution::GoogleChromeDistribution()
    : BrowserDistribution(CHROME_BROWSER) {
}

void GoogleChromeDistribution::DoPostUninstallOperations(
    const Version& version,
    const FilePath& local_data_path,
    const std::wstring& distribution_data) {
}

std::wstring GoogleChromeDistribution::GetAppGuid() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetApplicationName() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetAlternateApplicationName() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetBrowserAppId() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetInstallSubDir() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetPublisherName() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetAppDescription() {
  NOTREACHED();
  return std::wstring();
}

std::string GoogleChromeDistribution::GetSafeBrowsingName() {
  NOTREACHED();
  return std::string();
}

std::wstring GoogleChromeDistribution::GetStateKey() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetStateMediumKey() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetStatsServerURL() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetDistributionData(HKEY root_key) {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetUninstallLinkName() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetUninstallRegPath() {
  NOTREACHED();
  return std::wstring();
}

std::wstring GoogleChromeDistribution::GetVersionKey() {
  NOTREACHED();
  return std::wstring();
}

void GoogleChromeDistribution::UpdateInstallStatus(bool system_install,
    bool incremental_install, bool multi_install,
    installer::InstallStatus install_status) {
  NOTREACHED();
}

void GoogleChromeDistribution::LaunchUserExperiment(
    installer::InstallStatus status, const Version& version,
    const installer::Product& installation, bool system_level) {
  NOTREACHED();
}

void GoogleChromeDistribution::InactiveUserToastExperiment(int flavor,
    const installer::Product& installation,
    const FilePath& application_path) {
  NOTREACHED();
}

bool GoogleChromeDistribution::ExtractUninstallMetricsFromFile(
    const FilePath& file_path, std::wstring* uninstall_metrics_string) {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::ExtractUninstallMetrics(
    const DictionaryValue& root, std::wstring* uninstall_metrics_string) {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::BuildUninstallMetricsString(
    DictionaryValue* uninstall_metrics_dict, std::wstring* metrics) {
  NOTREACHED();
  return false;
}
