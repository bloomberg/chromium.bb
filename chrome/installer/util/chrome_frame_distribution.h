// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends the browser distribution with a specific implementation
// for Chrome Frame.

#ifndef CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
#pragma once

#include <string>

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {
class MasterPreferences;
}

class ChromeFrameDistribution : public BrowserDistribution {
 public:
  virtual std::wstring GetAppGuid();

  virtual std::wstring GetApplicationName();

  virtual std::wstring GetAlternateApplicationName();

  virtual std::wstring GetInstallSubDir();

  virtual std::wstring GetPublisherName();

  virtual std::wstring GetAppDescription();

  virtual std::wstring GetLongAppDescription();

  virtual std::string GetSafeBrowsingName();

  virtual std::wstring GetStateKey();

  virtual std::wstring GetStateMediumKey();

  virtual std::wstring GetStatsServerURL();

  virtual std::wstring GetUninstallLinkName();

  virtual std::wstring GetUninstallRegPath();

  virtual std::wstring GetVersionKey();

  virtual bool CanSetAsDefault();

  // This is the point at which the Google Chrome installer removes the Google
  // Update ap value. We implement this simply to have the same behaviour re.
  // the ap value.
  virtual void UpdateDiffInstallStatus(bool system_install,
      bool incremental_install, installer::InstallStatus install_status);

  virtual std::vector<FilePath> GetKeyFiles();

  virtual std::vector<FilePath> GetComDllList();

  virtual void AppendUninstallCommandLineFlags(CommandLine* cmd_line);

 protected:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  explicit ChromeFrameDistribution(
      const installer::MasterPreferences& prefs);

  // Determines whether this Chrome Frame distribution is being used to work
  // with CEEE bits as well.
  bool ceee_;
};

#endif  // CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
