// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"

class ChromiumBinariesDistribution : public BrowserDistribution {
 public:
  base::string16 GetBrowserProgIdPrefix() override;

  base::string16 GetBrowserProgIdDesc() override;

  base::string16 GetDisplayName() override;

  base::string16 GetShortcutName(ShortcutType shortcut_type) override;

  int GetIconIndex(ShortcutType shortcut_type) override;

  base::string16 GetBaseAppName() override;

  base::string16 GetBaseAppId() override;

  base::string16 GetInstallSubDir() override;

  base::string16 GetPublisherName() override;

  base::string16 GetAppDescription() override;

  base::string16 GetLongAppDescription() override;

  std::string GetSafeBrowsingName() override;

  base::string16 GetRegistryPath() override;

  base::string16 GetUninstallRegPath() override;

  DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() override;

  bool GetChromeChannel(base::string16* channel) override;

  base::string16 GetCommandExecuteImplClsid() override;

 protected:
  friend class BrowserDistribution;

  ChromiumBinariesDistribution();

  explicit ChromiumBinariesDistribution(
      scoped_ptr<AppRegistrationData> app_reg_data);

  BrowserDistribution* browser_distribution_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumBinariesDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_
