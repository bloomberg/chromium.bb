// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"

class ChromiumBinariesDistribution : public BrowserDistribution {
 public:
  virtual base::string16 GetBrowserProgIdPrefix() OVERRIDE;

  virtual base::string16 GetBrowserProgIdDesc() OVERRIDE;

  virtual base::string16 GetDisplayName() OVERRIDE;

  virtual base::string16 GetShortcutName(ShortcutType shortcut_type) OVERRIDE;

  virtual int GetIconIndex(ShortcutType shortcut_type) OVERRIDE;

  virtual base::string16 GetBaseAppName() OVERRIDE;

  virtual base::string16 GetBaseAppId() OVERRIDE;

  virtual base::string16 GetInstallSubDir() OVERRIDE;

  virtual base::string16 GetPublisherName() OVERRIDE;

  virtual base::string16 GetAppDescription() OVERRIDE;

  virtual base::string16 GetLongAppDescription() OVERRIDE;

  virtual std::string GetSafeBrowsingName() OVERRIDE;

  virtual base::string16 GetUninstallLinkName() OVERRIDE;

  virtual base::string16 GetUninstallRegPath() OVERRIDE;

  virtual DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() OVERRIDE;

  virtual bool GetChromeChannel(base::string16* channel) OVERRIDE;

  virtual bool GetCommandExecuteImplClsid(
      base::string16* handler_class_uuid) OVERRIDE;

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
