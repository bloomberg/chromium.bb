// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <msi.h>
#include <shellapi.h>
#include <shlobj.h>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/setup/chrome_frame_quick_enable.h"
#include "chrome/installer/setup/chrome_frame_ready_mode.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/google_update_util.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installation_validator.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"

#include "installer_util_strings.h"  // NOLINT

using installer::InstallerState;
using installer::InstallationState;
using installer::InstallationValidator;
using installer::Product;
using installer::ProductState;
using installer::Products;
using installer::MasterPreferences;

const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

namespace {

// This method unpacks and uncompresses the given archive file. For Chrome
// install we are creating a uncompressed archive that contains all the files
// needed for the installer. This uncompressed archive is later compressed.
//
// This method first uncompresses archive specified by parameter "archive"
// and assumes that it will result in an uncompressed full archive file
// (chrome.7z) or uncompressed archive patch file (chrome_patch.diff). If it
// is patch file, it is applied to the old archive file that should be
// present on the system already. As the final step the new archive file
// is unpacked in the path specified by parameter "output_directory".
DWORD UnPackArchive(const FilePath& archive,
                    const InstallerState& installer_state,
                    const FilePath& temp_path,
                    const FilePath& output_directory,
                    installer::ArchiveType* archive_type) {
  DCHECK(archive_type);

  installer_state.UpdateStage(installer::UNCOMPRESSING);

  // First uncompress the payload. This could be a differential
  // update (patch.7z) or full archive (chrome.7z). If this uncompress fails
  // return with error.
  string16 unpacked_file;
  int32 ret = LzmaUtil::UnPackArchive(archive.value(), temp_path.value(),
                                      &unpacked_file);
  if (ret != NO_ERROR)
    return ret;

  FilePath uncompressed_archive(temp_path.Append(installer::kChromeArchive));
  scoped_ptr<Version> archive_version(
      installer::GetMaxVersionFromArchiveDir(installer_state.target_path()));

  // Check if this is differential update and if it is, patch it to the
  // installer archive that should already be on the machine. We assume
  // it is a differential installer if chrome.7z is not found.
  if (!file_util::PathExists(uncompressed_archive)) {
    *archive_type = installer::INCREMENTAL_ARCHIVE_TYPE;
    VLOG(1) << "Differential patch found. Applying to existing archive.";
    if (!archive_version.get()) {
      LOG(ERROR) << "Can not use differential update when Chrome is not "
                 << "installed on the system.";
      return installer::CHROME_NOT_INSTALLED;
    }

    FilePath existing_archive(installer_state.target_path().AppendASCII(
        archive_version->GetString()));
    existing_archive = existing_archive.Append(installer::kInstallerDir);
    existing_archive = existing_archive.Append(installer::kChromeArchive);
    if (int i = installer::ApplyDiffPatch(existing_archive,
                                          FilePath(unpacked_file),
                                          uncompressed_archive,
                                          &installer_state)) {
      LOG(ERROR) << "Binary patching failed with error " << i;
      return i;
    }
  } else {
    *archive_type = installer::FULL_ARCHIVE_TYPE;
  }

  installer_state.UpdateStage(installer::UNPACKING);

  // Unpack the uncompressed archive.
  return LzmaUtil::UnPackArchive(uncompressed_archive.value(),
      output_directory.value(), &unpacked_file);
}

// In multi-install, adds all products to |installer_state| that are
// multi-installed and must be updated along with the products already present
// in |installer_state|.
void AddExistingMultiInstalls(const InstallationState& original_state,
                              InstallerState* installer_state) {
  if (installer_state->is_multi_install()) {
    for (size_t i = 0; i < BrowserDistribution::NUM_TYPES; ++i) {
      BrowserDistribution::Type type =
          static_cast<BrowserDistribution::Type>(i);

      if (!installer_state->FindProduct(type)) {
        const ProductState* state =
            original_state.GetProductState(installer_state->system_install(),
                                           type);
        if ((state != NULL) && state->is_multi_install()) {
          installer_state->AddProductFromState(type, *state);
          VLOG(1) << "Product already installed and must be included: "
                  << BrowserDistribution::GetSpecificDistribution(type)->
                         GetAppShortCutName();
        }
      }
    }
  }
}

// This function is called when --rename-chrome-exe option is specified on
// setup.exe command line. This function assumes an in-use update has happened
// for Chrome so there should be a file called new_chrome.exe on the file
// system and a key called 'opv' in the registry. This function will move
// new_chrome.exe to chrome.exe and delete 'opv' key in one atomic operation.
// This function also deletes elevation policies associated with the old version
// if they exist.
installer::InstallStatus RenameChromeExecutables(
    const InstallationState& original_state,
    InstallerState* installer_state) {
  // See what products are already installed in multi mode.  When we do the
  // rename for multi installs, we must update all installations since they
  // share the binaries.
  AddExistingMultiInstalls(original_state, installer_state);
  const FilePath &target_path = installer_state->target_path();
  FilePath chrome_exe(target_path.Append(installer::kChromeExe));
  FilePath chrome_new_exe(target_path.Append(installer::kChromeNewExe));
  FilePath chrome_old_exe(target_path.Append(installer::kChromeOldExe));

  // Create a temporary backup directory on the same volume as chrome.exe so
  // that moving in-use files doesn't lead to trouble.
  installer::SelfCleaningTempDir temp_path;
  if (!temp_path.Initialize(target_path.DirName(),
                            installer::kInstallTempDir)) {
    PLOG(ERROR) << "Failed to create Temp directory "
                << target_path.DirName()
                       .Append(installer::kInstallTempDir).value();
    return installer::RENAME_FAILED;
  }
  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  // Move chrome.exe to old_chrome.exe, then move new_chrome.exe to chrome.exe.
  install_list->AddMoveTreeWorkItem(chrome_exe.value(),
                                    chrome_old_exe.value(),
                                    temp_path.path().value(),
                                    WorkItem::ALWAYS_MOVE);
  install_list->AddMoveTreeWorkItem(chrome_new_exe.value(),
                                    chrome_exe.value(),
                                    temp_path.path().value(),
                                    WorkItem::ALWAYS_MOVE);
  install_list->AddDeleteTreeWorkItem(chrome_new_exe, temp_path.path());
  // Delete an elevation policy associated with the old version, should one
  // exist.
  if (installer_state->FindProduct(BrowserDistribution::CHROME_FRAME)) {
    installer::AddDeleteOldIELowRightsPolicyWorkItems(*installer_state,
                                                      install_list.get());
  }
  // old_chrome.exe is still in use in most cases, so ignore failures here.
  install_list->AddDeleteTreeWorkItem(chrome_old_exe, temp_path.path())->
      set_ignore_failure(true);

  // Add work items to delete the "opv", "cpv", and "cmd" values from all
  // products we're operating on (which including the multi-install binaries).
  const Products& products = installer_state->products();
  HKEY reg_root = installer_state->root_key();
  string16 version_key;
  for (Products::const_iterator it = products.begin(); it < products.end();
       ++it) {
    version_key = (*it)->distribution()->GetVersionKey();
    install_list->AddDeleteRegValueWorkItem(
        reg_root, version_key, google_update::kRegOldVersionField);
    install_list->AddDeleteRegValueWorkItem(
        reg_root, version_key, google_update::kRegCriticalVersionField);
    install_list->AddDeleteRegValueWorkItem(
        reg_root, version_key, google_update::kRegRenameCmdField);
  }
  installer::InstallStatus ret = installer::RENAME_SUCCESSFUL;
  if (!install_list->Do()) {
    LOG(ERROR) << "Renaming of executables failed. Rolling back any changes.";
    install_list->Rollback();
    ret = installer::RENAME_FAILED;
  }
  // temp_path's dtor will take care of deleting or scheduling itself for
  // deletion at reboot when this scope closes.
  VLOG(1) << "Deleting temporary directory " << temp_path.path().value();

  return ret;
}

// For each product that is being updated (i.e., already installed at an earlier
// version), see if that product has an update policy override that differs from
// that for the binaries.  If any are found, fail with an error indicating that
// the Group Policy settings are in an inconsistent state.  Do not do this test
// for same-version installs, since it would be unkind to block attempts to
// repair a corrupt installation.  This function returns false when installation
// should be halted, in which case |status| contains the relevant exit code and
// the proper installer result has been written to the registry.
bool CheckGroupPolicySettings(const InstallationState& original_state,
                              const InstallerState& installer_state,
                              const Version& new_version,
                              installer::InstallStatus* status) {
#if !defined(GOOGLE_CHROME_BUILD)
  // Chromium builds are not updated via Google Update, so there are no
  // Group Policy settings to consult.
  return true;
#else
  DCHECK(status);

  // Single installs are always in good shape.
  if (!installer_state.is_multi_install())
    return true;

  bool settings_are_valid = true;
  const bool is_system_install = installer_state.system_install();
  BrowserDistribution* const binaries_dist =
      installer_state.multi_package_binaries_distribution();

  // Get the update policy for the binaries.
  const GoogleUpdateSettings::UpdatePolicy binaries_policy =
      GoogleUpdateSettings::GetAppUpdatePolicy(binaries_dist->GetAppGuid(),
                                               NULL);

  // Check for differing update policies for all of the products being updated.
  const Products& products = installer_state.products();
  Products::const_iterator scan = products.begin();
  for (Products::const_iterator end = products.end(); scan != end; ++scan) {
    BrowserDistribution* dist = (*scan)->distribution();
    const ProductState* product_state =
        original_state.GetProductState(is_system_install, dist->GetType());
    // Is an earlier version of this product already installed?
    if (product_state != NULL &&
        product_state->version().CompareTo(new_version) < 0) {
      bool is_overridden = false;
      GoogleUpdateSettings::UpdatePolicy app_policy =
          GoogleUpdateSettings::GetAppUpdatePolicy(dist->GetAppGuid(),
                                                   &is_overridden);
      if (is_overridden && app_policy != binaries_policy) {
        LOG(ERROR) << "Found legacy Group Policy setting for "
                   << dist->GetAppShortCutName() << " (value: " << app_policy
                   << ") that does not match the setting for "
                   << binaries_dist->GetAppShortCutName()
                   << " (value: " << binaries_policy << ").";
        settings_are_valid = false;
      }
    }
  }

  if (!settings_are_valid) {
    // TODO(grt): add " See http://goo.gl/+++ for details." to the end of this
    // log message and to the IDS_INSTALL_INCONSISTENT_UPDATE_POLICY string once
    // we have a help center article that explains why this error is being
    // reported and how to resolve it.
    LOG(ERROR) << "Cannot apply update on account of inconsistent "
                  "Google Update Group Policy settings. Use the Group Policy "
                  "Editor to set the update policy override for the "
               << binaries_dist->GetAppShortCutName()
               << " application and try again.";
    *status = installer::INCONSISTENT_UPDATE_POLICY;
    installer_state.WriteInstallerResult(
        *status, IDS_INSTALL_INCONSISTENT_UPDATE_POLICY_BASE, NULL);
  }

  return settings_are_valid;
#endif  // defined(GOOGLE_CHROME_BUILD)
}

// If Chrome Frame is being installed by itself in multi-mode, non-ready-mode:
//   - If a non-multi Chrome Frame installation is present, fail.
// If Chrome Frame is being installed by itself in multi-mode, ready-mode:
//   - If no Chrome installation is present, fail.
//   - If a Chrome installation is present, add it to the set of products to
//     install.
// If Chrome Frame is being installed with Chrome in multi-mode, ready-mode:
//   - If a non-multi Chrome Frame installation is present, Chrome Frame is
//     removed from |installer_state|'s list of products (thereby preserving
//     the existing SxS install).
//   - If a multi Chrome Frame installation is present, its options are
//     preserved (i.e., the --ready-mode command-line option is ignored).
// If any product is being installed in single-mode that already exists in
// multi-mode, fail.
bool CheckMultiInstallConditions(const InstallationState& original_state,
                                 InstallerState* installer_state,
                                 installer::InstallStatus* status) {
  const Products& products = installer_state->products();
  DCHECK(products.size());

  const bool system_level = installer_state->system_install();

  if (installer_state->is_multi_install()) {
    const Product* chrome =
        installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER);
    const Product* app_host =
        installer_state->FindProduct(BrowserDistribution::CHROME_APP_HOST);
    const Product* binaries =
        installer_state->FindProduct(BrowserDistribution::CHROME_BINARIES);
    const Product* chrome_frame =
        installer_state->FindProduct(BrowserDistribution::CHROME_FRAME);
    const ProductState* cf_state =
        original_state.GetProductState(system_level,
                                       BrowserDistribution::CHROME_FRAME);
    const ProductState* chrome_state =
        original_state.GetProductState(system_level,
                                       BrowserDistribution::CHROME_BROWSER);

    if (!binaries) {
      // This will only be hit if --multi-install is given with no products, or
      // if the app host is being installed and doesn't need the binaries at
      // user-level.
      // The former case might be due to a request by an orphaned Application
      // Host to re-install the binaries. Thus we add them to the installation.
      // The latter case is fine and we let it be.
      // If this is not an app host install and the binaries are not already
      // present, the installation will fail later due to a lack of products to
      // install.
      if (app_host && !chrome && !chrome_frame && !cf_state && !chrome_state) {
        DCHECK(!system_level);
        // App Host may use Chrome/Chrome binaries at system-level.
        if (original_state.GetProductState(
                true,  // system
                BrowserDistribution::CHROME_BROWSER) ||
            original_state.GetProductState(
                true,  // system
                BrowserDistribution::CHROME_BINARIES)) {
          VLOG(1) << "Installing/updating Application Host without binaries.";
        } else {
          // Somehow the binaries were present when the quick-enable app host
          // command was run, but now they appear to be missing.
          // Force binaries to be installed/updated.
          scoped_ptr<Product> binaries_to_add(new Product(
              BrowserDistribution::GetSpecificDistribution(
                  BrowserDistribution::CHROME_BINARIES)));
          binaries_to_add->SetOption(installer::kOptionMultiInstall, true);
          binaries = installer_state->AddProduct(&binaries_to_add);
          VLOG(1) << "Adding binaries for pre-existing App Host installation.";
        }
      }

      return true;
    }

    if (chrome) {
      if (chrome_frame &&
          chrome_frame->HasOption(installer::kOptionReadyMode)) {
        // We're being asked to install Chrome with Chrome Frame in ready-mode.
        // This is an optimistic operation: if a SxS install of Chrome Frame
        // is already present, don't touch it; if a multi-install of Chrome
        // Frame is present, preserve its settings (ready-mode).
        if (cf_state) {
          installer_state->RemoveProduct(chrome_frame);
          chrome_frame = NULL;
          if (cf_state->is_multi_install()) {
            chrome_frame = installer_state->AddProductFromState(
                BrowserDistribution::CHROME_FRAME, *cf_state);
            VLOG(1) << "Upgrading existing multi-install Chrome Frame rather "
                       "than installing in ready-mode.";
          } else {
            VLOG(1) << "Skipping upgrade of single-install Chrome Frame rather "
                       "than installing in ready-mode.";
          }
        } else {
          VLOG(1) << "Performing initial install of Chrome Frame ready-mode.";
        }
      }
    } else if (chrome_state) {
      // A product other than Chrome is being installed in multi-install mode,
      // and Chrome is already present. Add Chrome to the set of products
      // (making it multi-install in the process) so that it is updated, too.
      scoped_ptr<Product> multi_chrome(new Product(
          BrowserDistribution::GetSpecificDistribution(
              BrowserDistribution::CHROME_BROWSER)));
      multi_chrome->SetOption(installer::kOptionMultiInstall, true);
      chrome = installer_state->AddProduct(&multi_chrome);
      VLOG(1) << "Upgrading existing Chrome browser in multi-install mode.";
    } else if (chrome_frame &&
               chrome_frame->HasOption(installer::kOptionReadyMode)) {
      // Chrome Frame with ready-mode is to be installed, yet Chrome is
      // neither installed nor being installed.  Fail.
      LOG(ERROR) << "Cannot install Chrome Frame in ready mode without Chrome.";
      *status = installer::READY_MODE_REQUIRES_CHROME;
      installer_state->WriteInstallerResult(
          *status, IDS_INSTALL_READY_MODE_REQUIRES_CHROME_BASE, NULL);
      return false;
    }

    // Fail if we're installing Chrome Frame when a single-install of it is
    // already installed.
    if (chrome_frame && cf_state && !cf_state->is_multi_install()) {
      LOG(ERROR) << "Cannot migrate existing Chrome Frame installation to "
                 << "multi-install.";
      *status = installer::NON_MULTI_INSTALLATION_EXISTS;
      installer_state->WriteInstallerResult(*status,
          IDS_INSTALL_NON_MULTI_INSTALLATION_EXISTS_BASE, NULL);
      return false;
    }
  } else {
    // This is a non-multi installation.

    // Check for an existing installation of the product.
    const ProductState* product_state = original_state.GetProductState(
        system_level, products[0]->distribution()->GetType());
    if (product_state != NULL) {
      // Block downgrades from multi-install to single-install.
      if (product_state->is_multi_install()) {
        LOG(ERROR) << "Multi-install "
                   << products[0]->distribution()->GetAppShortCutName()
                   << " exists; aborting single install.";
        *status = installer::MULTI_INSTALLATION_EXISTS;
        installer_state->WriteInstallerResult(*status,
            IDS_INSTALL_MULTI_INSTALLATION_EXISTS_BASE, NULL);
        return false;
      }
    }
  }

