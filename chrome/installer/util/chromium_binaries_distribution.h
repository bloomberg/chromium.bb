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
  virtual base::string16 GetBrowserProgIdPrefix() override;

  virtual base::string16 GetBrowserProgIdDesc() override;

  virtual base::string16 GetDisplayName() override;

  virtual base::string16 GetShortcutName(ShortcutType shortcut_type) override;

  virtual int GetIconIndex(ShortcutType shortcut_type) override;

  virtual base::string16 GetBaseAppName() override;

  virtual base::string16 GetBaseAppId() override;

  virtual base::string16 GetInstallSubDir() override;

  virtual base::string16 GetPublisherName() override;

  virtual base::string16 GetAppDescription() override;

  virtual base::string16 GetLongAppDescription() override;

  virtual std::string GetSafeBrowsingName() override;

  virtual base::string16 GetUninstallLinkName() override;

  virtual base::string16 GetUninstallRegPath() override;

  virtual DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() override;

  virtual bool GetChromeChannel(base::string16* channel) override;

  virtual bool GetCommandExecuteImplClsid(
      base::string16* handler_class_uuid) override;

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
