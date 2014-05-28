// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installer_state.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

bool InstallerState::IsMultiInstallUpdate(
    const MasterPreferences& prefs,
    const InstallationState& machine_state) {
  // First, are the binaries present?
  const ProductState* binaries =
      machine_state.GetProductState(level_ == SYSTEM_LEVEL,
                                    BrowserDistribution::CHROME_BINARIES);
  if (binaries == NULL) {
    // The multi-install binaries have not been installed, so they certainly
    // aren't being updated.
    return false;
  }

  if (prefs.install_chrome()) {
    const ProductState* product =
        machine_state.GetProductState(level_ == SYSTEM_LEVEL,
                                      BrowserDistribution::CHROME_BROWSER);
    if (product == NULL) {
      VLOG(2) << "It seems that chrome is being installed for the first time.";
      return false;
    }
    if (!product->channel().Equals(binaries->channel())) {
      VLOG(2) << "It seems that chrome is being over installed.";
      return false;
    }
  }

  VLOG(2) << "It seems that the binaries are being updated.";

  return true;
}

InstallerState::InstallerState()
    : operation_(UNINITIALIZED),
      state_type_(BrowserDistribution::CHROME_BROWSER),
      multi_package_distribution_(NULL),
      level_(UNKNOWN_LEVEL),
      package_type_(UNKNOWN_PACKAGE_TYPE),
      root_key_(NULL),
      msi_(false),
      verbose_logging_(false),
      ensure_google_update_present_(false) {
}

InstallerState::InstallerState(Level level)
    : operation_(UNINITIALIZED),
      state_type_(BrowserDistribution::CHROME_BROWSER),
      multi_package_distribution_(NULL),
      level_(UNKNOWN_LEVEL),
      package_type_(UNKNOWN_PACKAGE_TYPE),
      root_key_(NULL),
      msi_(false),
      verbose_logging_(false),
      ensure_google_update_present_(false) {
  // Use set_level() so that root_key_ is updated properly.
  set_level(level);
}