  return true;
}

// Checks app host pre-install conditions, specifically that this is a
// user-level multi-install.  When the pre-install conditions are not
// satisfied, the result is written to the registry (via WriteInstallerResult),
// |status| is set appropriately, and false is returned.
bool CheckAppHostPreconditions(const InstallationState& original_state,
                               InstallerState* installer_state,
                               installer::InstallStatus* status) {
  if (installer_state->FindProduct(BrowserDistribution::CHROME_APP_HOST)) {

    if (!installer_state->is_multi_install()) {
      LOG(DFATAL) << "Application Host requires multi install";
      *status = installer::APP_HOST_REQUIRES_MULTI_INSTALL;
      // No message string since there is nothing a user can do.
      installer_state->WriteInstallerResult(*status, 0, NULL);
      return false;
    }

    if (installer_state->system_install()) {
      LOG(DFATAL) << "Application Host may only be installed at user-level.";
      *status = installer::APP_HOST_REQUIRES_USER_LEVEL;
      // No message string since there is nothing a user can do.
      installer_state->WriteInstallerResult(*status, 0, NULL);
      return false;
    }

  }

  return true;
}

// Checks for compatibility between the current state of the system and the
// desired operation.  Also applies policy that mutates the desired operation;
// specifically, the |installer_state| object.
// Also blocks simultaneous user-level and system-level installs.  In the case
// of trying to install user-level Chrome when system-level exists, the
// existing system-level Chrome is launched.
// When the pre-install conditions are not satisfied, the result is written to
// the registry (via WriteInstallerResult), |status| is set appropriately, and
// false is returned.
bool CheckPreInstallConditions(const InstallationState& original_state,
                               InstallerState* installer_state,
                               installer::InstallStatus* status) {
  if (!CheckAppHostPreconditions(original_state, installer_state, status)) {
    DCHECK_NE(*status, installer::UNKNOWN_STATUS);
    return false;
  }

  // See what products are already installed in multi mode.  When we do multi
  // installs, we must upgrade all installations since they share the binaries.
  AddExistingMultiInstalls(original_state, installer_state);

  if (!CheckMultiInstallConditions(original_state, installer_state, status)) {
    DCHECK_NE(*status, installer::UNKNOWN_STATUS);
    return false;
  }

  const Products& products = installer_state->products();
  if (products.empty()) {
    // We haven't been given any products on which to operate.
    LOG(ERROR)
        << "Not given any products to install and no products found to update.";
    *status = installer::CHROME_NOT_INSTALLED;
    installer_state->WriteInstallerResult(*status,
        IDS_INSTALL_NO_PRODUCTS_TO_UPDATE_BASE, NULL);
    return false;
  }

  if (!installer_state->system_install()) {
    // This is a user-level installation. Make sure that we are not installing
    // on top of an existing system-level installation.
    for (Products::const_iterator it = products.begin(); it < products.end();
         ++it) {
      const Product& product = **it;
      BrowserDistribution* browser_dist = product.distribution();

      // Skip over the binaries, as it's okay for them to be at both levels
      // for different products.
      if (browser_dist->GetType() == BrowserDistribution::CHROME_BINARIES)
        continue;

      const ProductState* user_level_product_state =
          original_state.GetProductState(false, browser_dist->GetType());
      const ProductState* system_level_product_state =
          original_state.GetProductState(true, browser_dist->GetType());

      // Allow upgrades to proceed so that out-of-date versions are not left
      // around.
      if (user_level_product_state)
        continue;

      // This is a new user-level install...

      if (system_level_product_state) {
        // ... and the product already exists at system-level.
        LOG(ERROR) << "Already installed version "
                   << system_level_product_state->version().GetString()
                   << " at system-level conflicts with this one at user-level.";
        if (product.is_chrome()) {
          // Instruct Google Update to launch the existing system-level Chrome.
          // There should be no error dialog.
          FilePath install_path(installer::GetChromeInstallPath(true,  // system
                                                                browser_dist));
          if (install_path.empty()) {
            // Give up if we failed to construct the install path.
            *status = installer::OS_ERROR;
            installer_state->WriteInstallerResult(*status,
                                                  IDS_INSTALL_OS_ERROR_BASE,
                                                  NULL);
          } else {
            *status = installer::EXISTING_VERSION_LAUNCHED;
            FilePath chrome_exe = install_path.Append(installer::kChromeExe);
            CommandLine cmd(chrome_exe);
            cmd.AppendSwitch(switches::kForceFirstRun);
            installer_state->WriteInstallerResult(*status, 0, NULL);
            VLOG(1) << "Launching existing system-level chrome instead.";
            base::LaunchProcess(cmd, base::LaunchOptions(), NULL);
          }
        } else {
          // Display an error message for other products.
          *status = installer::SYSTEM_LEVEL_INSTALL_EXISTS;
          installer_state->WriteInstallerResult(
              *status, IDS_INSTALL_SYSTEM_LEVEL_EXISTS_BASE, NULL);
        }
        return false;
      }
    }

  } else {  // System-level install.
    // --ensure-google-update-present is supported for user-level only.
    // The flag is generic, but its primary use case involves App Host.
    if (installer_state->ensure_google_update_present()) {
      LOG(DFATAL) << "--" << installer::switches::kEnsureGoogleUpdatePresent
                  << " is supported for user-level only.";
      *status = installer::APP_HOST_REQUIRES_USER_LEVEL;
      // No message string since there is nothing a user can do.
      installer_state->WriteInstallerResult(*status, 0, NULL);
      return false;
    }
  }

  return true;
}

