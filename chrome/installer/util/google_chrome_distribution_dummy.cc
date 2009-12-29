// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

#include "base/logging.h"

bool GoogleChromeDistribution::BuildUninstallMetricsString(
    DictionaryValue* uninstall_metrics_dict, std::wstring* metrics) {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::ExtractUninstallMetricsFromFile(
    const std::wstring& file_path, std::wstring* uninstall_metrics_string) {
  NOTREACHED();
  return false;
}

bool GoogleChromeDistribution::ExtractUninstallMetrics(
    const DictionaryValue& root, std::wstring* uninstall_metrics_string) {
  NOTREACHED();
  return false;
}

void GoogleChromeDistribution::LaunchUserExperiment(
    installer_util::InstallStatus status, const installer::Version& version,
    bool system_install) {
  NOTREACHED();
}
void GoogleChromeDistribution::InactiveUserToastExperiment(int flavor) {
  NOTREACHED();
}
