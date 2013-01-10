// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_
#define CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/version.h"
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
// This class interprets the command-line arguments and master preferences and
// determines the operations to be performed. For example, the Chrome Binaries
// are automatically added if required in multi-install mode.
// TODO(erikwright): This is now used a fair bit during uninstall, and
// InstallerState::Initialize() contains a lot of code for uninstall. The class
// comment should probably be updated.
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

  // A convenient method returning the presence of the
  // --ensure-google-update-present switch.
  bool ensure_google_update_present() const {
    return ensure_google_update_present_;
  }

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

  // Returns the critical update version if all of the following are true:
  // * --critical-update-version=CUV was specified on the command-line.
  // * current_version == NULL or current_version < CUV.
  // * new_version >= CUV.
  // Otherwise, returns an invalid version.
  Version DetermineCriticalVersion(const Version* current_version,
                                   const Version& new_version) const;

  // Returns whether or not there is currently a Chrome Frame instance running.
  // Note that there isn't a mechanism to lock Chrome Frame in place, so Chrome
  // Frame may either exit or start up after this is called.
  bool IsChromeFrameRunning(const InstallationState& machine_state) const;

  // Returns the path to the installer under Chrome version folder
  // (for example <target_path>\Google\Chrome\Application\<Version>\Installer)
  FilePath GetInstallerDirectory(const Version& version) const;

  // Try to delete all directories under |temp_path| whose versions are less
  // than |new_version| and not equal to |existing_version|. |existing_version|
  // may be NULL.
  void RemoveOldVersionDirectories(const Version& new_version,
                                   Version* existing_version,
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

  // Sets installer result information in the registry for consumption by Google
  // Update.  The InstallerResult value is set to 0 (SUCCESS) or 1
  // (FAILED_CUSTOM_ERROR) depending on whether |status| maps to success or not.
  // |status| itself is written to the InstallerError value.
  // |string_resource_id|, if non-zero, identifies a localized string written to
  // the InstallerResultUIString value.  |launch_cmd|, if non-NULL and
  // non-empty, is written to the InstallerSuccessLaunchCmdLine value.
  void WriteInstallerResult(InstallStatus status,
                            int string_resource_id,
                            const std::wstring* launch_cmd) const;

  // Returns true if this install needs to register an Active Setup command.
  bool RequiresActiveSetup() const;

 protected:
  // Returns true if |file| exists and cannot be opened for exclusive write
  // access.
  static bool IsFileInUse(const FilePath& file);

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

  // Enumerates all files named one of
  // [chrome.exe, old_chrome.exe, new_chrome.exe] in target_path_ and
  // returns their version numbers in a set.
  void GetExistingExeVersions(std::set<std::string>* existing_versions) const;

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
  Version critical_update_version_;
  Level level_;
  PackageType package_type_;
#if defined(OS_WIN)
  HKEY root_key_;
#endif
  bool msi_;
  bool verbose_logging_;
  bool ensure_google_update_present_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallerState);
};  // class InstallerState

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_
