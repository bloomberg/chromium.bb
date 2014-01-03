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
    const base::string16& distribution_data) {
}

base::string16 GoogleChromeDistribution::GetActiveSetupGuid() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetAppGuid() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetBaseAppName() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetShortcutName(
    ShortcutType shortcut_type) {
  NOTREACHED();
  return base::string16();
}

int GoogleChromeDistribution::GetIconIndex(ShortcutType shortcut_type) {
  NOTREACHED();
  return 0;
}

base::string16 GoogleChromeDistribution::GetBaseAppId() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetBrowserProgIdPrefix() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetBrowserProgIdDesc() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetInstallSubDir() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetPublisherName() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetAppDescription() {
  NOTREACHED();
  return base::string16();
}

std::string GoogleChromeDistribution::GetSafeBrowsingName() {
  NOTREACHED();
  return std::string();
}

base::string16 GoogleChromeDistribution::GetStateKey() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetStateMediumKey() {
  NOTREACHED();
  return base::string16();
}

std::string GoogleChromeDistribution::GetNetworkStatsServer() const {
  NOTREACHED();
  return std::string();
}

std::string GoogleChromeDistribution::GetHttpPipeliningTestServer() const {
  NOTREACHED();
  return std::string();
}

base::string16 GoogleChromeDistribution::GetDistributionData(HKEY root_key) {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetUninstallLinkName() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetUninstallRegPath() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetVersionKey() {
  NOTREACHED();
  return base::string16();
}

base::string16 GoogleChromeDistribution::GetIconFilename() {
  NOTREACHED();
  return base::string16();
}

bool GoogleChromeDistribution::GetCommandExecuteImplClsid(
    base::string16* handler_class_uuid) {
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

bool GoogleChromeDistribution::ShouldSetExperimentLabels() {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::HasUserExperiments() {
  NOTREACHED();
  return false;
}