installer::InstallStatus InstallProductsHelper(
    const InstallationState& original_state,
    const CommandLine& cmd_line,
    const MasterPreferences& prefs,
    const InstallerState& installer_state,
    installer::ArchiveType* archive_type,
    bool* delegated_to_existing) {
  DCHECK(archive_type);
  const bool system_install = installer_state.system_install();
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;

  // For install the default location for chrome.packed.7z is in current
  // folder, so get that value first.
  FilePath archive(cmd_line.GetProgram().DirName().Append(
      installer::kChromeCompressedArchive));

  // If --install-archive is given, get the user specified value
  if (cmd_line.HasSwitch(installer::switches::kInstallArchive)) {
    archive = cmd_line.GetSwitchValuePath(
        installer::switches::kInstallArchive);
  }

  const Products& products = installer_state.products();

  // Create a temp folder where we will unpack Chrome archive. If it fails,
  // then we are doomed, so return immediately and no cleanup is required.
  installer::SelfCleaningTempDir temp_path;
  if (!temp_path.Initialize(installer_state.target_path().DirName(),
                            installer::kInstallTempDir)) {
    PLOG(ERROR) << "Could not create temporary path.";
    installer_state.WriteInstallerResult(installer::TEMP_DIR_FAILED,
        IDS_INSTALL_TEMP_DIR_FAILED_BASE, NULL);
    return installer::TEMP_DIR_FAILED;
  }
  VLOG(1) << "created path " << temp_path.path().value();

  FilePath unpack_path;
  if (!file_util::CreateTemporaryDirInDir(temp_path.path(),
                                          installer::kInstallSourceDir,
                                          &unpack_path)) {
    PLOG(ERROR) << "Could not create temporary path for unpacked archive.";
    installer_state.WriteInstallerResult(installer::TEMP_DIR_FAILED,
        IDS_INSTALL_TEMP_DIR_FAILED_BASE, NULL);
    return installer::TEMP_DIR_FAILED;
  }

  bool unpacked = false;

  // We want to keep uncompressed archive (chrome.7z) that we get after
  // uncompressing and binary patching. Get the location for this file.
  FilePath archive_to_copy;
  if (file_util::PathExists(archive)) {
    VLOG(1) << "Archive found to install Chrome " << archive.value();
    if (UnPackArchive(archive, installer_state, temp_path.path(), unpack_path,
                      archive_type)) {
      install_status = (*archive_type) == installer::INCREMENTAL_ARCHIVE_TYPE ?
          installer::APPLY_DIFF_PATCH_FAILED : installer::UNCOMPRESSION_FAILED;
      installer_state.WriteInstallerResult(
          install_status,
          IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
          NULL);
    } else {
      unpacked = true;
      archive_to_copy = temp_path.path().Append(installer::kChromeArchive);
    }
  } else {
    FilePath uncompressed_archive(cmd_line.GetProgram().DirName().Append(
        installer::kChromeArchive));

    if (file_util::PathExists(uncompressed_archive)) {
      VLOG(1) << "Uncompressed archive found to install Chrome "
              << uncompressed_archive.value();
      *archive_type = installer::FULL_ARCHIVE_TYPE;
      string16 unpacked_file;
      if (LzmaUtil::UnPackArchive(uncompressed_archive.value(),
                                  unpack_path.value(), &unpacked_file)) {
        installer_state.WriteInstallerResult(
            installer::UNCOMPRESSION_FAILED,
            IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
            NULL);
      } else {
        unpacked = true;
        archive_to_copy = uncompressed_archive;
      }
    }
  }
  if (unpacked) {
    VLOG(1) << "unpacked to " << unpack_path.value();
    FilePath src_path(unpack_path.Append(installer::kInstallSourceChromeDir));
    scoped_ptr<Version>
        installer_version(installer::GetMaxVersionFromArchiveDir(src_path));
    if (!installer_version.get()) {
      LOG(ERROR) << "Did not find any valid version in installer.";
      install_status = installer::INVALID_ARCHIVE;
      installer_state.WriteInstallerResult(install_status,
          IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
    } else {
      VLOG(1) << "version to install: " << installer_version->GetString();
      bool proceed_with_installation = true;

      if (installer_state.operation() == InstallerState::MULTI_INSTALL) {
        // This is a new install of a multi-install product. Rather than give up
        // in case a higher version of the binaries (including a single-install
        // of Chrome, which can safely be migrated to multi-install by way of
        // CheckMultiInstallConditions) is already installed, delegate to the
        // installed setup.exe to install the product at hand.
        FilePath setup_exe;
        if (GetExistingHigherInstaller(original_state, system_install,
                                       *installer_version, &setup_exe)) {
          VLOG(1) << "Deferring to existing installer.";
          installer_state.UpdateStage(installer::DEFERRING_TO_HIGHER_VERSION);
          if (DeferToExistingInstall(setup_exe, cmd_line, installer_state,
                                     temp_path.path(), &install_status)) {
            *delegated_to_existing = true;
            return install_status;
          }
        }
      }

      uint32 higher_products = 0;
      COMPILE_ASSERT(
          sizeof(higher_products) * 8 > BrowserDistribution::NUM_TYPES,
          too_many_distribution_types_);
      const Products& products = installer_state.products();
      for (Products::const_iterator it = products.begin(); it < products.end();
           ++it) {
        const Product& product = **it;
        const ProductState* product_state =
            original_state.GetProductState(system_install,
                                           product.distribution()->GetType());
        if (product_state != NULL &&
            (product_state->version().CompareTo(*installer_version) > 0)) {
          LOG(ERROR) << "Higher version of "
                     << product.distribution()->GetAppShortCutName()
                     << " is already installed.";
          higher_products |= (1 << product.distribution()->GetType());
        }
      }

      if (higher_products != 0) {
        COMPILE_ASSERT(BrowserDistribution::NUM_TYPES == 4,
                       add_support_for_new_products_here_);
        const uint32 kBrowserBit = 1 << BrowserDistribution::CHROME_BROWSER;
        const uint32 kGCFBit = 1 << BrowserDistribution::CHROME_FRAME;
        const uint32 kAppHostBit = 1 << BrowserDistribution::CHROME_APP_HOST;
        int message_id = 0;

        proceed_with_installation = false;
        install_status = installer::HIGHER_VERSION_EXISTS;
        switch (higher_products) {
          case kBrowserBit:
            message_id = IDS_INSTALL_HIGHER_VERSION_BASE;
            break;
          case kGCFBit:
            message_id = IDS_INSTALL_HIGHER_VERSION_CF_BASE;
            break;
          case kGCFBit | kBrowserBit:
            message_id = IDS_INSTALL_HIGHER_VERSION_CB_CF_BASE;
            break;
          default:
            message_id = IDS_INSTALL_HIGHER_VERSION_APP_HOST_BASE;
            break;
        }

        installer_state.WriteInstallerResult(install_status, message_id, NULL);
      }

      proceed_with_installation =
          proceed_with_installation &&
          CheckGroupPolicySettings(original_state, installer_state,
                                   *installer_version, &install_status);

      if (proceed_with_installation) {
        // If Google Update is absent at user-level, install it using the
        // Google Update installer from an existing system-level installation.
        // This is for quick-enable App Host install from a system-level
        // Chrome Binaries installation.
        if (!system_install && installer_state.ensure_google_update_present()) {
          if (!google_update::EnsureUserLevelGoogleUpdatePresent()) {
            LOG(ERROR) << "Failed to install Google Update";
            proceed_with_installation = false;
            install_status = installer::INSTALL_OF_GOOGLE_UPDATE_FAILED;
            installer_state.WriteInstallerResult(install_status, 0, NULL);
          }
        }
      }

      if (proceed_with_installation) {
        FilePath prefs_source_path(cmd_line.GetSwitchValueNative(
            installer::switches::kInstallerData));
        install_status = installer::InstallOrUpdateProduct(
            original_state, installer_state, cmd_line.GetProgram(),
            archive_to_copy, temp_path.path(), src_path, prefs_source_path,
            prefs, *installer_version);

        int install_msg_base = IDS_INSTALL_FAILED_BASE;
        string16 chrome_exe;
        string16 quoted_chrome_exe;
        if (install_status == installer::SAME_VERSION_REPAIR_FAILED) {
          if (installer_state.FindProduct(BrowserDistribution::CHROME_FRAME)) {
            install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_CF_BASE;
          } else {
            install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_BASE;
          }
        } else if (install_status != installer::INSTALL_FAILED) {
          if (installer_state.target_path().empty()) {
            // If we failed to construct install path, it means the OS call to
            // get %ProgramFiles% or %AppData% failed. Report this as failure.
            install_msg_base = IDS_INSTALL_OS_ERROR_BASE;
            install_status = installer::OS_ERROR;
          } else {
            chrome_exe = installer_state.target_path()
                .Append(installer::kChromeExe).value();
            quoted_chrome_exe = L"\"" + chrome_exe + L"\"";
            install_msg_base = 0;
          }
        }

        installer_state.UpdateStage(installer::FINISHING);

        // Only do Chrome-specific stuff (like launching the browser) if
        // Chrome was specifically requested (rather than being upgraded as
        // part of a multi-install).
        const Product* chrome_install = prefs.install_chrome() ?
            installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER) :
            NULL;

        bool do_not_register_for_update_launch = false;
        if (chrome_install) {
          prefs.GetBool(
              installer::master_preferences::kDoNotRegisterForUpdateLaunch,
              &do_not_register_for_update_launch);
        } else {
          do_not_register_for_update_launch = true;  // Never register.
        }

        bool write_chrome_launch_string =
            (!do_not_register_for_update_launch &&
             install_status != installer::IN_USE_UPDATED);

        installer_state.WriteInstallerResult(install_status, install_msg_base,
            write_chrome_launch_string ? &quoted_chrome_exe : NULL);

        if (install_status == installer::FIRST_INSTALL_SUCCESS) {
          VLOG(1) << "First install successful.";
          if (chrome_install) {
            // We never want to launch Chrome in system level install mode.
            bool do_not_launch_chrome = false;
            prefs.GetBool(
                installer::master_preferences::kDoNotLaunchChrome,
                &do_not_launch_chrome);
            if (!system_install && !do_not_launch_chrome)
              chrome_install->LaunchChrome(installer_state.target_path());
          }
        } else if ((install_status == installer::NEW_VERSION_UPDATED) ||
                   (install_status == installer::IN_USE_UPDATED)) {
          const Product* chrome = installer_state.FindProduct(
              BrowserDistribution::CHROME_BROWSER);
          if (chrome != NULL) {
            DCHECK_NE(chrome_exe, string16());
            installer::RemoveChromeLegacyRegistryKeys(chrome->distribution(),
                                                      chrome_exe);
          }
        }
      }
    }

    // There might be an experiment (for upgrade usually) that needs to happen.
    // An experiment's outcome can include chrome's uninstallation. If that is
    // the case we would not do that directly at this point but in another
    // instance of setup.exe
    //
    // There is another way to reach this same function if this is a system
    // level install. See HandleNonInstallCmdLineOptions().
    {
      // If installation failed, use the path to the currently running setup.
      // If installation succeeded, use the path to setup in the installer dir.
      FilePath setup_path(cmd_line.GetProgram());
      if (InstallUtil::GetInstallReturnCode(install_status) == 0) {
        setup_path = installer_state.GetInstallerDirectory(*installer_version)
            .Append(setup_path.BaseName());
      }
      for (Products::const_iterator it = products.begin(); it < products.end();
           ++it) {
        const Product& product = **it;
        product.distribution()->LaunchUserExperiment(setup_path,
            install_status, *installer_version, product, system_install);
      }
    }
  }

  // Delete the master profile file if present. Note that we do not care about
  // rollback here and we schedule for deletion on reboot if the delete fails.
  // As such, we do not use DeleteTreeWorkItem.
  if (cmd_line.HasSwitch(installer::switches::kInstallerData)) {
    FilePath prefs_path(cmd_line.GetSwitchValuePath(
        installer::switches::kInstallerData));
    if (!file_util::Delete(prefs_path, true)) {
      LOG(ERROR) << "Failed deleting master preferences file "
                 << prefs_path.value()
                 << ", scheduling for deletion after reboot.";
      ScheduleFileSystemEntityForDeletion(prefs_path.value().c_str());
    }
  }

  // temp_path's dtor will take care of deleting or scheduling itself for
  // deletion at reboot when this scope closes.
  VLOG(1) << "Deleting temporary directory " << temp_path.path().value();

  return install_status;
}

