// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installer_state.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/work_item.h"

namespace installer {

bool InstallerState::IsMultiInstallUpdate(const MasterPreferences& prefs,
    const InstallationState& machine_state) {
  // First, is the package present?
  const ProductState* package =
      machine_state.GetProductState(level_ == SYSTEM_LEVEL,
                                    BrowserDistribution::CHROME_BINARIES);
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
        machine_state.GetProductState(level_ == SYSTEM_LEVEL, *scan);
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

InstallerState::InstallerState()
    : operation_(UNINITIALIZED),
      multi_package_distribution_(NULL),
      level_(UNKNOWN_LEVEL),
      package_type_(UNKNOWN_PACKAGE_TYPE),
      state_type_(BrowserDistribution::CHROME_BROWSER),
      root_key_(NULL),
      msi_(false),
      verbose_logging_(false) {
}

InstallerState::InstallerState(Level level)
    : operation_(UNINITIALIZED),
      multi_package_distribution_(NULL),
      level_(UNKNOWN_LEVEL),
      package_type_(UNKNOWN_PACKAGE_TYPE),
      state_type_(BrowserDistribution::CHROME_BROWSER),
      root_key_(NULL),
      msi_(false),
      verbose_logging_(false) {
  // Use set_level() so that root_key_ is updated properly.
  set_level(level);
}

void InstallerState::Initialize(const CommandLine& command_line,
                                const MasterPreferences& prefs,
                                const InstallationState& machine_state) {
  bool pref_bool;
  if (!prefs.GetBool(master_preferences::kSystemLevel, &pref_bool))
    pref_bool = false;
  set_level(pref_bool ? SYSTEM_LEVEL : USER_LEVEL);

  if (!prefs.GetBool(master_preferences::kVerboseLogging, &verbose_logging_))
    verbose_logging_ = false;

  if (!prefs.GetBool(master_preferences::kMultiInstall, &pref_bool))
    pref_bool = false;
  set_package_type(pref_bool ? MULTI_PACKAGE : SINGLE_PACKAGE);

  if (!prefs.GetBool(master_preferences::kMsi, &msi_))
    msi_ = false;

  const bool is_uninstall = command_line.HasSwitch(switches::kUninstall);

  if (prefs.install_chrome()) {
    Product* p =
        AddProductFromPreferences(BrowserDistribution::CHROME_BROWSER, prefs,
                                  machine_state);
    VLOG(1) << (is_uninstall ? "Uninstall" : "Install")
            << " distribution: " << p->distribution()->GetApplicationName();
  }
  if (prefs.install_chrome_frame()) {
    Product* p =
        AddProductFromPreferences(BrowserDistribution::CHROME_FRAME, prefs,
                                  machine_state);
    VLOG(1) << (is_uninstall ? "Uninstall" : "Install")
            << " distribution: " << p->distribution()->GetApplicationName();
  }

  BrowserDistribution* operand = NULL;

  if (is_uninstall) {
    operation_ = UNINSTALL;
  } else if (!prefs.is_multi_install()) {
    // For a single-install, the current browser dist is the operand.
    operand = BrowserDistribution::GetDistribution();
    operation_ = SINGLE_INSTALL_OR_UPDATE;
  } else if (IsMultiInstallUpdate(prefs, machine_state)) {
    // Updates driven by Google Update take place under the multi-installer's
    // app guid.
    operand = multi_package_distribution_;
    operation_ = MULTI_UPDATE;
  } else {
    // Initial and over installs will always take place under one of the
    // product app guids.  Chrome Frame's will be used if only Chrome Frame
    // is being installed.  In all other cases, Chrome's is used.
    operation_ = MULTI_INSTALL;
  }

  if (operand == NULL) {
    operand = BrowserDistribution::GetSpecificDistribution(
        prefs.install_chrome() ?
            BrowserDistribution::CHROME_BROWSER :
            BrowserDistribution::CHROME_FRAME);
  }

  state_key_ = operand->GetStateKey();
  state_type_ = operand->GetType();
}

void InstallerState::set_level(Level level) {
  level_ = level;
  switch (level) {
    case USER_LEVEL:
      root_key_ = HKEY_CURRENT_USER;
      break;
    case SYSTEM_LEVEL:
      root_key_ = HKEY_LOCAL_MACHINE;
      break;
    default:
      DCHECK(level == UNKNOWN_LEVEL);
      level_ = UNKNOWN_LEVEL;
      root_key_ = NULL;
      break;
  }
}

void InstallerState::set_package_type(PackageType type) {
  package_type_ = type;
  switch (type) {
    case SINGLE_PACKAGE:
      multi_package_distribution_ = NULL;
      break;
    case MULTI_PACKAGE:
      multi_package_distribution_ =
          BrowserDistribution::GetSpecificDistribution(
              BrowserDistribution::CHROME_BINARIES);
      break;
    default:
      DCHECK(type == UNKNOWN_PACKAGE_TYPE);
      package_type_ = UNKNOWN_PACKAGE_TYPE;
      multi_package_distribution_ = NULL;
      break;
  }
}

// Returns the Chrome binaries directory for multi-install or |dist|'s directory
// otherwise.
FilePath InstallerState::GetDefaultProductInstallPath(
    BrowserDistribution* dist) const {
  DCHECK(dist);
  DCHECK(package_type_ != UNKNOWN_PACKAGE_TYPE);

  if (package_type_ == SINGLE_PACKAGE) {
    return GetChromeInstallPath(system_install(), dist);
  } else {
    return GetChromeInstallPath(system_install(),
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES));
  }
}

// Evaluates a product's eligibility for participation in this operation.
// We never expect these checks to fail, hence they all terminate the process in
// debug builds.  See the log messages for details.
bool InstallerState::CanAddProduct(const Product& product,
                                   const FilePath* product_dir) const {
  switch (package_type_) {
    case SINGLE_PACKAGE:
      if (!products_.empty()) {
        LOG(DFATAL) << "Cannot process more than one single-install product.";
        return false;
      }
      break;
    case MULTI_PACKAGE:
      if (!product.HasOption(kOptionMultiInstall)) {
        LOG(DFATAL) << "Cannot process a single-install product with a "
                       "multi-install state.";
        return false;
      }
      if (FindProduct(product.distribution()->GetType()) != NULL) {
        LOG(DFATAL) << "Cannot process more than one product of the same type.";
        return false;
      }
      if (!target_path_.empty()) {
        FilePath default_dir;
        if (product_dir == NULL)
          default_dir = GetDefaultProductInstallPath(product.distribution());
        if (!FilePath::CompareEqualIgnoreCase(
                (product_dir == NULL ? default_dir : *product_dir).value(),
                target_path_.value())) {
          LOG(DFATAL) << "Cannot process products in different directories.";
          return false;
        }
      }
      break;
    default:
      DCHECK_EQ(UNKNOWN_PACKAGE_TYPE, package_type_);
      break;
  }
  return true;
}

// Adds |product|, installed in |product_dir| to this object's collection.  If
// |product_dir| is NULL, the product's default install location is used.
// Returns NULL if |product| is incompatible with this object.  Otherwise,
// returns a pointer to the product (ownership is held by this object).
Product* InstallerState::AddProductInDirectory(const FilePath* product_dir,
                                               scoped_ptr<Product>* product) {
  DCHECK(product != NULL);
  DCHECK(product->get() != NULL);
  const Product& the_product = *product->get();

  if (!CanAddProduct(the_product, product_dir))
    return NULL;

  if (package_type_ == UNKNOWN_PACKAGE_TYPE) {
    set_package_type(the_product.HasOption(kOptionMultiInstall) ?
                         MULTI_PACKAGE : SINGLE_PACKAGE);
  }

  if (target_path_.empty()) {
    if (product_dir == NULL)
      target_path_ = GetDefaultProductInstallPath(the_product.distribution());
    else
      target_path_ = *product_dir;
  }

  if (state_key_.empty())
    state_key_ = the_product.distribution()->GetStateKey();

  products_.push_back(product->release());
  return products_[products_->size() - 1];
}

Product* InstallerState::AddProduct(scoped_ptr<Product>* product) {
  return AddProductInDirectory(NULL, product);
}

// Adds a product of type |distribution_type| constructed on the basis of
// |prefs|, setting this object's msi flag if the product is represented in
// |machine_state| and is msi-installed.  Returns the product that was added,
// or NULL if |state| is incompatible with this object.  Ownership is not passed
// to the caller.
Product* InstallerState::AddProductFromPreferences(
    BrowserDistribution::Type distribution_type,
    const MasterPreferences& prefs,
    const InstallationState& machine_state) {
  scoped_ptr<Product> product_ptr(
      new Product(BrowserDistribution::GetSpecificDistribution(
          distribution_type)));
  product_ptr->InitializeFromPreferences(prefs);

  Product* product = AddProductInDirectory(NULL, &product_ptr);

  if (product != NULL && !msi_) {
    const ProductState* product_state = machine_state.GetProductState(
        system_install(), distribution_type);
    if (product_state != NULL)
      msi_ = product_state->is_msi();
  }

  return product;
}

Product* InstallerState::AddProductFromState(
    BrowserDistribution::Type type,
    const ProductState& state) {
  scoped_ptr<Product> product_ptr(
      new Product(BrowserDistribution::GetSpecificDistribution(type)));
  product_ptr->InitializeFromUninstallCommand(state.uninstall_command());

  // Strip off <version>/Installer/setup.exe; see GetInstallerDirectory().
  FilePath product_dir = state.GetSetupPath().DirName().DirName().DirName();

  Product* product = AddProductInDirectory(&product_dir, &product_ptr);

  if (product != NULL)
    msi_ |= state.is_msi();

  return product;
}

bool InstallerState::system_install() const {
  DCHECK(level_ == USER_LEVEL || level_ == SYSTEM_LEVEL);
  return level_ == SYSTEM_LEVEL;
}

bool InstallerState::is_multi_install() const {
  DCHECK(package_type_ == SINGLE_PACKAGE || package_type_ == MULTI_PACKAGE);
  return package_type_ != SINGLE_PACKAGE;
}

bool InstallerState::RemoveProduct(const Product* product) {
  ScopedVector<Product>::iterator it =
      std::find(products_.begin(), products_.end(), product);
  if (it != products_.end()) {
    products_->erase(it);
    return true;
  }
  return false;
}

const Product* InstallerState::FindProduct(
    BrowserDistribution::Type distribution_type) const {
  for (Products::const_iterator scan = products_.begin(), end = products_.end();
       scan != end; ++scan) {
     if ((*scan)->is_type(distribution_type))
       return *scan;
  }
  return NULL;
}

Version* InstallerState::GetCurrentVersion(
    const InstallationState& machine_state) const {
  DCHECK(!products_.empty());
  scoped_ptr<Version> current_version;
  const BrowserDistribution::Type prod_type = (package_type_ == MULTI_PACKAGE) ?
      BrowserDistribution::CHROME_BINARIES :
      products_[0]->distribution()->GetType();
  const ProductState* product_state =
      machine_state.GetProductState(level_ == SYSTEM_LEVEL, prod_type);

  if (product_state != NULL) {
    const Version* version = NULL;

    // Be aware that there might be a pending "new_chrome.exe" already in the
    // installation path.  If so, we use old_version, which holds the version of
    // "chrome.exe" itself.
    if (file_util::PathExists(target_path().Append(kChromeNewExe)))
      version = product_state->old_version();

    if (version == NULL)
      version = &product_state->version();

    current_version.reset(version->Clone());
  }

  return current_version.release();
}

FilePath InstallerState::GetInstallerDirectory(const Version& version) const {
  return target_path().Append(ASCIIToWide(version.GetString()))
      .Append(kInstallerDir);
}

void InstallerState::RemoveOldVersionDirectories(
    const Version& latest_version,
    const FilePath& temp_path) const {
  file_util::FileEnumerator version_enum(target_path(), false,
      file_util::FileEnumerator::DIRECTORIES);
  scoped_ptr<Version> version;
  std::vector<FilePath> key_files;

  // We try to delete all directories whose versions are lower than
  // latest_version.
  FilePath next_version = version_enum.Next();
  while (!next_version.empty()) {
    file_util::FileEnumerator::FindInfo find_data = {0};
    version_enum.GetFindInfo(&find_data);
    VLOG(1) << "directory found: " << find_data.cFileName;
    version.reset(Version::GetVersionFromString(
                      WideToASCII(find_data.cFileName)));
    if (version.get() && latest_version.CompareTo(*version) > 0) {
      key_files.clear();
      std::for_each(products_.begin(), products_.end(),
                    std::bind2nd(std::mem_fun(&Product::AddKeyFiles),
                                 &key_files));
      const std::vector<FilePath>::iterator end = key_files.end();
      for (std::vector<FilePath>::iterator scan = key_files.begin();
           scan != end; ++scan) {
        *scan = next_version.Append(*scan);
      }

      VLOG(1) << "Deleting directory: " << next_version.value();

      scoped_ptr<WorkItem> item(
          WorkItem::CreateDeleteTreeWorkItem(next_version, temp_path,
                                             key_files));
      if (!item->Do())
        item->Rollback();
    }

    next_version = version_enum.Next();
  }
}

void InstallerState::AddComDllList(std::vector<FilePath>* com_dll_list) const {
  std::for_each(products_.begin(), products_.end(),
                std::bind2nd(std::mem_fun(&Product::AddComDllList),
                             com_dll_list));
}

bool InstallerState::SetChannelFlags(bool set,
                                     ChannelInfo* channel_info) const {
  bool modified = false;
  for (Products::const_iterator scan = products_.begin(), end = products_.end();
       scan != end; ++scan) {
     modified |= (*scan)->SetChannelFlags(set, channel_info);
  }
  return modified;
}

}  // namespace installer
