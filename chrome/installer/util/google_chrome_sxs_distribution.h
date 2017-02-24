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
  base::string16 GetBaseAppName() override;
  base::string16 GetShortcutName() override;
  int GetIconIndex() override;
  base::string16 GetStartMenuShortcutSubfolder(
      Subfolder subfolder_type) override;
  base::string16 GetBaseAppId() override;
  base::string16 GetBrowserProgIdPrefix() override;
  base::string16 GetBrowserProgIdDesc() override;
  base::string16 GetInstallSubDir() override;
  base::string16 GetUninstallRegPath() override;
  DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy() override;
  base::string16 GetCommandExecuteImplClsid() override;
  bool ShouldSetExperimentLabels() override;
  bool HasUserExperiments() override;

 private:
  friend class BrowserDistribution;

  // Disallow construction from non-friends.
  GoogleChromeSxSDistribution();
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_SXS_DISTRIBUTION_H_
