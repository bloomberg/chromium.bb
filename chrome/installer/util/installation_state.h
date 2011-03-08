// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_
#define CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "chrome/installer/util/app_commands.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"

class Version;

namespace base {
namespace win {
class RegKey;
}
}

namespace installer {

class InstallationState;
class MasterPreferences;

// A representation of a product's state on the machine based on the contents
// of the Windows registry.
// TODO(grt): Pull this out into its own file.
class ProductState {
 public:
  ProductState();

  // Returns true if the product is installed (i.e., the product's Clients key
  // exists and has a "pv" value); false otherwise.
  bool Initialize(bool system_install,
                  BrowserDistribution::Type type);
  bool Initialize(bool system_install,
                  BrowserDistribution* distribution);

  // Returns the product's channel info (i.e., the Google Update "ap" value).
  const ChannelInfo& channel() const { return channel_; }

  // Returns the path to the product's "setup.exe"; may be empty.
  FilePath GetSetupPath() const;

  // Returns the product's version.  This method may only be called on an
  // instance that has been initialized for an installed product.
  const Version& version() const;

  // Returns the current version of the product if a new version is awaiting
  // update; may be NULL.  Ownership of a returned value is not passed to the
  // caller.
  const Version* old_version() const { return old_version_.get(); }

  // Returns the brand code the product is currently installed with.
  const std::wstring& brand() const { return brand_; }

  // Returns the command to be used to update to the new version that is
  // awaiting update; may be empty.
  const std::wstring& rename_cmd() const { return rename_cmd_; }

  // True if the "msi" value in the ClientState key is present and non-zero.
  bool is_msi() const { return msi_; }

  // The command to uninstall the product; may be empty.
  const CommandLine& uninstall_command() const { return uninstall_command_; }

  // True if |uninstall_command| contains --multi-install.
  bool is_multi_install() const { return multi_install_; }

  // Returns the set of Google Update commands.
  const AppCommands& commands() const { return commands_; }

  // Returns this object a la operator=().
  ProductState& CopyFrom(const ProductState& other);

  // Clears the state of this object.
  void Clear();

 protected:
  static bool InitializeCommands(const base::win::RegKey& version_key,
                                 AppCommands* commands);

  ChannelInfo channel_;
  scoped_ptr<Version> version_;
  scoped_ptr<Version> old_version_;
  std::wstring brand_;
  std::wstring rename_cmd_;
  CommandLine uninstall_command_;
  AppCommands commands_;
  bool msi_;
  bool multi_install_;

 private:
  friend class InstallationState;

  DISALLOW_COPY_AND_ASSIGN(ProductState);
};  // class ProductState

// Encapsulates the state of all products on the system.
// TODO(grt): Rename this to MachineState and put it in its own file.
class InstallationState {
 public:
  InstallationState();

  // Initializes this object with the machine's current state.
  void Initialize();

  // Returns the state of a product or NULL if not installed.
  // Caller does NOT assume ownership of returned pointer.
  const ProductState* GetProductState(bool system_install,
                                      BrowserDistribution::Type type) const;

  // Returns the state of a product, even one that has not yet been installed.
  // This is useful during first install, when some but not all ProductState
  // information has been written by Omaha. Notably absent from the
  // ProductState returned here are the version numbers. Do NOT try to access
  // the version numbers from a ProductState returned by this method.
  // Caller does NOT assume ownership of returned pointer. This method will
  // never return NULL.
  const ProductState* GetNonVersionedProductState(
      bool system_install, BrowserDistribution::Type type) const;

 protected:
  enum {
    CHROME_BROWSER_INDEX,
    CHROME_FRAME_INDEX,
    CHROME_BINARIES_INDEX,
    NUM_PRODUCTS
  };

  static int IndexFromDistType(BrowserDistribution::Type type);

  ProductState user_products_[NUM_PRODUCTS];
  ProductState system_products_[NUM_PRODUCTS];

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallationState);
};  // class InstallationState

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALLATION_STATE_H_