installer::InstallStatus InstallProducts(
    const InstallationState& original_state,
    const CommandLine& cmd_line,
    const MasterPreferences& prefs,
    InstallerState* installer_state) {
  DCHECK(installer_state);
  const bool system_install = installer_state->system_install();
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  installer::ArchiveType archive_type = installer::UNKNOWN_ARCHIVE_TYPE;
  bool incremental_install = false;
  installer_state->UpdateStage(installer::PRECONDITIONS);
  // The stage provides more fine-grained information than -multifail, so remove
  // the -multifail suffix from the Google Update "ap" value.
  BrowserDistribution::GetSpecificDistribution(installer_state->state_type())->
      UpdateInstallStatus(system_install, archive_type, install_status);
  if (CheckPreInstallConditions(original_state, installer_state,
                                &install_status)) {
    VLOG(1) << "Installing to " << installer_state->target_path().value();
    bool delegated_to_existing = false;
    install_status = InstallProductsHelper(
        original_state, cmd_line, prefs, *installer_state, &archive_type,
        &delegated_to_existing);
    // Early exit if this setup.exe delegated to another, since that one would
    // have taken care of UpdateInstallStatus and UpdateStage.
    if (delegated_to_existing)
      return install_status;
  } else {
    // CheckPreInstallConditions must set the status on failure.
    DCHECK_NE(install_status, installer::UNKNOWN_STATUS);
  }

  const Products& products = installer_state->products();

  for (Products::const_iterator it = products.begin(); it < products.end();
       ++it) {
    (*it)->distribution()->UpdateInstallStatus(
        system_install, archive_type, install_status);
  }

  installer_state->UpdateStage(installer::NO_STAGE);
  return install_status;
}

