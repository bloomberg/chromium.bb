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
  virtual base::string16 GetBrowserProgIdPrefix() OVERRIDE;

  virtual base::string16 GetBrowserProgIdDesc() OVERRIDE;

  virtual base::string16 GetDisplayName() OVERRIDE;

  virtual base::string16 GetShortcutName(ShortcutType shortcut_type) OVERRIDE;

  virtual int GetIconIndex(ShortcutType shortcut_type) OVERRIDE;

  virtual base::string16 GetBaseAppName() OVERRIDE;

  virtual base::string16 GetInstallSubDir() OVERRIDE;

  virtual base::string16 GetPublisherName() OVERRIDE;

  virtual base::string16 GetAppDescription() OVERRIDE;

  virtual base::string16 GetLongAppDescription() OVERRIDE;

  virtual std::string GetSafeBrowsingName() OVERRIDE;

  virtual std::string GetNetworkStatsServer() const OVERRIDE;

  virtual base::string16 GetUninstallLinkName() OVERRIDE;

  virtual base::string16 GetUninstallRegPath() OVERRIDE;

  virtual base::string16 GetIconFilename() OVERRIDE;

  virtual DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() OVERRIDE;

  virtual bool CanCreateDesktopShortcuts() OVERRIDE;

  virtual bool GetCommandExecuteImplClsid(
      base::string16* handler_class_uuid) OVERRIDE;

  virtual void UpdateInstallStatus(bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status) OVERRIDE;

 protected:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  ChromeFrameDistribution();
};

#endif  // CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
