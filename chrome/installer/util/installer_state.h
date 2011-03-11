// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_
#define CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"

#if defined(OS_WIN)
#include <windows.h>  // NOLINT
#endif

class CommandLine;
class Version;

namespace installer {

class ChannelInfo;
class InstallationState;
class MasterPreferences;

class ProductState;

typedef std::vector<Product*> Products;

// Encapsulates the state of the current installation operation.  Only valid
// for installs and upgrades (not for uninstalls or non-install commands).
// TODO(grt): Rename to InstallerEngine/Conductor or somesuch?
class InstallerState {
 public:
  enum Level {
    UNKNOWN_LEVEL,
    USER_LEVEL,
    SYSTEM_LEVEL
  };

  enum PackageType {
    UNKNOWN_PACKAGE_TYPE,
    SINGLE_PACKAGE,
    MULTI_PACKAGE
  };

  enum Operation {
    UNINITIALIZED,
    SINGLE_INSTALL_OR_UPDATE,
    MULTI_INSTALL,
    MULTI_UPDATE,
    UNINSTALL
  };

  // Constructs an uninitialized instance; see Initialize().
  InstallerState();

  // Constructs an initialized but empty instance.
  explicit InstallerState(Level level);

  // Initializes this object based on the current operation.
  void Initialize(const CommandLine& command_line,
                  const MasterPreferences& prefs,
                  const InstallationState& machine_state);

  // Adds a product constructed on the basis of |state|, setting this object's
  // msi flag if |state| is msi-installed.  Returns the product that was added,
  // or NULL if |state| is incompatible with this object.  Ownership is not
  // passed to the caller.
  Product* AddProductFromState(BrowserDistribution::Type type,
                               const ProductState& state);

  // Returns the product that was added, or NULL if |product| is incompatible
  // with this object.  Ownership of |product| is taken by this object, while
  // ownership of the return value is not passed to the caller.
  Product* AddProduct(scoped_ptr<Product>* product);

  // Removes |product| from the set of products to be operated on.  The object
  // pointed to by |product| is freed.  Returns false if |product| is not
  // present in the set.
  bool RemoveProduct(const Product* product);

  // The level (user or system) of this operation.
  Level level() const { return level_; }

  // The package type (single or multi) of this operation.
  PackageType package_type() const { return package_type_; }

  // An identifier of this operation.
  Operation operation() const { return operation_; }

  // A convenience method returning level() == SYSTEM_LEVEL.
  // TODO(grt): Eradicate the bool in favor of the enum.
  bool system_install() const;

  // A convenience method returning package_type() == MULTI_PACKAGE.
  // TODO(grt): Eradicate the bool in favor of the enum.
  bool is_multi_install() const;

  // The full path to the place where the operand resides.
  const FilePath& target_path() const { return target_path_; }

  // True if the "msi" preference is set or if a product with the "msi" state
  // flag is set is to be operated on.
  bool is_msi() const { return msi_; }

  // True if the --verbose-logging command-line flag is set or if the
  // verbose_logging master preferences option is true.
  bool verbose_logging() const { return verbose_logging_; }

#if defined(OS_WIN)
  HKEY root_key() const { return root_key_; }
#endif

  // The ClientState key by which we interact with Google Update.
  const std::wstring& state_key() const { return state_key_; }

  // Convenience method to return the type of the BrowserDistribution associated
  // with the ClientState key we will be interacting with.
  BrowserDistribution::Type state_type() const { return state_type_; }

  // Returns the BrowserDistribution instance corresponding to the binaries for
  // this run if we're operating on a multi-package product.
  BrowserDistribution* multi_package_binaries_distribution() const {
    DCHECK(package_type_ == MULTI_PACKAGE);
    DCHECK(multi_package_distribution_ != NULL);
    return multi_package_distribution_;
  }

  const Products& products() const { return products_.get(); }

  // Returns the product of the desired type, or NULL if none found.
  const Product* FindProduct(BrowserDistribution::Type distribution_type) const;

  // Returns the currently installed version in |target_path|, or NULL if no
  // products are installed.  Ownership is passed to the caller.
  Version* GetCurrentVersion(const InstallationState& machine_state) const;

  // Returns the path to the installer under Chrome version folder
  // (for example <target_path>\Google\Chrome\Application\<Version>\Installer)
  FilePath GetInstallerDirectory(const Version& version) const;

  // Try to delete all directories whose versions are lower than
  // |latest_version|.
  void RemoveOldVersionDirectories(const Version& latest_version,
                                   const FilePath& temp_path) const;

  // Adds to |com_dll_list| the list of COM DLLs that are to be registered
  // and/or unregistered. The list may be empty.
  void AddComDllList(std::vector<FilePath>* com_dll_list) const;

  bool SetChannelFlags(bool set, ChannelInfo* channel_info) const;

  // See InstallUtil::UpdateInstallerStage.
  void UpdateStage(installer::InstallerStage stage) const;

  // For a MULTI_INSTALL or MULTI_UPDATE operation, updates the Google Update
  // "ap" values for all products being operated on.
  void UpdateChannels() const;

 protected:
  FilePath GetDefaultProductInstallPath(BrowserDistribution* dist) const;
  bool CanAddProduct(const Product& product, const FilePath* product_dir) const;
  Product* AddProductInDirectory(const FilePath* product_dir,
                                 scoped_ptr<Product>* product);
  Product* AddProductFromPreferences(
      BrowserDistribution::Type distribution_type,
      const MasterPreferences& prefs,
      const InstallationState& machine_state);
  bool IsMultiInstallUpdate(const MasterPreferences& prefs,
                            const InstallationState& machine_state);

  // Sets this object's level and updates the root_key_ accordingly.
  void set_level(Level level);

  // Sets this object's package type and updates the multi_package_distribution_
  // accordingly.
  void set_package_type(PackageType type);

  Operation operation_;
  FilePath target_path_;
  std::wstring state_key_;
  BrowserDistribution::Type state_type_;
  ScopedVector<Product> products_;
  BrowserDistribution* multi_package_distribution_;
  Level level_;
  PackageType package_type_;
#if defined(OS_WIN)
  HKEY root_key_;
#endif
  bool msi_;
  bool verbose_logging_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallerState);
};  // class InstallerState

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_
