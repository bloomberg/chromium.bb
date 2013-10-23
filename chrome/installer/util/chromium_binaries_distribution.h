// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_

#include <string>

#include "chrome/installer/util/browser_distribution.h"

class ChromiumBinariesDistribution : public BrowserDistribution {
 public:
  virtual string16 GetAppGuid() OVERRIDE;

  virtual string16 GetBrowserProgIdPrefix() OVERRIDE;

  virtual string16 GetBrowserProgIdDesc() OVERRIDE;

  virtual string16 GetDisplayName() OVERRIDE;

  virtual string16 GetShortcutName(ShortcutType shortcut_type) OVERRIDE;

  virtual int GetIconIndex(ShortcutType shortcut_type) OVERRIDE;

  virtual string16 GetBaseAppName() OVERRIDE;

  virtual string16 GetBaseAppId() OVERRIDE;

  virtual string16 GetInstallSubDir() OVERRIDE;

  virtual string16 GetPublisherName() OVERRIDE;

  virtual string16 GetAppDescription() OVERRIDE;

  virtual string16 GetLongAppDescription() OVERRIDE;

  virtual std::string GetSafeBrowsingName() OVERRIDE;

  virtual string16 GetStateKey() OVERRIDE;

  virtual string16 GetStateMediumKey() OVERRIDE;

  virtual string16 GetUninstallLinkName() OVERRIDE;

  virtual string16 GetUninstallRegPath() OVERRIDE;

  virtual string16 GetVersionKey() OVERRIDE;

  virtual DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() OVERRIDE;

  virtual bool GetChromeChannel(string16* channel) OVERRIDE;

  virtual bool GetCommandExecuteImplClsid(
      string16* handler_class_uuid) OVERRIDE;

 protected:
  friend class BrowserDistribution;

  ChromiumBinariesDistribution();

  BrowserDistribution* browser_distribution_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumBinariesDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_