installer::InstallStatus UninstallProduct(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const CommandLine& cmd_line,
    bool remove_all,
    bool force_uninstall,
    const Product& product) {
  const ProductState* product_state =
      original_state.GetProductState(installer_state.system_install(),
                                     product.distribution()->GetType());
  if (product_state != NULL) {
    VLOG(1) << "version on the system: "
            << product_state->version().GetString();
  } else if (!force_uninstall) {
    LOG(ERROR) << product.distribution()->GetAppShortCutName()
               << " not found for uninstall.";
    return installer::CHROME_NOT_INSTALLED;
  }

  return installer::UninstallProduct(
      original_state, installer_state, cmd_line.GetProgram(), product,
      remove_all, force_uninstall, cmd_line);
}

installer::InstallStatus UninstallProducts(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const CommandLine& cmd_line) {
  const Products& products = installer_state.products();

  // Decide whether Active Setup should be triggered and/or system-level Chrome
  // should be launched post-uninstall. This needs to be done outside the
  // UninstallProduct calls as some of them might terminate the processes
  // launched by a previous one otherwise...
  bool trigger_active_setup = false;
  // System-level Chrome will be launched via this command if its program gets
  // set below.
  CommandLine system_level_cmd(CommandLine::NO_PROGRAM);

  const Product* chrome =
      installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER);
  if (chrome) {
    // InstallerState::Initialize always puts Chrome first, and we rely on that
    // here for this reason: if Chrome is in-use, the user will be prompted to
    // confirm uninstallation.  Upon cancel, we should not continue with the
    // other products.
    DCHECK(products[0]->is_chrome());

    if (cmd_line.HasSwitch(installer::switches::kSelfDestruct) &&
        !installer_state.system_install()) {
      BrowserDistribution* dist = chrome->distribution();
      const FilePath system_exe_path(
          installer::GetChromeInstallPath(true, dist)
              .Append(installer::kChromeExe));
      system_level_cmd.SetProgram(system_exe_path);

      FilePath first_run_sentinel;
      InstallUtil::GetSentinelFilePath(
          chrome::kFirstRunSentinel, dist, &first_run_sentinel);
      if (file_util::PathExists(first_run_sentinel)) {
        // If the Chrome being self-destructed has already undergone First Run,
        // trigger Active Setup and make sure the system-level Chrome doesn't go
        // through first run.
        trigger_active_setup = true;
        system_level_cmd.AppendSwitch(::switches::kNoFirstRun);
      }
    }
  }
  if (installer_state.FindProduct(BrowserDistribution::CHROME_BINARIES)) {
    // Chrome Binaries should be last; if something else is cancelled, they
    // should stay.
    DCHECK(products[products.size() - 1]->is_chrome_binaries());
  }

  installer::InstallStatus install_status = installer::UNINSTALL_SUCCESSFUL;
  installer::InstallStatus prod_status = installer::UNKNOWN_STATUS;
  const bool force = cmd_line.HasSwitch(installer::switches::kForceUninstall);
  const bool remove_all = !cmd_line.HasSwitch(
      installer::switches::kDoNotRemoveSharedItems);

  for (Products::const_iterator it = products.begin();
       install_status != installer::UNINSTALL_CANCELLED && it < products.end();
       ++it) {
    prod_status = UninstallProduct(original_state, installer_state,
        cmd_line, remove_all, force, **it);
    if (prod_status != installer::UNINSTALL_SUCCESSFUL)
      install_status = prod_status;
  }

  installer::CleanUpInstallationDirectoryAfterUninstall(
      original_state, installer_state, cmd_line, &install_status);

  if (trigger_active_setup)
    InstallUtil::TriggerActiveSetupCommand();

  if (!system_level_cmd.GetProgram().empty())
    base::LaunchProcess(system_level_cmd, base::LaunchOptions(), NULL);

  // Tell Google Update that an uninstall has taken place.
  // Ignore the return value: success or failure of Google Update
  // has no bearing on the success or failure of Chrome's uninstallation.
  google_update::UninstallGoogleUpdate(installer_state.system_install());

  return install_status;
}

