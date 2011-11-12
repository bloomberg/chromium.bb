// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends the browser distribution with a specific implementation
// for Chrome Frame.

#ifndef CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

class ChromeFrameDistribution : public BrowserDistribution {
 public:
  virtual std::wstring GetAppGuid() OVERRIDE;

  virtual std::wstring GetApplicationName() OVERRIDE;

  virtual std::wstring GetAppShortCutName() OVERRIDE;

  virtual std::wstring GetAlternateApplicationName() OVERRIDE;

  virtual std::wstring GetInstallSubDir() OVERRIDE;

  virtual std::wstring GetPublisherName() OVERRIDE;

  virtual std::wstring GetAppDescription() OVERRIDE;

  virtual std::wstring GetLongAppDescription() OVERRIDE;

  virtual std::string GetSafeBrowsingName() OVERRIDE;

  virtual std::wstring GetStateKey() OVERRIDE;

  virtual std::wstring GetStateMediumKey() OVERRIDE;

  virtual std::wstring GetStatsServerURL() OVERRIDE;

  virtual std::string GetNetworkStatsServer() const OVERRIDE;

  virtual std::wstring GetUninstallLinkName() OVERRIDE;

  virtual std::wstring GetUninstallRegPath() OVERRIDE;

  virtual std::wstring GetVersionKey() OVERRIDE;

  virtual bool CanSetAsDefault() OVERRIDE;

  virtual void UpdateInstallStatus(bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status) OVERRIDE;

 protected:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  ChromeFrameDistribution();
};

#endif  // CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
