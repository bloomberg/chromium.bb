// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_FAKE_INSTALLATION_STATE_H_
#define CHROME_INSTALLER_UTIL_FAKE_INSTALLATION_STATE_H_

#include "base/files/file_path.h"
#include "base/version.h"
#include "chrome/installer/util/fake_product_state.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

// An InstallationState helper for use by unit tests.
class FakeInstallationState : public InstallationState {
 public:
  // Takes ownership of |version|.
  void AddChrome(bool system_install, bool multi_install, Version* version) {
    FakeProductState chrome_state;
    chrome_state.set_version(version);
    chrome_state.set_multi_install(multi_install);
    base::FilePath setup_exe(
        GetChromeInstallPath(system_install,
                             BrowserDistribution::GetSpecificDistribution(
                                 BrowserDistribution::CHROME_BROWSER)));
    setup_exe = setup_exe
        .AppendASCII(version->GetString())
        .Append(kInstallerDir)
        .Append(kSetupExe);
    chrome_state.SetUninstallProgram(setup_exe);
    chrome_state.AddUninstallSwitch(switches::kUninstall);
    if (multi_install) {
      chrome_state.AddUninstallSwitch(switches::kMultiInstall);
      chrome_state.AddUninstallSwitch(switches::kChrome);
    }
    SetProductState(system_install, BrowserDistribution::CHROME_BROWSER,
                    chrome_state);
  }

  void SetProductState(bool system_install,
                       BrowserDistribution::Type type,
                       const ProductState& product_state) {
    ProductState& target = GetProducts(system_install)[IndexFromDistType(type)];
    target.CopyFrom(product_state);
  }

 protected:
  ProductState* GetProducts(bool system_install) {
    return system_install ? system_products_ : user_products_;
  }
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_FAKE_INSTALLATION_STATE_H_
