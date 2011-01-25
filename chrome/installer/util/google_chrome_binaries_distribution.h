// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_BINARIES_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_BINARIES_DISTRIBUTION_H_
#pragma once

#include "chrome/installer/util/chromium_binaries_distribution.h"

class GoogleChromeBinariesDistribution : public ChromiumBinariesDistribution {
 public:
  virtual std::wstring GetAppGuid();

  virtual std::wstring GetAppShortCutName();

  virtual std::wstring GetStateKey();

  virtual std::wstring GetStateMediumKey();

  virtual std::wstring GetVersionKey();

  virtual void UpdateInstallStatus(bool system_install,
      bool incremental_install, bool multi_install,
      installer::InstallStatus install_status);

 protected:
  friend class BrowserDistribution;

  GoogleChromeBinariesDistribution();

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleChromeBinariesDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_BINARIES_DISTRIBUTION_H_