installer::InstallStatus ShowEULADialog(const string16& inner_frame) {
  VLOG(1) << "About to show EULA";
  string16 eula_path = installer::GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return installer::EULA_REJECTED;
  }
  // Newer versions of the caller pass an inner frame parameter that must
  // be given to the html page being launched.
  installer::EulaHTMLDialog dlg(eula_path, inner_frame);
  installer::EulaHTMLDialog::Outcome outcome = dlg.ShowModal();
  if (installer::EulaHTMLDialog::REJECTED == outcome) {
    LOG(ERROR) << "EULA rejected or EULA failure";
    return installer::EULA_REJECTED;
  }
  if (installer::EulaHTMLDialog::ACCEPTED_OPT_IN == outcome) {
    VLOG(1) << "EULA accepted (opt-in)";
    return installer::EULA_ACCEPTED_OPT_IN;
  }
  VLOG(1) << "EULA accepted (no opt-in)";
  return installer::EULA_ACCEPTED;
}

// Creates the sentinel indicating that the EULA was required and has been
// accepted.
bool CreateEULASentinel(BrowserDistribution* dist) {
  FilePath eula_sentinel;
  if (!InstallUtil::GetSentinelFilePath(installer::kEULASentinelFile,
                                        dist, &eula_sentinel)) {
    return false;
  }

  return (file_util::CreateDirectory(eula_sentinel.DirName()) &&
          file_util::WriteFile(eula_sentinel, "", 0) != -1);
}

void ActivateMetroChrome() {
  // Check to see if we're per-user or not. Need to do this since we may
  // not have been invoked with --system-level even for a machine install.
  wchar_t exe_path[MAX_PATH * 2] = {};
  GetModuleFileName(NULL, exe_path, arraysize(exe_path));
  bool is_per_user_install = InstallUtil::IsPerUserInstall(exe_path);

  string16 app_model_id =
      ShellUtil::GetBrowserModelId(BrowserDistribution::GetDistribution(),
                                   is_per_user_install);

  base::win::ScopedComPtr<IApplicationActivationManager> activator;
  HRESULT hr = activator.CreateInstance(CLSID_ApplicationActivationManager);
  if (SUCCEEDED(hr)) {
    DWORD pid = 0;
    hr = activator->ActivateApplication(
        app_model_id.c_str(), L"open", AO_NONE, &pid);
  }

  LOG_IF(ERROR, FAILED(hr)) << "Tried and failed to launch Metro Chrome. "
                            << "hr=" << std::hex << hr;
}

installer::InstallStatus RegisterDevChrome(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const CommandLine& cmd_line) {
  BrowserDistribution* chrome_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER);

  // Only proceed with registering a dev chrome if no real Chrome installation
  // of the same distribution are present on this system.
  const ProductState* existing_chrome =
    original_state.GetProductState(false, BrowserDistribution::CHROME_BROWSER);
  if (!existing_chrome) {
    existing_chrome =
      original_state.GetProductState(true, BrowserDistribution::CHROME_BROWSER);
  }
  if (existing_chrome) {
    static const wchar_t kPleaseUninstallYourChromeMessage[] =
        L"You already have a full-installation (non-dev) of %1ls, please "
        L"uninstall it first using Add/Remove Programs in the control panel.";
    string16 name(chrome_dist->GetAppShortCutName());
    string16 message(base::StringPrintf(kPleaseUninstallYourChromeMessage,
                                        name.c_str()));

    LOG(ERROR) << "Aborting operation: another installation of " << name
               << " was found, as a last resort (if the product is not present "
                  "in Add/Remove Programs), try executing: "
               << existing_chrome->uninstall_command().GetCommandLineString();
    MessageBox(NULL, message.c_str(), NULL, MB_ICONERROR);
    return installer::INSTALL_FAILED;
  }

  FilePath chrome_exe(
      cmd_line.GetSwitchValuePath(installer::switches::kRegisterDevChrome));
  if (chrome_exe.empty())
    chrome_exe = cmd_line.GetProgram().DirName().Append(installer::kChromeExe);
  if (!chrome_exe.IsAbsolute())
    file_util::AbsolutePath(&chrome_exe);

  installer::InstallStatus status = installer::FIRST_INSTALL_SUCCESS;
  if (file_util::PathExists(chrome_exe)) {
    Product chrome(chrome_dist);

    // Create the Start menu shortcut and pin it to the taskbar.
    ShellUtil::ShortcutProperties shortcut_properties(ShellUtil::CURRENT_USER);
    chrome.AddDefaultShortcutProperties(chrome_exe, &shortcut_properties);
    shortcut_properties.set_dual_mode(true);
    shortcut_properties.set_pin_to_taskbar(true);
    ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_START_MENU, chrome_dist,
        shortcut_properties, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS);

    // Register Chrome at user-level and make it default.
    scoped_ptr<WorkItemList> delegate_execute_list(
        WorkItem::CreateWorkItemList());
    installer::AddDelegateExecuteWorkItems(
        installer_state, chrome_exe.DirName(), Version(), chrome,
        delegate_execute_list.get());
    delegate_execute_list->Do();
    if (ShellUtil::CanMakeChromeDefaultUnattended()) {
      ShellUtil::MakeChromeDefault(
          chrome_dist, ShellUtil::CURRENT_USER, chrome_exe.value(), true);
    } else {
      ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_dist, chrome_exe.value());
    }
  } else {
    LOG(ERROR) << "Path not found: " << chrome_exe.value();
    status = installer::INSTALL_FAILED;
  }
  return status;
}

