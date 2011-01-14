// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installer_state.h"

#include "base/logging.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/package_properties.h"

namespace installer {

bool InstallerState::IsMultiInstallUpdate(const MasterPreferences& prefs,
    const InstallationState& machine_state) {
  // First, is the package present?
  const ProductState* package =
      machine_state.GetMultiPackageState(system_install_);
  if (package == NULL) {
    // The multi-install package has not been installed, so it certainly isn't
    // being updated.
    return false;
  }

  BrowserDistribution::Type types[2];
  size_t num_types = 0;
  if (prefs.install_chrome())
    types[num_types++] = BrowserDistribution::CHROME_BROWSER;
  if (prefs.install_chrome_frame())
    types[num_types++] = BrowserDistribution::CHROME_FRAME;

  for (const BrowserDistribution::Type* scan = &types[0],
           *end = &types[num_types]; scan != end; ++scan) {
    const ProductState* product =
        machine_state.GetProductState(system_install_, *scan);
    if (product == NULL) {
      VLOG(2) << "It seems that distribution type " << *scan
              << " is being installed for the first time.";
      return false;
    }
    if (!product->channel().Equals(package->channel())) {
      VLOG(2) << "It seems that distribution type " << *scan
              << " is being over installed.";
      return false;
    }
  }

  VLOG(2) << "It seems that the package is being updated.";

  return true;
}

InstallerState::InstallerState() : operation_(UNINITIALIZED) {
}

void InstallerState::Initialize(const MasterPreferences& prefs,
                                const InstallationState& machine_state) {
  if (!prefs.GetBool(installer::master_preferences::kSystemLevel,
                     &system_install_))
    system_install_ = false;

  BrowserDistribution* operand = NULL;

  if (!prefs.is_multi_install()) {
    // For a single-install, the current browser dist is the operand.
    operand = BrowserDistribution::GetDistribution();
    operation_ = SINGLE_INSTALL_OR_UPDATE;
  } else if (IsMultiInstallUpdate(prefs, machine_state)) {
    // Updates driven by Google Update take place under the multi-installer's
    // app guid.
    installer::ActivePackageProperties package_properties;
    operation_ = MULTI_UPDATE;
    state_key_ = package_properties.GetStateKey();
  } else {
    // Initial and over installs will always take place under one of the
    // product app guids.  Chrome Frame's will be used if only Chrome Frame
    // is being installed.  In all other cases, Chrome's is used.
    operand = BrowserDistribution::GetSpecificDistribution(
        prefs.install_chrome() ?
            BrowserDistribution::CHROME_BROWSER :
            BrowserDistribution::CHROME_FRAME,
        prefs);
    operation_ = MULTI_INSTALL;
  }

  if (operand != NULL) {
    state_key_ = operand->GetStateKey();
  }
}

}  // namespace installer
