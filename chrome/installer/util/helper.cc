// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/installer/util/browser_distribution.h"

namespace {

FilePath GetChromeInstallBasePath(bool system,
                                  BrowserDistribution* distribution,
                                  const wchar_t* sub_path) {
  FilePath install_path;
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

FilePath installer::GetChromeInstallPath(bool system_install,
                                         BrowserDistribution* dist) {
  return GetChromeInstallBasePath(system_install, dist,
                                  installer::kInstallBinaryDir);
}

FilePath installer::GetChromeUserDataPath(BrowserDistribution* dist) {
  return GetChromeInstallBasePath(false, dist,
      installer::kInstallUserDataDir);
}