// This method processes any command line options that make setup.exe do
// various tasks other than installation (renaming chrome.exe, showing eula
// among others). This function returns true if any such command line option
// has been found and processed (so setup.exe should exit at that point).
bool HandleNonInstallCmdLineOptions(const InstallationState& original_state,
                                    const CommandLine& cmd_line,
                                    InstallerState* installer_state,
                                    int* exit_code) {
  // TODO(gab): Add a local |status| variable which each block below sets;
  // only determine the |exit_code| from |status| at the end (this will allow
  // this method to validate that
  // (!handled || status != installer::UNKNOWN_STATUS)).
  bool handled = true;
  // TODO(tommi): Split these checks up into functions and use a data driven
  // map of switch->function.
  if (cmd_line.HasSwitch(installer::switches::kUpdateSetupExe)) {
    installer::InstallStatus status = installer::SETUP_PATCH_FAILED;
    // If --update-setup-exe command line option is given, we apply the given
    // patch to current exe, and store the resulting binary in the path
    // specified by --new-setup-exe. But we need to first unpack the file
    // given in --update-setup-exe.
    base::ScopedTempDir temp_path;
    if (!temp_path.CreateUniqueTempDir()) {
      PLOG(ERROR) << "Could not create temporary path.";
    } else {
      string16 setup_patch = cmd_line.GetSwitchValueNative(
          installer::switches::kUpdateSetupExe);
      VLOG(1) << "Opening archive " << setup_patch;
      string16 uncompressed_patch;
      if (LzmaUtil::UnPackArchive(setup_patch, temp_path.path().value(),
                                  &uncompressed_patch) == NO_ERROR) {
        FilePath old_setup_exe = cmd_line.GetProgram();
        FilePath new_setup_exe = cmd_line.GetSwitchValuePath(
            installer::switches::kNewSetupExe);
        if (!installer::ApplyDiffPatch(old_setup_exe,
                                       FilePath(uncompressed_patch),
                                       new_setup_exe,
                                       installer_state))
          status = installer::NEW_VERSION_UPDATED;
      }
      if (!temp_path.Delete()) {
        // PLOG would be nice, but Delete() doesn't leave a meaningful value in
        // the Windows last-error code.
        LOG(WARNING) << "Scheduling temporary path " << temp_path.path().value()
                     << " for deletion at reboot.";
        ScheduleDirectoryForDeletion(temp_path.path().value().c_str());
      }
    }

    *exit_code = InstallUtil::GetInstallReturnCode(status);
    if (*exit_code) {
      LOG(WARNING) << "setup.exe patching failed.";
      installer_state->WriteInstallerResult(
          status, IDS_SETUP_PATCH_FAILED_BASE, NULL);
    }
    // We will be exiting normally, so clear the stage indicator.
    installer_state->UpdateStage(installer::NO_STAGE);
  } else if (cmd_line.HasSwitch(installer::switches::kShowEula)) {
    // Check if we need to show the EULA. If it is passed as a command line
    // then the dialog is shown and regardless of the outcome setup exits here.
    string16 inner_frame =
        cmd_line.GetSwitchValueNative(installer::switches::kShowEula);
    *exit_code = ShowEULADialog(inner_frame);

    if (installer::EULA_REJECTED != *exit_code) {
      if (GoogleUpdateSettings::SetEULAConsent(
              original_state, BrowserDistribution::GetDistribution(), true)) {
        CreateEULASentinel(BrowserDistribution::GetDistribution());
      }
      // For a metro-originated launch, we now need to launch back into metro.
      if (cmd_line.HasSwitch(installer::switches::kShowEulaForMetro))
        ActivateMetroChrome();
    }
  } else if (cmd_line.HasSwitch(installer::switches::kConfigureUserSettings)) {
    // NOTE: Should the work done here, on kConfigureUserSettings, change:
    // kActiveSetupVersion in install_worker.cc needs to be increased for Active
    // Setup to invoke this again for all users of this install.
    const Product* chrome_install =
        installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER);
    installer::InstallStatus status = installer::INVALID_STATE_FOR_OPTION;
    if (chrome_install && installer_state->system_install()) {
      bool force =
          cmd_line.HasSwitch(installer::switches::kForceConfigureUserSettings);
      installer::HandleActiveSetupForBrowser(installer_state->target_path(),
                                             *chrome_install, force);
      status = installer::INSTALL_REPAIRED;
    } else {
      LOG(DFATAL) << "chrome_install:" << chrome_install
                  << ", system_install:" << installer_state->system_install();
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRegisterDevChrome)) {
    installer::InstallStatus status = RegisterDevChrome(
        original_state, *installer_state, cmd_line);
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser)) {
    installer::InstallStatus status = installer::UNKNOWN_STATUS;
    const Product* chrome_install =
        installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER);
    if (chrome_install) {
      // If --register-chrome-browser option is specified, register all
      // Chrome protocol/file associations, as well as register it as a valid
      // browser for Start Menu->Internet shortcut. This switch will also
      // register Chrome as a valid handler for a set of URL protocols that
      // Chrome may become the default handler for, either by the user marking
      // Chrome as the default browser, through the Windows Default Programs
      // control panel settings, or through website use of
      // registerProtocolHandler. These protocols are found in
      // ShellUtil::kPotentialProtocolAssociations.
      // The --register-url-protocol will additionally register Chrome as a
      // potential handler for the supplied protocol, and is used if a website
      // registers a handler for a protocol not found in
      // ShellUtil::kPotentialProtocolAssociations.
      // These options should only be used when setup.exe is launched with admin
      // rights. We do not make any user specific changes with this option.
      DCHECK(IsUserAnAdmin());
      string16 chrome_exe(cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowser));
      string16 suffix;
      if (cmd_line.HasSwitch(
          installer::switches::kRegisterChromeBrowserSuffix)) {
        suffix = cmd_line.GetSwitchValueNative(
            installer::switches::kRegisterChromeBrowserSuffix);
      }
      if (cmd_line.HasSwitch(installer::switches::kRegisterURLProtocol)) {
        string16 protocol = cmd_line.GetSwitchValueNative(
            installer::switches::kRegisterURLProtocol);
        // ShellUtil::RegisterChromeForProtocol performs all registration
        // done by ShellUtil::RegisterChromeBrowser, as well as registering
        // with Windows as capable of handling the supplied protocol.
        if (ShellUtil::RegisterChromeForProtocol(
                chrome_install->distribution(), chrome_exe, suffix, protocol,
                false))
          status = installer::IN_USE_UPDATED;
      } else {
        if (ShellUtil::RegisterChromeBrowser(chrome_install->distribution(),
            chrome_exe, suffix, false))
          status = installer::IN_USE_UPDATED;
      }
    } else {
      LOG(DFATAL) << "Can't register browser - Chrome distribution not found";
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRenameChromeExe)) {
    // If --rename-chrome-exe is specified, we want to rename the executables
    // and exit.
    *exit_code = RenameChromeExecutables(original_state, installer_state);
  } else if (cmd_line.HasSwitch(
                 installer::switches::kRemoveChromeRegistration)) {
    // This is almost reverse of --register-chrome-browser option above.
    // Here we delete Chrome browser registration. This option should only
    // be used when setup.exe is launched with admin rights. We do not
    // make any user specific changes in this option.
    string16 suffix;
    if (cmd_line.HasSwitch(
            installer::switches::kRegisterChromeBrowserSuffix)) {
      suffix = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowserSuffix);
    }
    installer::InstallStatus tmp = installer::UNKNOWN_STATUS;
    const Product* chrome_install =
        installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER);
    DCHECK(chrome_install);
    if (chrome_install) {
      installer::DeleteChromeRegistrationKeys(*installer_state,
          chrome_install->distribution(), HKEY_LOCAL_MACHINE, suffix, &tmp);
    }
    *exit_code = tmp;
  } else if (cmd_line.HasSwitch(installer::switches::kOnOsUpgrade)) {
    const Product* chrome_install =
        installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER);
    installer::InstallStatus status = installer::INVALID_STATE_FOR_OPTION;
    if (chrome_install) {
      installer::HandleOsUpgradeForBrowser(*installer_state,
                                           *chrome_install);
      status = installer::INSTALL_REPAIRED;
    } else {
      LOG(DFATAL) << "Chrome product not found.";
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kInactiveUserToast)) {
    // Launch the inactive user toast experiment.
    int flavor = -1;
    base::StringToInt(cmd_line.GetSwitchValueNative(
        installer::switches::kInactiveUserToast), &flavor);
    std::string experiment_group =
        cmd_line.GetSwitchValueASCII(installer::switches::kExperimentGroup);
    DCHECK_NE(-1, flavor);
    if (flavor == -1) {
      *exit_code = installer::UNKNOWN_STATUS;
    } else {
      const Products& products = installer_state->products();
      for (Products::const_iterator it = products.begin(); it < products.end();
           ++it) {
        const Product& product = **it;
        BrowserDistribution* browser_dist = product.distribution();
        browser_dist->InactiveUserToastExperiment(
            flavor, ASCIIToUTF16(experiment_group), product,
            installer_state->target_path());
      }
    }
  } else if (cmd_line.HasSwitch(installer::switches::kSystemLevelToast)) {
    const Products& products = installer_state->products();
    for (Products::const_iterator it = products.begin(); it < products.end();
         ++it) {
      const Product& product = **it;
      BrowserDistribution* browser_dist = product.distribution();
      // We started as system-level and have been re-launched as user level
      // to continue with the toast experiment.
      Version installed_version;
      InstallUtil::GetChromeVersion(browser_dist, true, &installed_version);
      if (!installed_version.IsValid()) {
        LOG(ERROR) << "No installation of "
                   << browser_dist->GetAppShortCutName()
                   << " found for system-level toast.";
      } else {
        browser_dist->LaunchUserExperiment(cmd_line.GetProgram(),
                                           installer::REENTRY_SYS_UPDATE,
                                           installed_version, product, true);
      }
    }
  } else if (cmd_line.HasSwitch(
                 installer::switches::kChromeFrameReadyModeOptIn)) {
    *exit_code = InstallUtil::GetInstallReturnCode(
        installer::ChromeFrameReadyModeOptIn(original_state, *installer_state));
  } else if (cmd_line.HasSwitch(
                 installer::switches::kChromeFrameReadyModeTempOptOut)) {
    *exit_code = InstallUtil::GetInstallReturnCode(
        installer::ChromeFrameReadyModeTempOptOut(original_state,
                                                  *installer_state));
  } else if (cmd_line.HasSwitch(
                 installer::switches::kChromeFrameReadyModeEndTempOptOut)) {
    *exit_code = InstallUtil::GetInstallReturnCode(
        installer::ChromeFrameReadyModeEndTempOptOut(original_state,
                                                     *installer_state));
  } else if (cmd_line.HasSwitch(installer::switches::kChromeFrameQuickEnable)) {
    *exit_code = installer::ChromeFrameQuickEnable(original_state,
                                                   installer_state);
  } else {
    handled = false;
  }

  return handled;
}

bool ShowRebootDialog() {
  // Get a token for this process.
  HANDLE token;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                        &token)) {
    LOG(ERROR) << "Failed to open token.";
    return false;
  }

  // Use a ScopedHandle to keep track of and eventually close our handle.
  // TODO(robertshield): Add a Receive() method to base's ScopedHandle.
  base::win::ScopedHandle scoped_handle(token);

  // Get the LUID for the shutdown privilege.
  TOKEN_PRIVILEGES tkp = {0};
  LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
  tkp.PrivilegeCount = 1;
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  // Get the shutdown privilege for this process.
  AdjustTokenPrivileges(token, FALSE, &tkp, 0,
                        reinterpret_cast<PTOKEN_PRIVILEGES>(NULL), 0);
  if (GetLastError() != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to get shutdown privileges.";
    return false;
  }

  // Popup a dialog that will prompt to reboot using the default system message.
  // TODO(robertshield): Add a localized, more specific string to the prompt.
  RestartDialog(NULL, NULL, EWX_REBOOT | EWX_FORCEIFHUNG);
  return true;
}

