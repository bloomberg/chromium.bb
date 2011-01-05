// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_
#define CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"

class Version;

namespace installer {

class InstallationState;
class MasterPreferences;
class PackageProperties;

class ProductState {
 public:
  ProductState();
  void Initialize(bool system_install,
                  const std::wstring& version_key,
                  const std::wstring& state_key);

  const ChannelInfo& channel() const { return channel_; }
  ChannelInfo& channel() { return channel_; }

  const Version& version() const;
  // Takes ownership of |version|.
  void set_version(Version* version) { version_.reset(version); }

  void CopyFrom(const ProductState& other);

 private:
  friend class InstallationState;

  ChannelInfo channel_;
  scoped_ptr<Version> version_;
  DISALLOW_COPY_AND_ASSIGN(ProductState);
};  // class ProductState

// Encapsulates the state of all products on the system.
class InstallationState {
 public:
  InstallationState();

  // Initializes |this| with the machine's current state.
  void Initialize(const MasterPreferences& prefs);

  // Returns the state of the multi-installer package or NULL if no
  // multi-install products are installed.
  const ProductState* GetMultiPackageState(bool system_install) const;

  // Sets the state of the multi-installer package; intended for tests.
  void SetMultiPackageState(bool system_install,
                            const ProductState& package_state);

  // Returns the state of a product or NULL if not installed.
  const ProductState* GetProductState(bool system_install,
                                      BrowserDistribution::Type type) const;

  // Sets the state of a product; indended for tests.
  void SetProductState(bool system_install,
                       BrowserDistribution::Type type,
                       const ProductState& product_state);

 private:
  enum {
    CHROME_BROWSER_INDEX,
    CHROME_FRAME_INDEX,
    MULTI_PACKAGE_INDEX,
    NUM_PRODUCTS
  };

  static int IndexFromDistType(BrowserDistribution::Type type);
  static void InitializeProduct(bool system_install,
                                BrowserDistribution* distribution,
                                ProductState* product);
  static void InitializeMultiPackage(bool system_install,
                                     PackageProperties& properties,
                                     ProductState* product);

  ProductState user_products_[NUM_PRODUCTS];
  ProductState system_products_[NUM_PRODUCTS];

  DISALLOW_COPY_AND_ASSIGN(InstallationState);
};  // class InstallationState

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_
