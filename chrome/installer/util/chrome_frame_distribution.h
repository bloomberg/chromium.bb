// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends the browser distribution with a specific implementation
// for Chrome Frame.

#ifndef CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

class ChromeFrameDistribution : public BrowserDistribution {
 public:
  virtual string16 GetAppGuid() OVERRIDE;

  virtual string16 GetBaseAppName() OVERRIDE;

  virtual string16 GetAppShortCutName() OVERRIDE;

  virtual string16 GetAlternateApplicationName() OVERRIDE;

  virtual string16 GetInstallSubDir() OVERRIDE;

  virtual string16 GetPublisherName() OVERRIDE;

  virtual string16 GetAppDescription() OVERRIDE;

  virtual string16 GetLongAppDescription() OVERRIDE;

  virtual std::string GetSafeBrowsingName() OVERRIDE;

  virtual string16 GetStateKey() OVERRIDE;

  virtual string16 GetStateMediumKey() OVERRIDE;

  virtual string16 GetStatsServerURL() OVERRIDE;

  virtual std::string GetNetworkStatsServer() const OVERRIDE;

  virtual std::string GetHttpPipeliningTestServer() const OVERRIDE;

  virtual string16 GetUninstallLinkName() OVERRIDE;

  virtual string16 GetUninstallRegPath() OVERRIDE;

  virtual string16 GetVersionKey() OVERRIDE;

  virtual string16 GetIconFilename() OVERRIDE;

  virtual int GetIconIndex() OVERRIDE;

  virtual bool CanSetAsDefault() OVERRIDE;

  virtual bool CanCreateDesktopShortcuts() OVERRIDE;

  virtual bool GetCommandExecuteImplClsid(
      string16* handler_class_uuid) OVERRIDE;

  virtual void UpdateInstallStatus(bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status) OVERRIDE;

 protected:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  ChromeFrameDistribution();
};

#endif  // CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
