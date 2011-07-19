// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_SXS_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_SXS_DISTRIBUTION_H_
#pragma once

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {
class MasterPreferences;
}

// GoogleChromeSxSDistribution encapsulates properties of Google Chrome Sxs
// distribution which can co-exist with other Google Chrome distributions.
// Google Chrome Sxs distribution is installed to a different path, runs
// alongside with normally installed Google Chrome, and is updated separately.
// It is mainly used for developer preview and testing, and is disabled for
// system level install and setting as default browser.
class GoogleChromeSxSDistribution : public GoogleChromeDistribution {
 public:
  virtual std::wstring GetAppShortCutName();
  virtual std::wstring GetBrowserAppId();
  virtual std::wstring GetInstallSubDir();
  virtual std::wstring GetUninstallRegPath();
  virtual bool CanSetAsDefault();
  virtual int GetIconIndex();
  virtual bool GetChromeChannel(std::wstring* channel);
  // returns the channel name for GoogleChromeSxSDistribution
  static std::wstring ChannelName();
 private:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  GoogleChromeSxSDistribution();
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_SXS_DISTRIBUTION_H_