// Returns the Custom information for the client identified by the exe path
// passed in. This information is used for crash reporting.
google_breakpad::CustomClientInfo* GetCustomInfo(const wchar_t* exe_path) {
  string16 product;
  string16 version;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(FilePath(exe_path)));
  if (version_info.get()) {
    version = version_info->product_version();
    product = version_info->product_short_name();
  }

  if (version.empty())
    version = L"0.1.0.0";

  if (product.empty())
    product = L"Chrome Installer";

  static google_breakpad::CustomInfoEntry ver_entry(L"ver", version.c_str());
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", product.c_str());
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype",
                                                     L"Chrome Installer");
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };
  static google_breakpad::CustomClientInfo custom_info = {
      entries, arraysize(entries) };
  return &custom_info;
}

// Initialize crash reporting for this process. This involves connecting to
// breakpad, etc.
google_breakpad::ExceptionHandler* InitializeCrashReporting(
    bool system_install) {
  // Only report crashes if the user allows it.
  if (!GoogleUpdateSettings::GetCollectStatsConsent())
    return NULL;

  // Get the alternate dump directory. We use the temp path.
  FilePath temp_directory;
  if (!file_util::GetTempDir(&temp_directory) || temp_directory.empty())
    return NULL;

  wchar_t exe_path[MAX_PATH * 2] = {0};
  GetModuleFileName(NULL, exe_path, arraysize(exe_path));

  // Build the pipe name. It can be either:
  // System-wide install: "NamedPipe\GoogleCrashServices\S-1-5-18"
  // Per-user install: "NamedPipe\GoogleCrashServices\<user SID>"
  string16 user_sid = kSystemPrincipalSid;

  if (!system_install) {
    if (!base::win::GetUserSidString(&user_sid)) {
      return NULL;
    }
  }

  string16 pipe_name = kGoogleUpdatePipeName;
  pipe_name += user_sid;

  google_breakpad::ExceptionHandler* breakpad =
      new google_breakpad::ExceptionHandler(
          temp_directory.value(), NULL, NULL, NULL,
          google_breakpad::ExceptionHandler::HANDLER_ALL, kLargerDumpType,
          pipe_name.c_str(), GetCustomInfo(exe_path));
  return breakpad;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* command_line, int show_command) {
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  CommandLine::Init(0, NULL);

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  installer::InitInstallerLogging(prefs);

  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  VLOG(1) << "Command Line: " << cmd_line.GetCommandLineString();

  VLOG(1) << "multi install is " << prefs.is_multi_install();
  bool system_install = false;
  prefs.GetBool(installer::master_preferences::kSystemLevel, &system_install);
  VLOG(1) << "system install is " << system_install;

  google_breakpad::scoped_ptr<google_breakpad::ExceptionHandler> breakpad(
      InitializeCrashReporting(system_install));

  InstallationState original_state;
  original_state.Initialize();

  InstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, original_state);
  const bool is_uninstall = cmd_line.HasSwitch(installer::switches::kUninstall);

  // Check to make sure current system is WinXP or later. If not, log
  // error message and get out.
  if (!InstallUtil::IsOSSupported()) {
    LOG(ERROR) << "Chrome only supports Windows XP or later.";
    installer_state.WriteInstallerResult(
        installer::OS_NOT_SUPPORTED, IDS_INSTALL_OS_NOT_SUPPORTED_BASE, NULL);
    return installer::OS_NOT_SUPPORTED;
  }

  // Initialize COM for use later.
  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.succeeded()) {
    installer_state.WriteInstallerResult(
        installer::OS_ERROR, IDS_INSTALL_OS_ERROR_BASE, NULL);
    return installer::OS_ERROR;
  }

  // Some command line options don't work with SxS install/uninstall
  if (InstallUtil::IsChromeSxSProcess()) {
    if (system_install ||
        prefs.is_multi_install() ||
        cmd_line.HasSwitch(installer::switches::kForceUninstall) ||
        cmd_line.HasSwitch(installer::switches::kMakeChromeDefault) ||
        cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser) ||
        cmd_line.HasSwitch(installer::switches::kRemoveChromeRegistration) ||
        cmd_line.HasSwitch(installer::switches::kInactiveUserToast) ||
        cmd_line.HasSwitch(installer::switches::kSystemLevelToast) ||
        cmd_line.HasSwitch(installer::switches::kChromeFrameQuickEnable)) {
      return installer::SXS_OPTION_NOT_SUPPORTED;
    }
  }

  int exit_code = 0;
  if (HandleNonInstallCmdLineOptions(
          original_state, cmd_line, &installer_state, &exit_code)) {
    return exit_code;
  }

  if (system_install && !IsUserAnAdmin()) {
    if (base::win::GetVersion() >= base::win::VERSION_VISTA &&
        !cmd_line.HasSwitch(installer::switches::kRunAsAdmin)) {
      CommandLine new_cmd(CommandLine::NO_PROGRAM);
      new_cmd.AppendArguments(cmd_line, true);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      new_cmd.AppendSwitch(installer::switches::kRunAsAdmin);
      // If system_install became true due to an environment variable, append
      // it to the command line here since env vars may not propagate past the
      // elevation.
      if (!new_cmd.HasSwitch(installer::switches::kSystemLevel))
        new_cmd.AppendSwitch(installer::switches::kSystemLevel);

      DWORD exit_code = installer::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(new_cmd, &exit_code);
      return exit_code;
    } else {
      LOG(ERROR) << "Non admin user can not install system level Chrome.";
      installer_state.WriteInstallerResult(installer::INSUFFICIENT_RIGHTS,
          IDS_INSTALL_INSUFFICIENT_RIGHTS_BASE, NULL);
      return installer::INSUFFICIENT_RIGHTS;
    }
  }

  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  // If --uninstall option is given, uninstall the identified product(s)
  if (is_uninstall) {
    install_status =
        UninstallProducts(original_state, installer_state, cmd_line);
  } else {
    // If --uninstall option is not specified, we assume it is install case.
    install_status =
        InstallProducts(original_state, cmd_line, prefs, &installer_state);
  }

  // Validate that the machine is now in a good state following the operation.
  // TODO(grt): change this to log at DFATAL once we're convinced that the
  // validator handles all cases properly.
  InstallationValidator::InstallationType installation_type =
      InstallationValidator::NO_PRODUCTS;
  LOG_IF(ERROR,
         !InstallationValidator::ValidateInstallationType(system_install,
                                                          &installation_type));

  const Product* cf_install =
      installer_state.FindProduct(BrowserDistribution::CHROME_FRAME);

  if (cf_install &&
      !cmd_line.HasSwitch(installer::switches::kForceUninstall)) {
    if (install_status == installer::UNINSTALL_REQUIRES_REBOOT) {
      ShowRebootDialog();
    } else if (is_uninstall) {
      // Only show the message box if Chrome Frame was the only product being
      // uninstalled.
      const Products& products = installer_state.products();
      int num_products = 0;
      for (Products::const_iterator it = products.begin(); it < products.end();
           ++it) {
        if (!(*it)->is_chrome_binaries())
          ++num_products;
      }
      if (num_products == 1U) {
        ::MessageBoxW(NULL,
                      installer::GetLocalizedString(
                          IDS_UNINSTALL_COMPLETE_BASE).c_str(),
                      cf_install->distribution()->GetAppShortCutName().c_str(),
                      MB_OK);
      }
    }
  }

  int return_code = 0;
  // MSI demands that custom actions always return 0 (ERROR_SUCCESS) or it will
  // rollback the action. If we're uninstalling we want to avoid this, so always
  // report success, squashing any more informative return codes.
  if (!(installer_state.is_msi() && is_uninstall))
    // Note that we allow the status installer::UNINSTALL_REQUIRES_REBOOT
    // to pass through, since this is only returned on uninstall which is
    // never invoked directly by Google Update.
    return_code = InstallUtil::GetInstallReturnCode(install_status);

  VLOG(1) << "Installation complete, returning: " << return_code;

  return return_code;
}
