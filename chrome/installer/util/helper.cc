// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/package_properties.h"

using base::win::RegKey;

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

namespace installer {

bool IsInstalledAsMulti(bool system_install, BrowserDistribution* dist) {
  bool installed_as_multi = false;
  CommandLine cmd(CommandLine::NO_PROGRAM);
  if (GetUninstallSwitches(system_install, dist, &cmd))
    installed_as_multi = cmd.HasSwitch(installer::switches::kMultiInstall);
  return installed_as_multi;
}

bool GetUninstallSwitches(bool system_install, BrowserDistribution* dist,
                          CommandLine* cmd_line_switches) {
  scoped_ptr<Version> installed(InstallUtil::GetChromeVersion(dist,
                                                              system_install));
  if (installed.get()) {
    HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    RegKey key(root, dist->GetStateKey().c_str(), KEY_READ);
    if (key.Valid()) {
      std::wstring args;
      key.ReadValue(installer::kUninstallArgumentsField, &args);
      if (!args.empty()) {
        args.insert(0, L"foo.exe ");
        *cmd_line_switches = CommandLine::FromString(args);
      } else {
        LOG(ERROR) << "No uninstallation arguments for "
                   << dist->GetApplicationName();
        installed.reset();
      }
    } else {
      LOG(ERROR) << "Product looks to be installed but we can't access the "
                    "state key: " << dist->GetApplicationName();
      installed.reset();
    }
  }

  return installed.get() != NULL;
}

FilePath GetChromeInstallPath(bool system_install, BrowserDistribution* dist) {
  return GetChromeInstallBasePath(system_install, dist,
                                  installer::kInstallBinaryDir);
}

FilePath GetChromeUserDataPath(BrowserDistribution* dist) {
  return GetChromeInstallBasePath(false, dist, kInstallUserDataDir);
}

FilePath GetChromeFrameInstallPath(bool multi_install, bool system_install,
                                   BrowserDistribution* dist) {
  DCHECK_EQ(BrowserDistribution::CHROME_FRAME, dist->GetType());

  scoped_ptr<Version> installed_version(
      InstallUtil::GetChromeVersion(dist, system_install));

  if (!multi_install) {
    // Check if Chrome Frame is installed as multi. If it is, return an empty
    // path and log an error.
    if (installed_version.get() && IsInstalledAsMulti(system_install, dist)) {
      LOG(ERROR) << "Cannot install Chrome Frame in single mode as a multi mode"
                    " installation already exists.";
      return FilePath();
    }
    VLOG(1) << "Chrome Frame will be installed as 'single'";
    return GetChromeInstallPath(system_install, dist);
  }

  // TODO(tommi): If Chrome Frame is installed as single and the installed
  // channel is older than the one we're installing, we should migrate
  // CF to Chrome's install folder and change its channel.

  // Multi install.  Check if Chrome Frame is already installed.
  // If CF is installed as single (i.e. not multi), we will return an empty
  // path (for now).  Otherwise, if CF is not installed or if it is installed
  // as multi, we will return Chrome's install folder.
  if (installed_version.get() && !IsInstalledAsMulti(system_install, dist)) {
    LOG(ERROR) << "Cannot install Chrome Frame in multi mode as a single mode"
                  " installation already exists.";
    return FilePath();
  }

  // Return Chrome's installation folder.
  VLOG(1) << "Chrome Frame will be installed as 'multi'";
  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  BrowserDistribution* chrome =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER, prefs);
  return GetChromeInstallPath(system_install, chrome);
}

std::wstring GetAppGuidForUpdates(bool system_install) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // If we're part of a multi-install, we need to poll using the multi-installer
  // package's app guid rather than the browser's or Chrome Frame's app guid.
  return IsInstalledAsMulti(system_install, dist) ?
      ActivePackageProperties().GetAppGuid() :
      dist->GetAppGuid();
}

}  // namespace installer.
