// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"

namespace {

base::FilePath GetChromeInstallBasePath(bool system,
                                        BrowserDistribution* distribution,
                                        const wchar_t* sub_path) {
  base::FilePath install_path;
  if (system) {
    PathService::Get(base::DIR_PROGRAM_FILES, &install_path);
  } else {
    PathService::Get(base::DIR_LOCAL_APP_DATA, &install_path);
  }

  if (!install_path.empty()) {
    install_path = install_path.Append(distribution->GetInstallSubDir());
    install_path = install_path.Append(sub_path);
  }

  return install_path;
}

}  // namespace

namespace installer {

base::FilePath GetChromeInstallPath(bool system_install,
                                    BrowserDistribution* dist) {
  return GetChromeInstallBasePath(system_install, dist, kInstallBinaryDir);
}

void GetChromeUserDataPaths(BrowserDistribution* dist,
                            std::vector<base::FilePath>* paths) {
  const bool has_metro_data = dist->CanSetAsDefault() &&
      base::win::GetVersion() >= base::win::VERSION_WIN8;
  base::FilePath data_dir(GetChromeInstallBasePath(false, dist,
                                                   kInstallUserDataDir));
  if (data_dir.empty()) {
    paths->clear();
  } else {
    paths->resize(has_metro_data ? 2 : 1);
    (*paths)[0] = data_dir;
    if (has_metro_data) {
      (*paths)[1] = data_dir.DirName().Append(
          chrome::kMetroChromeUserDataSubDir);
    }
  }
  DCHECK(!paths->empty());
}

BrowserDistribution* GetBinariesDistribution(bool system_install) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  ProductState state;

  // If we're part of a multi-install, we need to poll using the multi-installer
  // package's app guid rather than the browser's or Chrome Frame's app guid.
  // If we can't read the app's state from the registry, assume it isn't
  // multi-installed.
  if (state.Initialize(system_install, dist) && state.is_multi_install()) {
    return BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
  } else {
    return dist;
  }
}

std::wstring GetAppGuidForUpdates(bool system_install) {
  return GetBinariesDistribution(system_install)->GetAppGuid();
}

}  // namespace installer.
