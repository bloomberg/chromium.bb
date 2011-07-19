// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_
#pragma once

#include <string>

#include "chrome/installer/util/browser_distribution.h"

class ChromiumBinariesDistribution : public BrowserDistribution {
 public:
  virtual std::wstring GetAppGuid() OVERRIDE;

  virtual std::wstring GetApplicationName() OVERRIDE;

  virtual std::wstring GetAppShortCutName() OVERRIDE;

  virtual std::wstring GetAlternateApplicationName() OVERRIDE;

  virtual std::wstring GetBrowserAppId() OVERRIDE;

  virtual std::wstring GetInstallSubDir() OVERRIDE;

  virtual std::wstring GetPublisherName() OVERRIDE;

  virtual std::wstring GetAppDescription() OVERRIDE;

  virtual std::wstring GetLongAppDescription() OVERRIDE;

  virtual std::string GetSafeBrowsingName() OVERRIDE;

  virtual std::wstring GetStateKey() OVERRIDE;

  virtual std::wstring GetStateMediumKey() OVERRIDE;

  virtual std::wstring GetUninstallLinkName() OVERRIDE;

  virtual std::wstring GetUninstallRegPath() OVERRIDE;

  virtual std::wstring GetVersionKey() OVERRIDE;

  virtual bool CanSetAsDefault() OVERRIDE;

  virtual int GetIconIndex() OVERRIDE;

  virtual bool GetChromeChannel(std::wstring* channel) OVERRIDE;

 protected:
  friend class BrowserDistribution;

  ChromiumBinariesDistribution();

  BrowserDistribution* browser_distribution_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumBinariesDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_CHROMIUM_BINARIES_DISTRIBUTION_H_