void InstallerState::Initialize(const CommandLine& command_line,
                                const MasterPreferences& prefs,
                                const InstallationState& machine_state) {
  Clear();

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

  ensure_google_update_present_ =
      command_line.HasSwitch(installer::switches::kEnsureGoogleUpdatePresent);

  const bool is_uninstall = command_line.HasSwitch(switches::kUninstall);

  if (prefs.install_chrome()) {
    Product* p = AddProductFromPreferences(
        BrowserDistribution::CHROME_BROWSER, prefs, machine_state);
    VLOG(1) << (is_uninstall ? "Uninstall" : "Install")
            << " distribution: " << p->distribution()->GetDisplayName();
  }

  if (prefs.install_chrome_app_launcher()) {
    Product* p = AddProductFromPreferences(
        BrowserDistribution::CHROME_APP_HOST, prefs, machine_state);
    VLOG(1) << (is_uninstall ? "Uninstall" : "Install")
            << " distribution: " << p->distribution()->GetDisplayName();
  }

  if (!is_uninstall && is_multi_install()) {
    bool need_binaries = false;
    if (FindProduct(BrowserDistribution::CHROME_APP_HOST)) {
      // App Host will happily use Chrome at system level, or binaries at system
      // level, even if app host is user level.
      const ProductState* chrome_state = machine_state.GetProductState(
          true,  // system level
          BrowserDistribution::CHROME_BROWSER);
      // If Chrome is at system-level, multi- or otherwise. We'll use it.
      if (!chrome_state) {
        const ProductState* binaries_state = machine_state.GetProductState(
            true,  // system level
            BrowserDistribution::CHROME_BINARIES);
        if (!binaries_state)
          need_binaries = true;
      }
    }

    // Chrome multi needs Binaries at its own level.
    if (FindProduct(BrowserDistribution::CHROME_BROWSER))
      need_binaries = true;

    if (need_binaries && !FindProduct(BrowserDistribution::CHROME_BINARIES)) {
      // Force binaries to be installed/updated.
      Product* p = AddProductFromPreferences(
          BrowserDistribution::CHROME_BINARIES, prefs, machine_state);
      VLOG(1) << "Install distribution: "
              << p->distribution()->GetDisplayName();
    }
  }

  if (is_uninstall && prefs.is_multi_install()) {
    if (FindProduct(BrowserDistribution::CHROME_BROWSER)) {
      // Uninstall each product of type |type| listed below based on the
      // presence or absence of |switch_name| in that product's uninstall
      // command.
      const struct {
        BrowserDistribution::Type type;
        const char* switch_name;
        bool switch_expected;
      } conditional_additions[] = {
        // If the App Host is installed, but not the App Launcher, remove it
        // with Chrome. Note however that for system-level Chrome uninstalls,
        // any installed user-level App Host will remain even if there is no
        // App Launcher present (the orphaned app_host.exe will prompt the user
        // for further action when executed).
        { BrowserDistribution::CHROME_APP_HOST,
          switches::kChromeAppLauncher,
          false },
      };

      for (size_t i = 0; i < arraysize(conditional_additions); ++i) {
        const ProductState* product_state = machine_state.GetProductState(
            system_install(), conditional_additions[i].type);
        if (product_state != NULL &&
            product_state->uninstall_command().HasSwitch(
                conditional_additions[i].switch_name) ==
                    conditional_additions[i].switch_expected &&
            !FindProduct(conditional_additions[i].type)) {
          Product* p = AddProductFromPreferences(
              conditional_additions[i].type, prefs, machine_state);
          VLOG(1) << "Uninstall distribution: "
                  << p->distribution()->GetDisplayName();
        }
      }
    }

    bool keep_binaries = false;
    // Look for a multi-install product that is not the binaries and that is not
    // being uninstalled. If not found, binaries are uninstalled too.
    for (size_t i = 0; i < BrowserDistribution::NUM_TYPES; ++i) {
      BrowserDistribution::Type type =
          static_cast<BrowserDistribution::Type>(i);

      if (type == BrowserDistribution::CHROME_BINARIES)
        continue;

      const ProductState* product_state =
          machine_state.GetProductState(system_install(), type);
      if (product_state == NULL) {
        // The product is not installed.
        continue;
      }

      if (!product_state->is_multi_install() &&
          type != BrowserDistribution::CHROME_BROWSER) {
        // The product is not sharing the binaries. It is ordinarily impossible
        // for single-install Chrome to be installed along with any
        // multi-install product. Treat single-install Chrome the same as any
        // multi-install product just in case the impossible happens.
        continue;
      }

      // The product is installed.

      if (!FindProduct(type)) {
        // The product is not being uninstalled.
        if (type != BrowserDistribution::CHROME_APP_HOST) {
          keep_binaries = true;
          break;
        } else {
          // If binaries/chrome are at system-level, we can discard them at
          // user-level...
          if (!machine_state.GetProductState(
                  true,  // system-level
                  BrowserDistribution::CHROME_BROWSER) &&
              !machine_state.GetProductState(
                  true,  // system-level
                  BrowserDistribution::CHROME_BINARIES)) {
            // ... otherwise keep them.
            keep_binaries = true;
            break;
          }

        }
      }

      // The product is being uninstalled.
    }
    if (!keep_binaries &&
        machine_state.GetProductState(system_install(),
                                      BrowserDistribution::CHROME_BINARIES)) {
      Product* p = AddProductFromPreferences(
          BrowserDistribution::CHROME_BINARIES, prefs, machine_state);
      VLOG(1) << (is_uninstall ? "Uninstall" : "Install")
              << " distribution: " << p->distribution()->GetDisplayName();
    }
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
    operation_ = MULTI_INSTALL;
  }

  // Initial, over, and un-installs will take place under one of the product app
  // guids (Chrome, App Host, or Binaries, in order of preference).
  if (operand == NULL) {
    BrowserDistribution::Type operand_distribution_type =
        BrowserDistribution::CHROME_BINARIES;
    if (prefs.install_chrome())
      operand_distribution_type = BrowserDistribution::CHROME_BROWSER;
    else if (prefs.install_chrome_app_launcher())
      operand_distribution_type = BrowserDistribution::CHROME_APP_HOST;

    operand = BrowserDistribution::GetSpecificDistribution(
        operand_distribution_type);
  }

  state_key_ = operand->GetStateKey();
  state_type_ = operand->GetType();

  // Parse --critical-update-version=W.X.Y.Z
  std::string critical_version_value(
      command_line.GetSwitchValueASCII(switches::kCriticalUpdateVersion));
  critical_update_version_ = Version(critical_version_value);
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
base::FilePath InstallerState::GetDefaultProductInstallPath(
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
                                   const base::FilePath* product_dir) const {
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
        base::FilePath default_dir;
        if (product_dir == NULL)
          default_dir = GetDefaultProductInstallPath(product.distribution());
        if (!base::FilePath::CompareEqualIgnoreCase(
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
Product* InstallerState::AddProductInDirectory(
    const base::FilePath* product_dir,
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
  return products_[products_.size() - 1];
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
  base::FilePath product_dir =
      state.GetSetupPath().DirName().DirName().DirName();

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
    products_.weak_erase(it);
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
  // If we're doing a multi-install, the current version may be either an
  // existing multi or an existing single product that is being migrated
  // in place (i.e., Chrome).  In the latter case, there is no existing
  // CHROME_BINARIES installation so we need to search for the product.
  BrowserDistribution::Type prod_type;
  if (package_type_ == MULTI_PACKAGE) {
    prod_type = BrowserDistribution::CHROME_BINARIES;
    if (machine_state.GetProductState(level_ == SYSTEM_LEVEL,
                                      prod_type) == NULL) {
      // Search for a product on which we're operating that is installed in our
      // target directory.
      Products::const_iterator end = products().end();
      for (Products::const_iterator scan = products().begin(); scan != end;
           ++scan) {
        BrowserDistribution::Type product_type =
            (*scan)->distribution()->GetType();
        const ProductState* state =
            machine_state.GetProductState(level_ == SYSTEM_LEVEL, product_type);
        if (state != NULL && target_path_.IsParent(state->GetSetupPath())) {
          prod_type = product_type;
          break;
        }
      }
    }
  } else {
    prod_type = products_[0]->distribution()->GetType();
  }
  const ProductState* product_state =
      machine_state.GetProductState(level_ == SYSTEM_LEVEL, prod_type);

  if (product_state != NULL) {
    const Version* version = NULL;

    // Be aware that there might be a pending "new_chrome.exe" already in the
    // installation path.  If so, we use old_version, which holds the version of
    // "chrome.exe" itself.
    if (base::PathExists(target_path().Append(kChromeNewExe)))
      version = product_state->old_version();

    if (version == NULL)
      version = &product_state->version();

    current_version.reset(new Version(*version));
  }

  return current_version.release();
}

Version InstallerState::DetermineCriticalVersion(
    const Version* current_version,
    const Version& new_version) const {
  DCHECK(current_version == NULL || current_version->IsValid());
  DCHECK(new_version.IsValid());
  if (critical_update_version_.IsValid() &&
      (current_version == NULL ||
       (current_version->CompareTo(critical_update_version_) < 0)) &&
      new_version.CompareTo(critical_update_version_) >= 0) {
    return critical_update_version_;
  }
  return Version();
}

bool InstallerState::IsChromeFrameRunning(
    const InstallationState& machine_state) const {
  return AnyExistsAndIsInUse(machine_state, CHROME_FRAME_DLL);
}

bool InstallerState::AreBinariesInUse(
    const InstallationState& machine_state) const {
  return AnyExistsAndIsInUse(
      machine_state,
      (CHROME_FRAME_HELPER_EXE | CHROME_FRAME_HELPER_DLL |
       CHROME_FRAME_DLL | CHROME_DLL));
}

base::FilePath InstallerState::GetInstallerDirectory(
    const Version& version) const {
  return target_path().Append(base::ASCIIToWide(version.GetString()))
      .Append(kInstallerDir);
}

// static
bool InstallerState::IsFileInUse(const base::FilePath& file) {
  // Call CreateFile with a share mode of 0 which should cause this to fail
  // with ERROR_SHARING_VIOLATION if the file exists and is in-use.
  return !base::win::ScopedHandle(CreateFile(file.value().c_str(),
                                             GENERIC_WRITE, 0, NULL,
                                             OPEN_EXISTING, 0, 0)).IsValid();
}

void InstallerState::Clear() {
  operation_ = UNINITIALIZED;
  target_path_.clear();
  state_key_.clear();
  state_type_ = BrowserDistribution::CHROME_BROWSER;
  products_.clear();
  multi_package_distribution_ = NULL;
  critical_update_version_ = base::Version();
  level_ = UNKNOWN_LEVEL;
  package_type_ = UNKNOWN_PACKAGE_TYPE;
  root_key_ = NULL;
  msi_ = false;
  verbose_logging_ = false;
  ensure_google_update_present_ = false;
}

bool InstallerState::AnyExistsAndIsInUse(
    const InstallationState& machine_state,
    uint32 file_bits) const {
  static const wchar_t* const kBinaryFileNames[] = {
    kChromeDll,
    kChromeFrameDll,
    kChromeFrameHelperDll,
    kChromeFrameHelperExe,
  };
  DCHECK_NE(file_bits, 0U);
  DCHECK_LT(file_bits, 1U << NUM_BINARIES);
  COMPILE_ASSERT(CHROME_DLL == 1, no_youre_out_of_order);
  COMPILE_ASSERT(CHROME_FRAME_DLL == 2, no_youre_out_of_order);
  COMPILE_ASSERT(CHROME_FRAME_HELPER_DLL == 4, no_youre_out_of_order);
  COMPILE_ASSERT(CHROME_FRAME_HELPER_EXE == 8, no_youre_out_of_order);

  // Check only for the current version (i.e., the version we are upgrading
  // _from_). Later versions from pending in-use updates need not be checked
  // since the current version is guaranteed to be in use if any such are.
  bool in_use = false;
  scoped_ptr<Version> current_version(GetCurrentVersion(machine_state));
  if (!current_version)
    return false;
  base::FilePath directory(
      target_path().AppendASCII(current_version->GetString()));
  for (int i = 0; i < NUM_BINARIES; ++i) {
    if (!(file_bits & (1U << i)))
      continue;
    base::FilePath file(directory.Append(kBinaryFileNames[i]));
    if (base::PathExists(file) && IsFileInUse(file))
      return true;
  }
  return false;
}

void InstallerState::GetExistingExeVersions(
    std::set<std::string>* existing_versions) const {

  static const wchar_t* const kChromeFilenames[] = {
    installer::kChromeExe,
    installer::kChromeNewExe,
    installer::kChromeOldExe,
  };

  for (int i = 0; i < arraysize(kChromeFilenames); ++i) {
    base::FilePath chrome_exe(target_path().Append(kChromeFilenames[i]));
    scoped_ptr<FileVersionInfo> file_version_info(
        FileVersionInfo::CreateFileVersionInfo(chrome_exe));
    if (file_version_info) {
      base::string16 version_string = file_version_info->file_version();
      if (!version_string.empty() && base::IsStringASCII(version_string))
        existing_versions->insert(base::UTF16ToASCII(version_string));
    }
  }
}

void InstallerState::RemoveOldVersionDirectories(
    const Version& new_version,
    Version* existing_version,
    const base::FilePath& temp_path) const {
  Version version;
  scoped_ptr<WorkItem> item;

  std::set<std::string> existing_version_strings;
  existing_version_strings.insert(new_version.GetString());
  if (existing_version)
    existing_version_strings.insert(existing_version->GetString());

  // Make sure not to delete any version dir that is "referenced" by an existing
  // Chrome executable.
  GetExistingExeVersions(&existing_version_strings);

  // Try to delete all directories that are not in the set we care to keep.
  base::FileEnumerator version_enum(target_path(), false,
                                    base::FileEnumerator::DIRECTORIES);
  for (base::FilePath next_version = version_enum.Next(); !next_version.empty();
       next_version = version_enum.Next()) {
    base::FilePath dir_name(next_version.BaseName());
    version = Version(base::UTF16ToASCII(dir_name.value()));
    // Delete the version folder if it is less than the new version and not
    // equal to the old version (if we have an old version).
    if (version.IsValid() &&
        existing_version_strings.count(version.GetString()) == 0) {
      // Note: temporarily log old version deletion at ERROR level to make it
      // more likely we see this in the installer log.
      LOG(ERROR) << "Deleting old version directory: " << next_version.value();

      // Attempt to recursively delete the old version dir.
      bool delete_succeeded = base::DeleteFile(next_version, true);

      // Note: temporarily log old version deletion at ERROR level to make it
      // more likely we see this in the installer log.
      LOG_IF(ERROR, !delete_succeeded)
          << "Failed to delete old version directory: " << next_version.value();
    }
  }
}

void InstallerState::AddComDllList(
    std::vector<base::FilePath>* com_dll_list) const {
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

void InstallerState::UpdateStage(installer::InstallerStage stage) const {
  InstallUtil::UpdateInstallerStage(system_install(), state_key_, stage);
}

void InstallerState::UpdateChannels() const {
  if (operation_ != MULTI_INSTALL && operation_ != MULTI_UPDATE) {
    VLOG(1) << "InstallerState::UpdateChannels noop: " << operation_;
    return;
  }

  // Update the "ap" value for the product being installed/updated.  We get the
  // current value from the registry since the InstallationState instance used
  // by the bulk of the installer does not track changes made by UpdateStage.
  // Create the app's ClientState key if it doesn't exist.
  ChannelInfo channel_info;
  base::win::RegKey state_key;
  LONG result =
      state_key.Create(root_key_,
                       state_key_.c_str(),
                       KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result == ERROR_SUCCESS) {
    channel_info.Initialize(state_key);

    // This is a multi-install product.
    bool modified = channel_info.SetMultiInstall(true);

    // Add the appropriate modifiers for all products and their options.
    modified |= SetChannelFlags(true, &channel_info);

    VLOG(1) << "ap: " << channel_info.value();

    // Write the results if needed.
    if (modified)
      channel_info.Write(&state_key);

    // Remove the -stage: modifier since we don't want to propagate that to the
    // other app_guids.
    channel_info.SetStage(NULL);

    // Synchronize the other products and the package with this one.
    ChannelInfo other_info;
    for (int i = 0; i < BrowserDistribution::NUM_TYPES; ++i) {
      BrowserDistribution::Type type =
          static_cast<BrowserDistribution::Type>(i);
      // Skip the app_guid we started with.
      if (type == state_type_)
        continue;
      BrowserDistribution* dist = NULL;
      // Always operate on the binaries.
      if (i == BrowserDistribution::CHROME_BINARIES) {
        dist = multi_package_distribution_;
      } else {
        const Product* product = FindProduct(type);
        // Skip this one if it's for a product we're not operating on.
        if (product == NULL)
          continue;
        dist = product->distribution();
      }
      result =
          state_key.Create(root_key_,
                           dist->GetStateKey().c_str(),
                           KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY);
      if (result == ERROR_SUCCESS) {
        other_info.Initialize(state_key);
        if (!other_info.Equals(channel_info))
          channel_info.Write(&state_key);
      } else {
        LOG(ERROR) << "Failed opening key " << dist->GetStateKey()
                   << " to update app channels; result: " << result;
      }
    }
  } else {
    LOG(ERROR) << "Failed opening key " << state_key_
               << " to update app channels; result: " << result;
  }
}

void InstallerState::WriteInstallerResult(
    InstallStatus status,
    int string_resource_id,
    const std::wstring* const launch_cmd) const {
  // Use a no-rollback list since this is a best-effort deal.
  scoped_ptr<WorkItemList> install_list(
      WorkItem::CreateNoRollbackWorkItemList());
  const bool system_install = this->system_install();
  // Write the value for all products upon which we're operating.
  Products::const_iterator end = products().end();
  for (Products::const_iterator scan = products().begin(); scan != end;
       ++scan) {
    InstallUtil::AddInstallerResultItems(
        system_install, (*scan)->distribution()->GetStateKey(), status,
        string_resource_id, launch_cmd, install_list.get());
  }
  // And for the binaries if this is a multi-install.
  if (is_multi_install()) {
    InstallUtil::AddInstallerResultItems(
        system_install, multi_package_binaries_distribution()->GetStateKey(),
        status, string_resource_id, launch_cmd, install_list.get());
  }
  if (!install_list->Do())
    LOG(ERROR) << "Failed to record installer error information in registry.";
}

bool InstallerState::RequiresActiveSetup() const {
  return system_install() && FindProduct(BrowserDistribution::CHROME_BROWSER);
}

}  // namespace installer
