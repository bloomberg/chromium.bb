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
  base::string16 GetBrowserProgIdPrefix() override;

  base::string16 GetBrowserProgIdDesc() override;

  base::string16 GetDisplayName() override;

  base::string16 GetShortcutName(ShortcutType shortcut_type) override;

  int GetIconIndex(ShortcutType shortcut_type) override;

  base::string16 GetBaseAppName() override;

  base::string16 GetInstallSubDir() override;

  base::string16 GetPublisherName() override;

  base::string16 GetAppDescription() override;

  base::string16 GetLongAppDescription() override;

  std::string GetSafeBrowsingName() override;

  base::string16 GetUninstallRegPath() override;

  base::string16 GetIconFilename() override;

  DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() override;

  bool CanCreateDesktopShortcuts() override;

  base::string16 GetCommandExecuteImplClsid() override;

  void UpdateInstallStatus(bool system_install,
                           installer::ArchiveType archive_type,
                           installer::InstallStatus install_status) override;

 protected:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  ChromeFrameDistribution();
};

#endif  // CHROME_INSTALLER_UTIL_CHROME_FRAME_DISTRIBUTION_H_
