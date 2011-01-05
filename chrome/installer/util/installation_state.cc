// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installation_state.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/package_properties.h"

namespace installer {

ProductState::ProductState() {
}

void ProductState::Initialize(bool system_install,
                              const std::wstring& version_key,
                              const std::wstring& state_key) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey key(root_key, version_key.c_str(), KEY_QUERY_VALUE);
  std::wstring version_str;
  if (key.ReadValue(google_update::kRegVersionField, &version_str)) {
    version_.reset(Version::GetVersionFromString(WideToASCII(version_str)));
    if (version_.get() != NULL) {
      // The product is installed.  Check for the channel value (absent if not
      // installed/managed by Google Update).
      if (!key.Open(root_key, state_key.c_str(), KEY_QUERY_VALUE) ||
          !channel_.Initialize(key)) {
        channel_.set_value(std::wstring());
      }
    }
  } else {
    version_.reset();
  }
}

const Version& ProductState::version() const {
  DCHECK(version_.get() != NULL);
  return *version_;
}

void ProductState::CopyFrom(const ProductState& other) {
  channel_.set_value(other.channel_.value());
  if (other.version_.get() == NULL)
    version_.reset();
  else
    version_.reset(other.version_->Clone());
}

InstallationState::InstallationState() {
}

// static
int InstallationState::IndexFromDistType(BrowserDistribution::Type type) {
  COMPILE_ASSERT(BrowserDistribution::CHROME_BROWSER == CHROME_BROWSER_INDEX,
                 unexpected_chrome_browser_distribution_value_);
  COMPILE_ASSERT(BrowserDistribution::CHROME_FRAME == CHROME_FRAME_INDEX,
                 unexpected_chrome_frame_distribution_value_);
  DCHECK(type == BrowserDistribution::CHROME_BROWSER ||
         type == BrowserDistribution::CHROME_FRAME);
  return type;
}

void InstallationState::Initialize(const MasterPreferences& prefs) {
  BrowserDistribution* distribution;

  distribution = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BROWSER, prefs);
  InitializeProduct(false, distribution, &user_products_[CHROME_BROWSER_INDEX]);
  InitializeProduct(true, distribution,
      &system_products_[CHROME_BROWSER_INDEX]);

  distribution = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME, prefs);
  InitializeProduct(false, distribution, &user_products_[CHROME_FRAME_INDEX]);
  InitializeProduct(true, distribution, &system_products_[CHROME_FRAME_INDEX]);

  ActivePackageProperties package_properties;
  InitializeMultiPackage(false, package_properties,
      &user_products_[MULTI_PACKAGE_INDEX]);
  InitializeMultiPackage(true, package_properties,
      &system_products_[MULTI_PACKAGE_INDEX]);
}

// static
void InstallationState::InitializeProduct(bool system_install,
                                          BrowserDistribution* distribution,
                                          ProductState* product) {
  product->Initialize(system_install, distribution->GetVersionKey(),
      distribution->GetStateKey());
}

// static
void InstallationState::InitializeMultiPackage(bool system_install,
                                               PackageProperties& properties,
                                               ProductState* product) {
  product->Initialize(system_install, properties.GetVersionKey(),
      properties.GetStateKey());
}

const ProductState* InstallationState::GetMultiPackageState(
    bool system_install) const {
  const ProductState& product_state =
      (system_install ? system_products_ : user_products_)[MULTI_PACKAGE_INDEX];
  return product_state.version_.get() == NULL ? NULL : &product_state;
}

// Included for testing.
void InstallationState::SetMultiPackageState(bool system_install,
    const ProductState& package_state) {
  ProductState& target =
      (system_install ? system_products_ : user_products_)[MULTI_PACKAGE_INDEX];
  target.CopyFrom(package_state);
}

const ProductState* InstallationState::GetProductState(
    bool system_install,
    BrowserDistribution::Type type) const {
  const ProductState& product_state = (system_install ? system_products_ :
      user_products_)[IndexFromDistType(type)];
  return product_state.version_.get() == NULL ? NULL : &product_state;
}

// Included for testing.
void InstallationState::SetProductState(bool system_install,
    BrowserDistribution::Type type, const ProductState& product_state) {
  ProductState& target = (system_install ? system_products_ :
      user_products_)[IndexFromDistType(type)];
  target.CopyFrom(product_state);
}

}  // namespace installer
