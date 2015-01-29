// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_SXS_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_SXS_DISTRIBUTION_H_

#include "base/strings/string16.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/util_constants.h"

// GoogleChromeSxSDistribution encapsulates properties of Google Chrome Sxs
// distribution which can co-exist with other Google Chrome distributions.
// Google Chrome Sxs distribution is installed to a different path, runs
// alongside with normally installed Google Chrome, and is updated separately.
// It is mainly used for developer preview and testing, and is disabled for
// system level install and setting as default browser.
class GoogleChromeSxSDistribution : public GoogleChromeDistribution {
 public:
  virtual base::string16 GetBaseAppName() override;
  virtual base::string16 GetShortcutName(ShortcutType shortcut_type) override;
  virtual int GetIconIndex(ShortcutType shortcut_type) override;
  virtual base::string16 GetStartMenuShortcutSubfolder(
      Subfolder subfolder_type) override;
  virtual base::string16 GetBaseAppId() override;
  virtual base::string16 GetBrowserProgIdPrefix() override;
  virtual base::string16 GetBrowserProgIdDesc() override;
  virtual base::string16 GetInstallSubDir() override;
  virtual base::string16 GetUninstallRegPath() override;
  virtual DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() override;
  virtual bool GetChromeChannel(base::string16* channel) override;
  virtual bool GetCommandExecuteImplClsid(
      base::string16* handler_class_uuid) override;
  virtual bool ShouldSetExperimentLabels() override;
  virtual bool HasUserExperiments() override;
  // returns the channel name for GoogleChromeSxSDistribution
  static base::string16 ChannelName();
 private:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  GoogleChromeSxSDistribution();
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_SXS_DISTRIBUTION_H_
