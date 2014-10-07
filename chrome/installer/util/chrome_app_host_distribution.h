// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends the browser distribution with a specific implementation
// for Chrome AppHost.

#ifndef CHROME_INSTALLER_UTIL_CHROME_APP_HOST_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_CHROME_APP_HOST_DISTRIBUTION_H_

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"

class ChromeAppHostDistribution : public BrowserDistribution {
 public:
  virtual base::string16 GetDisplayName() override;

  virtual base::string16 GetBrowserProgIdPrefix() override;

  virtual base::string16 GetBrowserProgIdDesc() override;

  // This can only be called with SHORTCUT_APP_LAUNCHER for |shortcut_type|.
  virtual base::string16 GetShortcutName(ShortcutType shortcut_type) override;

  virtual base::string16 GetIconFilename() override;

  virtual base::string16 GetBaseAppName() override;

  virtual base::string16 GetBaseAppId() override;

  virtual base::string16 GetInstallSubDir() override;

  virtual base::string16 GetPublisherName() override;

  virtual base::string16 GetAppDescription() override;

  virtual base::string16 GetLongAppDescription() override;

  virtual std::string GetSafeBrowsingName() override;

  virtual std::string GetNetworkStatsServer() const override;

  virtual base::string16 GetUninstallLinkName() override;

  virtual base::string16 GetUninstallRegPath() override;

  virtual DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() override;

  virtual bool CanCreateDesktopShortcuts() override;

  virtual bool GetCommandExecuteImplClsid(
      base::string16* handler_class_uuid) override;

  virtual void UpdateInstallStatus(bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status) override;

 protected:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  ChromeAppHostDistribution();
};

#endif  // CHROME_INSTALLER_UTIL_CHROME_APP_HOST_DISTRIBUTION_H_
