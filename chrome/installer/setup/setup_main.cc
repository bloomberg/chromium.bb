// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <msi.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/setup/chrome_frame_ready_mode.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"

#include "installer_util_strings.h"  // NOLINT

using installer::InstallerState;
using installer::InstallationState;
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
                    bool& incremental_install) {
  // First uncompress the payload. This could be a differential
  // update (patch.7z) or full archive (chrome.7z). If this uncompress fails
  // return with error.
  std::wstring unpacked_file;
  int32 ret = LzmaUtil::UnPackArchive(archive.value(), temp_path.value(),
                                      &unpacked_file);
  if (ret != NO_ERROR)
    return ret;

  FilePath uncompressed_archive(temp_path.Append(installer::kChromeArchive));
  scoped_ptr<Version> archive_version(
      installer::GetVersionFromArchiveDir(installer_state.target_path()));

  // Check if this is differential update and if it is, patch it to the
  // installer archive that should already be on the machine. We assume
  // it is a differential installer if chrome.7z is not found.
  if (!file_util::PathExists(uncompressed_archive)) {
    incremental_install = true;
    VLOG(1) << "Differential patch found. Applying to existing archive.";
    if (!archive_version.get()) {
      LOG(ERROR) << "Can not use differential update when Chrome is not "
                 << "installed on the system.";
      return installer::CHROME_NOT_INSTALLED;
    }

    FilePath existing_archive(installer_state.target_path().Append(
        UTF8ToWide(archive_version->GetString())));
    existing_archive = existing_archive.Append(installer::kInstallerDir);
    existing_archive = existing_archive.Append(installer::kChromeArchive);
    if (int i = installer::ApplyDiffPatch(FilePath(existing_archive),
                                           FilePath(unpacked_file),
                                           FilePath(uncompressed_archive))) {
      LOG(ERROR) << "Binary patching failed with error " << i;
      return i;
    }
  }

  // Unpack the uncompressed archive.
  return LzmaUtil::UnPackArchive(uncompressed_archive.value(),
      output_directory.value(), &unpacked_file);
}


// This function is called when --rename-chrome-exe option is specified on
// setup.exe command line. This function assumes an in-use update has happened
// for Chrome so there should be a file called new_chrome.exe on the file
// system and a key called 'opv' in the registry. This function will move
// new_chrome.exe to chrome.exe and delete 'opv' key in one atomic operation.
installer::InstallStatus RenameChromeExecutables(
    const InstallerState& installer_state) {
  const FilePath &target_path = installer_state.target_path();
  FilePath chrome_exe(target_path.Append(installer::kChromeExe));
  FilePath chrome_old_exe(target_path.Append(installer::kChromeOldExe));
  FilePath chrome_new_exe(target_path.Append(installer::kChromeNewExe));

  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddDeleteTreeWorkItem(chrome_old_exe);
  FilePath temp_path;
  if (!file_util::CreateNewTempDirectory(L"chrome_", &temp_path)) {
    LOG(ERROR) << "Failed to create Temp directory " << temp_path.value();
    return installer::RENAME_FAILED;
  }

  install_list->AddCopyTreeWorkItem(chrome_new_exe.value(),
                                    chrome_exe.value(),
                                    temp_path.ToWStringHack(),
                                    WorkItem::IF_DIFFERENT,
                                    std::wstring());
  install_list->AddDeleteTreeWorkItem(chrome_new_exe);

  HKEY reg_root = installer_state.root_key();
  const Products& products = installer_state.products();
  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];
    BrowserDistribution* browser_dist = product->distribution();
    std::wstring version_key(browser_dist->GetVersionKey());
    install_list->AddDeleteRegValueWorkItem(reg_root,
                                            version_key,
                                            google_update::kRegOldVersionField);
    install_list->AddDeleteRegValueWorkItem(reg_root,
                                            version_key,
                                            google_update::kRegRenameCmdField);
  }
  installer::InstallStatus ret = installer::RENAME_SUCCESSFUL;
  if (!install_list->Do()) {
    LOG(ERROR) << "Renaming of executables failed. Rolling back any changes.";
    install_list->Rollback();
    ret = installer::RENAME_FAILED;
  }
  file_util::Delete(temp_path, true);
  return ret;
}

// The supported multi-install modes are:
// --multi-install --chrome --chrome-frame --ready-mode
//   - If a non-multi Chrome Frame installation is present, Chrome Frame is
//     removed from |installer_state|'s list of products (thereby preserving
//     the existing SxS install).
//   - If a multi Chrome Frame installation is present, its options are
//     preserved (i.e., the --ready-mode command-line option is ignored).
// --multi-install --chrome-frame
//   - If a non-multi Chrome Frame installation is present, fail.
//   - If a Chrome installation on a different channel is present, fail.
//   - If --ready-mode and no Chrome installation is present, fail.
//   - If a Chrome installation is present, add it to the set of products to
//     install.
bool CheckMultiInstallConditions(const InstallationState& original_state,
                                 InstallerState* installer_state,
                                 installer::InstallStatus* status) {
  const Products& products = installer_state->products();
  DCHECK(products.size());

  bool is_first_install = true;
  const bool system_level = installer_state->system_install();

  if (installer_state->is_multi_install()) {
    const Product* chrome =
        installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER);
    const Product* chrome_frame =
        installer_state->FindProduct(BrowserDistribution::CHROME_FRAME);
    const ProductState* cf_state =
        original_state.GetProductState(system_level,
                                       BrowserDistribution::CHROME_FRAME);
    if (chrome != NULL) {
      if (chrome_frame != NULL &&
          chrome_frame->HasOption(installer::kOptionReadyMode)) {
        // We're being asked to install Chrome with Chrome Frame in ready-mode.
        // This is an optimistic operation: if a SxS install of Chrome Frame
        // is already present, don't touch it; if a multi-install of Chrome
        // Frame is present, preserve its settings (ready-mode, CEEE).
        if (cf_state != NULL) {
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
    } else if (chrome_frame != NULL) {
      // We're being asked to install or update Chrome Frame alone.
      const ProductState* chrome_state =
          original_state.GetProductState(system_level,
                                         BrowserDistribution::CHROME_BROWSER);
      if (chrome_state != NULL) {
        base::win::RegKey key;
        installer::ChannelInfo cf_channel;
        // Chrome Frame may not yet be installed, so peek into the registry
        // directly to see what channel Google Update has specified.  There will
        // be no value if we're not being managed by Google Update.
        if (key.Open(installer_state->root_key(),
                     chrome_frame->distribution()->GetStateKey().c_str(),
                     KEY_QUERY_VALUE) == ERROR_SUCCESS) {
          cf_channel.Initialize(key);
        }
        // Fail if Chrome is already installed but is on a different update
        // channel.
        if (!cf_channel.EqualsBaseOf(chrome_state->channel())) {
          LOG(ERROR) << "Cannot install Chrome Frame because existing Chrome "
                        "install is on a different update channel.";
          *status = installer::CONFLICTING_CHANNEL_EXISTS;
          InstallUtil::WriteInstallerResult(system_level,
              installer_state->state_key(), *status,
              IDS_INSTALL_CONFLICTING_CHANNEL_EXISTS_BASE, NULL);
          return false;
        }
        // Otherwise, add Chrome to the set of products (making it multi-install
        // in the process) so that it is updated, too.
        scoped_ptr<Product> multi_chrome(new Product(
            BrowserDistribution::GetSpecificDistribution(
                BrowserDistribution::CHROME_BROWSER)));
        multi_chrome->SetOption(installer::kOptionMultiInstall, true);
        chrome = installer_state->AddProduct(&multi_chrome);
        VLOG(1) << "Upgrading existing multi-install Chrome browser along with "
                << chrome_frame->distribution()->GetApplicationName();
      } else if (chrome_frame->HasOption(installer::kOptionReadyMode)) {
        // Chrome Frame with ready-mode is to be installed, yet Chrome is
        // neither installed nor being installed.  Fail.
        LOG(ERROR) << "Cannot install Chrome Frame in ready mode without "
                      "Chrome.";
        *status = installer::READY_MODE_REQUIRES_CHROME;
        InstallUtil::WriteInstallerResult(system_level,
            installer_state->state_key(), *status,
            IDS_INSTALL_READY_MODE_REQUIRES_CHROME_BASE, NULL);
        return false;
      }
    }

    // Fail if we're installing Chrome Frame when a single-install of it is
    // already installed.
    // TODO(grt): Add support for migration of Chrome Frame from single- to
    // multi-install.
    if (chrome_frame != NULL &&
        cf_state != NULL && !cf_state->is_multi_install()) {
      LOG(ERROR) << "Cannot migrate existing Chrome Frame installation to "
                    "multi-install.";
      *status = installer::NON_MULTI_INSTALLATION_EXISTS;
      InstallUtil::WriteInstallerResult(system_level,
          installer_state->state_key(), *status,
          IDS_INSTALL_NON_MULTI_INSTALLATION_EXISTS_BASE, NULL);
      return false;
    }
  } else if (DCHECK_IS_ON()) {
    // It isn't possible to stuff two products into a single-install
    // InstallerState.  Abort the process here in debug builds just in case
    // someone finds a way.
    DCHECK_EQ(1U, products.size());
  }

  return true;
}

// In multi-install, adds all products to |installer_state| that are
// multi-installed and must be updated along with the products already present
// in |installer_state|.
void AddExistingMultiInstalls(const InstallationState& original_state,
                              InstallerState* installer_state) {
  if (installer_state->is_multi_install()) {
    BrowserDistribution::Type product_checks[] = {
      BrowserDistribution::CHROME_BROWSER,
      BrowserDistribution::CHROME_FRAME
    };

    for (size_t i = 0; i < arraysize(product_checks); ++i) {
      BrowserDistribution::Type type = product_checks[i];
      if (!installer_state->FindProduct(type)) {
        const ProductState* state =
            original_state.GetProductState(installer_state->system_install(),
                                           type);
        if ((state != NULL) && state->is_multi_install()) {
          installer_state->AddProductFromState(type, *state);
          VLOG(1) << "Product already installed and must be included: "
                  << BrowserDistribution::GetSpecificDistribution(
                         type)->GetApplicationName();
        }
      }
    }
  }
}

// Checks for compatibility between the current state of the system and the
// desired operation.  Also applies policy that mutates the desired operation;
// specifically, the |installer_state| object.
// Also blocks simultaneous user-level and system-level installs.  In the case
// of trying to install user-level Chrome when system-level exists, the
// existing system-level Chrome is launched.
bool CheckPreInstallConditions(const InstallationState& original_state,
                               InstallerState* installer_state,
                               installer::InstallStatus* status) {
  const Products& products = installer_state->products();
  DCHECK(products.size());

  if (!CheckMultiInstallConditions(original_state, installer_state, status))
    return false;

  bool is_first_install = true;
  const bool system_level = installer_state->system_install();

  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];
    BrowserDistribution* browser_dist = product->distribution();

    // Check for an existing installation of the product.
    const ProductState* product_state =
        original_state.GetProductState(system_level, browser_dist->GetType());
    if (product_state != NULL) {
      is_first_install = false;
      // Block downgrades from multi-install to single-install.
      if (!installer_state->is_multi_install() &&
          product_state->is_multi_install()) {
        LOG(ERROR) << "Multi-install " << browser_dist->GetApplicationName()
                   << " exists; aborting single install.";
        *status = installer::MULTI_INSTALLATION_EXISTS;
        InstallUtil::WriteInstallerResult(system_level,
            installer_state->state_key(), *status,
            IDS_INSTALL_MULTI_INSTALLATION_EXISTS_BASE, NULL);
        return false;
      }
    }

    // Check to avoid simultaneous per-user and per-machine installs.
    const ProductState* other_state =
        original_state.GetProductState(!system_level, browser_dist->GetType());
    if (other_state != NULL) {
      LOG(ERROR) << "Already installed version "
                 << other_state->version().GetString()
                 << " conflicts with the current install mode.";
      if (!system_level && is_first_install && product->is_chrome()) {
        // This is user-level install and there is a system-level chrome
        // installation. Instruct Google Update to launch the existing one.
        // There should be no error dialog.
        FilePath chrome_exe(installer::GetChromeInstallPath(!system_level,
                                                            browser_dist));
        if (chrome_exe.empty()) {
          // If we failed to construct install path. Give up.
          *status = installer::OS_ERROR;
          InstallUtil::WriteInstallerResult(system_level,
              installer_state->state_key(), *status, IDS_INSTALL_OS_ERROR_BASE,
              NULL);
        } else {
          *status = installer::EXISTING_VERSION_LAUNCHED;
          chrome_exe = chrome_exe.Append(installer::kChromeExe);
          CommandLine cmd(chrome_exe);
          cmd.AppendSwitch(switches::kFirstRun);
          InstallUtil::WriteInstallerResult(system_level,
              installer_state->state_key(), *status, 0, NULL);
          VLOG(1) << "Launching existing system-level chrome instead.";
          base::LaunchApp(cmd, false, false, NULL);
        }
        return false;
      }

      // If the following compile assert fires it means that the InstallStatus
      // enumeration changed which will break the contract between the old
      // chrome installed and the new setup.exe that is trying to upgrade.
      COMPILE_ASSERT(installer::CONFLICTING_CHANNEL_EXISTS == 39,
                     dont_change_enum);

      // This is an update, not an install. Omaha should know the difference
      // and not show a dialog.
      *status = system_level ? installer::USER_LEVEL_INSTALL_EXISTS :
                               installer::SYSTEM_LEVEL_INSTALL_EXISTS;
      int str_id = system_level ? IDS_INSTALL_USER_LEVEL_EXISTS_BASE :
                                  IDS_INSTALL_SYSTEM_LEVEL_EXISTS_BASE;
      InstallUtil::WriteInstallerResult(system_level,
          installer_state->state_key(), *status, str_id, NULL);
      return false;
    }
  }

  // See what products are already installed in multi mode.  When we do multi
  // installs, we must upgrade all installations since they share the binaries.
  AddExistingMultiInstalls(original_state, installer_state);

  // If no previous installation of Chrome, make sure installation directory
  // either does not exist or can be deleted (i.e. is not locked by some other
  // process).
  if (is_first_install) {
    if (file_util::PathExists(installer_state->target_path()) &&
        !file_util::Delete(installer_state->target_path(), true)) {
      LOG(ERROR) << "Installation directory "
                 << installer_state->target_path().value()
                 << " exists and can not be deleted.";
      *status = installer::INSTALL_DIR_IN_USE;
      int str_id = IDS_INSTALL_DIR_IN_USE_BASE;
      InstallUtil::WriteInstallerResult(system_level,
          installer_state->state_key(), *status, str_id, NULL);
      return false;
    }
  }

  return true;
}

installer::InstallStatus InstallProducts(
    const InstallationState& original_state,
    const CommandLine& cmd_line,
    const MasterPreferences& prefs,
    InstallerState* installer_state) {
  const bool system_install = installer_state->system_install();
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  if (!CheckPreInstallConditions(original_state, installer_state,
                                 &install_status))
    return install_status;

  // For install the default location for chrome.packed.7z is in current
  // folder, so get that value first.
  FilePath archive(cmd_line.GetProgram().DirName().Append(
      installer::kChromeCompressedArchive));

  // If --install-archive is given, get the user specified value
  if (cmd_line.HasSwitch(installer::switches::kInstallArchive)) {
    archive = cmd_line.GetSwitchValuePath(
        installer::switches::kInstallArchive);
  }
  VLOG(1) << "Archive found to install Chrome " << archive.value();
  const Products& products = installer_state->products();

  // Create a temp folder where we will unpack Chrome archive. If it fails,
  // then we are doomed, so return immediately and no cleanup is required.
  FilePath temp_path;
  if (!file_util::CreateNewTempDirectory(L"chrome_", &temp_path)) {
    LOG(ERROR) << "Could not create temporary path.";
    InstallUtil::WriteInstallerResult(system_install,
        installer_state->state_key(), installer::TEMP_DIR_FAILED,
        IDS_INSTALL_TEMP_DIR_FAILED_BASE, NULL);
    return installer::TEMP_DIR_FAILED;
  }
  VLOG(1) << "created path " << temp_path.value();

  FilePath unpack_path(temp_path.Append(installer::kInstallSourceDir));
  bool incremental_install = false;
  if (UnPackArchive(archive, *installer_state, temp_path, unpack_path,
                    incremental_install)) {
    install_status = installer::UNCOMPRESSION_FAILED;
    InstallUtil::WriteInstallerResult(system_install,
        installer_state->state_key(), install_status,
        IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, NULL);
  } else {
    VLOG(1) << "unpacked to " << unpack_path.value();
    FilePath src_path(unpack_path.Append(installer::kInstallSourceChromeDir));
    scoped_ptr<Version>
        installer_version(installer::GetVersionFromArchiveDir(src_path));
    if (!installer_version.get()) {
      LOG(ERROR) << "Did not find any valid version in installer.";
      install_status = installer::INVALID_ARCHIVE;
      InstallUtil::WriteInstallerResult(system_install,
          installer_state->state_key(), install_status,
          IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
    } else {
      // TODO(tommi): Move towards having only a single version that is common
      // to all products.  Only the package should have a version since it
      // represents all the binaries.  When a single product is upgraded, all
      // currently installed product for the shared binary installation, should
      // (or rather must) be upgraded.
      VLOG(1) << "version to install: " << installer_version->GetString();
      bool higher_version_installed = false;
      for (size_t i = 0; i < installer_state->products().size(); ++i) {
        const Product* product = installer_state->products()[i];
        const ProductState* product_state =
            original_state.GetProductState(system_install,
                                           product->distribution()->GetType());
        if (product_state != NULL &&
            (product_state->version().CompareTo(*installer_version) > 0)) {
          LOG(ERROR) << "Higher version is already installed.";
          higher_version_installed = true;
          install_status = installer::HIGHER_VERSION_EXISTS;

          if (product->is_chrome()) {
            // TODO(robertshield): We should take the installer result text
            // strings from the Product.
            InstallUtil::WriteInstallerResult(system_install,
                installer_state->state_key(), install_status,
                IDS_INSTALL_HIGHER_VERSION_BASE, NULL);
          } else {
            InstallUtil::WriteInstallerResult(system_install,
                installer_state->state_key(), install_status,
                IDS_INSTALL_HIGHER_VERSION_CF_BASE, NULL);
          }
        }
      }

      if (!higher_version_installed) {
        // We want to keep uncompressed archive (chrome.7z) that we get after
        // uncompressing and binary patching. Get the location for this file.
        FilePath archive_to_copy(temp_path.Append(installer::kChromeArchive));
        FilePath prefs_source_path(cmd_line.GetSwitchValueNative(
            installer::switches::kInstallerData));
        install_status = installer::InstallOrUpdateProduct(original_state,
            *installer_state, cmd_line.GetProgram(), archive_to_copy, temp_path,
            prefs_source_path, prefs, *installer_version);

        int install_msg_base = IDS_INSTALL_FAILED_BASE;
        std::wstring chrome_exe;
        if (install_status == installer::SAME_VERSION_REPAIR_FAILED) {
          if (installer_state->FindProduct(BrowserDistribution::CHROME_FRAME)) {
            install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_CF_BASE;
          } else {
            install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_BASE;
          }
        } else if (install_status != installer::INSTALL_FAILED) {
          if (installer_state->target_path().empty()) {
            // If we failed to construct install path, it means the OS call to
            // get %ProgramFiles% or %AppData% failed. Report this as failure.
            install_msg_base = IDS_INSTALL_OS_ERROR_BASE;
            install_status = installer::OS_ERROR;
          } else {
            chrome_exe = installer_state->target_path()
                .Append(installer::kChromeExe).value();
            chrome_exe = L"\"" + chrome_exe + L"\"";
            install_msg_base = 0;
          }
        }

        // Only do Chrome-specific stuff (like launching the browser) if
        // Chrome was specifically requested (rather than being upgraded as
        // part of a multi-install).
        const Product* chrome_install = prefs.install_chrome() ?
            installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER) :
            NULL;

        bool value = false;
        if (chrome_install) {
          prefs.GetBool(
              installer::master_preferences::kDoNotRegisterForUpdateLaunch,
              &value);
        } else {
          value = true;  // Never register.
        }

        bool write_chrome_launch_string = (!value) &&
            (install_status != installer::IN_USE_UPDATED);

        InstallUtil::WriteInstallerResult(system_install,
            installer_state->state_key(), install_status, install_msg_base,
            write_chrome_launch_string ? &chrome_exe : NULL);

        if (install_status == installer::FIRST_INSTALL_SUCCESS) {
          VLOG(1) << "First install successful.";
          if (chrome_install) {
            // We never want to launch Chrome in system level install mode.
            bool do_not_launch_chrome = false;
            prefs.GetBool(
                installer::master_preferences::kDoNotLaunchChrome,
                &do_not_launch_chrome);
            if (!system_install && !do_not_launch_chrome)
              chrome_install->LaunchChrome(installer_state->target_path());
          }
        } else if ((install_status == installer::NEW_VERSION_UPDATED) ||
                   (install_status == installer::IN_USE_UPDATED)) {
          for (size_t i = 0; i < products.size(); ++i) {
            installer::RemoveLegacyRegistryKeys(
                products[i]->distribution());
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
    for (size_t i = 0; i < products.size(); ++i) {
      const Product* product = products[i];
      product->distribution()->LaunchUserExperiment(install_status,
          *installer_version, *product, system_install);
    }
  }

  // Delete temporary files. These include install temporary directory
  // and master profile file if present. Note that we do not care about rollback
  // here and we schedule for deletion on reboot below if the deletes fail. As
  // such, we do not use DeleteTreeWorkItem.
  VLOG(1) << "Deleting temporary directory " << temp_path.value();
  bool cleanup_success = file_util::Delete(temp_path, true);
  if (cmd_line.HasSwitch(installer::switches::kInstallerData)) {
    std::wstring prefs_path = cmd_line.GetSwitchValueNative(
        installer::switches::kInstallerData);
    cleanup_success = file_util::Delete(prefs_path, true) && cleanup_success;
  }

  // The above cleanup has been observed to fail on several users machines.
  // Specifically, it appears that the temp folder may be locked when we try
  // to delete it. This is Rather Bad in the case where we have failed updates
  // as we end up filling users' disks with large-ish temp files. To mitigate
  // this, if we fail to delete the temp folders, then schedule them for
  // deletion at next reboot.
  if (!cleanup_success) {
    ScheduleDirectoryForDeletion(temp_path.ToWStringHack().c_str());
    if (cmd_line.HasSwitch(installer::switches::kInstallerData)) {
      std::wstring prefs_path = cmd_line.GetSwitchValueNative(
          installer::switches::kInstallerData);
      ScheduleDirectoryForDeletion(prefs_path.c_str());
    }
  }

  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];
    product->distribution()->UpdateInstallStatus(
        system_install, incremental_install, prefs.is_multi_install(),
        install_status);
  }
  if (installer_state->is_multi_install()) {
    installer_state->multi_package_binaries_distribution()->UpdateInstallStatus(
        system_install, incremental_install, true, install_status);
  }

  return install_status;
}

installer::InstallStatus UninstallProduct(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const CommandLine& cmd_line,
    const Product& product) {
  bool force = cmd_line.HasSwitch(installer::switches::kForceUninstall);
  const ProductState* product_state =
      original_state.GetProductState(installer_state.system_install(),
                                     product.distribution()->GetType());
  if (product_state != NULL) {
    VLOG(1) << "version on the system: "
            << product_state->version().GetString();
  } else if (!force) {
    LOG(ERROR) << "No Chrome installation found for uninstall.";
    return installer::CHROME_NOT_INSTALLED;
  }

  bool remove_all = !cmd_line.HasSwitch(
      installer::switches::kDoNotRemoveSharedItems);

  return installer::UninstallProduct(original_state, installer_state,
      cmd_line.GetProgram(), product, remove_all, force, cmd_line);
}

installer::InstallStatus ShowEULADialog(const std::wstring& inner_frame) {
  VLOG(1) << "About to show EULA";
  std::wstring eula_path = installer::GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return installer::EULA_REJECTED;
  }
  // Newer versions of the caller pass an inner frame parameter that must
  // be given to the html page being launched.
  if (!inner_frame.empty()) {
    eula_path += L"?innerframe=";
    eula_path += inner_frame;
  }
  installer::EulaHTMLDialog dlg(eula_path);
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

// This method processes any command line options that make setup.exe do
// various tasks other than installation (renaming chrome.exe, showing eula
// among others). This function returns true if any such command line option
// has been found and processed (so setup.exe should exit at that point).
bool HandleNonInstallCmdLineOptions(const InstallationState& original_state,
                                    const CommandLine& cmd_line,
                                    const InstallerState& installer_state,
                                    int* exit_code) {
  bool handled = true;
  // TODO(tommi): Split these checks up into functions and use a data driven
  // map of switch->function.
  if (cmd_line.HasSwitch(installer::switches::kUpdateSetupExe)) {
    installer::InstallStatus status = installer::SETUP_PATCH_FAILED;
    // If --update-setup-exe command line option is given, we apply the given
    // patch to current exe, and store the resulting binary in the path
    // specified by --new-setup-exe. But we need to first unpack the file
    // given in --update-setup-exe.
    FilePath temp_path;
    if (!file_util::CreateNewTempDirectory(L"chrome_", &temp_path)) {
      LOG(ERROR) << "Could not create temporary path.";
    } else {
      std::wstring setup_patch = cmd_line.GetSwitchValueNative(
          installer::switches::kUpdateSetupExe);
      VLOG(1) << "Opening archive " << setup_patch;
      std::wstring uncompressed_patch;
      if (LzmaUtil::UnPackArchive(setup_patch, temp_path.ToWStringHack(),
                                  &uncompressed_patch) == NO_ERROR) {
        FilePath old_setup_exe = cmd_line.GetProgram();
        FilePath new_setup_exe = cmd_line.GetSwitchValuePath(
            installer::switches::kNewSetupExe);
        if (!installer::ApplyDiffPatch(old_setup_exe,
                                       FilePath(uncompressed_patch),
                                       new_setup_exe))
          status = installer::NEW_VERSION_UPDATED;
      }
    }

    *exit_code = InstallUtil::GetInstallReturnCode(status);
    if (*exit_code) {
      LOG(WARNING) << "setup.exe patching failed.";
      InstallUtil::WriteInstallerResult(installer_state.system_install(),
          installer_state.state_key(), status, IDS_SETUP_PATCH_FAILED_BASE,
          NULL);
    }
    file_util::Delete(temp_path, true);
  } else if (cmd_line.HasSwitch(installer::switches::kShowEula)) {
    // Check if we need to show the EULA. If it is passed as a command line
    // then the dialog is shown and regardless of the outcome setup exits here.
    std::wstring inner_frame =
        cmd_line.GetSwitchValueNative(installer::switches::kShowEula);
    *exit_code = ShowEULADialog(inner_frame);
    if (installer::EULA_REJECTED != *exit_code)
      GoogleUpdateSettings::SetEULAConsent(installer_state, true);
  } else if (cmd_line.HasSwitch(
      installer::switches::kRegisterChromeBrowser)) {
    const Product* chrome_install =
        installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER);
    if (chrome_install) {
      // If --register-chrome-browser option is specified, register all
      // Chrome protocol/file associations as well as register it as a valid
      // browser for Start Menu->Internet shortcut. This option should only
      // be used when setup.exe is launched with admin rights. We do not
      // make any user specific changes in this option.
      std::wstring chrome_exe(cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowser));
      std::wstring suffix;
      if (cmd_line.HasSwitch(
          installer::switches::kRegisterChromeBrowserSuffix)) {
        suffix = cmd_line.GetSwitchValueNative(
            installer::switches::kRegisterChromeBrowserSuffix);
      }
      *exit_code = ShellUtil::RegisterChromeBrowser(
          chrome_install->distribution(), chrome_exe, suffix, false);
    } else {
      LOG(DFATAL) << "Can't register browser - Chrome distribution not found";
      *exit_code = installer::UNKNOWN_STATUS;
    }
  } else if (cmd_line.HasSwitch(installer::switches::kRenameChromeExe)) {
    // If --rename-chrome-exe is specified, we want to rename the executables
    // and exit.
    *exit_code = RenameChromeExecutables(installer_state);
  } else if (cmd_line.HasSwitch(
      installer::switches::kRemoveChromeRegistration)) {
    // This is almost reverse of --register-chrome-browser option above.
    // Here we delete Chrome browser registration. This option should only
    // be used when setup.exe is launched with admin rights. We do not
    // make any user specific changes in this option.
    std::wstring suffix;
    if (cmd_line.HasSwitch(
        installer::switches::kRegisterChromeBrowserSuffix)) {
      suffix = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowserSuffix);
    }
    installer::InstallStatus tmp = installer::UNKNOWN_STATUS;
    const Product* chrome_install =
        installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER);
    DCHECK(chrome_install);
    if (chrome_install) {
      installer::DeleteChromeRegistrationKeys(chrome_install->distribution(),
          HKEY_LOCAL_MACHINE, suffix, tmp);
    }
    *exit_code = tmp;
  } else if (cmd_line.HasSwitch(installer::switches::kInactiveUserToast)) {
    // Launch the inactive user toast experiment.
    int flavor = -1;
    base::StringToInt(cmd_line.GetSwitchValueNative(
        installer::switches::kInactiveUserToast), &flavor);
    DCHECK_NE(-1, flavor);
    if (flavor == -1) {
      *exit_code = installer::UNKNOWN_STATUS;
    } else {
      const Products& products = installer_state.products();
      for (size_t i = 0; i < products.size(); ++i) {
        const Product* product = products[i];
        BrowserDistribution* browser_dist = product->distribution();
        browser_dist->InactiveUserToastExperiment(flavor, *product,
            installer_state.target_path());
      }
    }
  } else if (cmd_line.HasSwitch(installer::switches::kSystemLevelToast)) {
    const Products& products = installer_state.products();
    for (size_t i = 0; i < products.size(); ++i) {
      const Product* product = products[i];
      BrowserDistribution* browser_dist = product->distribution();
      // We started as system-level and have been re-launched as user level
      // to continue with the toast experiment.
      scoped_ptr<Version> installed_version(
          InstallUtil::GetChromeVersion(browser_dist,
                                        installer_state.system_install()));
      browser_dist->LaunchUserExperiment(installer::REENTRY_SYS_UPDATE,
                                         *installed_version, *product, true);
    }
  } else if (cmd_line.HasSwitch(
                 installer::switches::kChromeFrameReadyModeOptIn)) {
    *exit_code = InstallUtil::GetInstallReturnCode(
        installer::ChromeFrameReadyModeOptIn(original_state, installer_state));
  } else if (cmd_line.HasSwitch(
                 installer::switches::kChromeFrameReadyModeTempOptOut)) {
    *exit_code = InstallUtil::GetInstallReturnCode(
        installer::ChromeFrameReadyModeTempOptOut(original_state,
                                                  installer_state));
  } else if (cmd_line.HasSwitch(
                 installer::switches::kChromeFrameReadyModeEndTempOptOut)) {
    *exit_code = InstallUtil::GetInstallReturnCode(
        installer::ChromeFrameReadyModeEndTempOptOut(original_state,
                                                     installer_state));
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

// Class to manage COM initialization and uninitialization
class AutoCom {
 public:
  AutoCom() : initialized_(false) { }
  ~AutoCom() {
    if (initialized_) CoUninitialize();
  }
  bool Init(bool system_install) {
    if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK) {
      LOG(ERROR) << "COM initialization failed.";
      return false;
    }
    initialized_ = true;
    return true;
  }

 private:
  bool initialized_;
};





// Returns the Custom information for the client identified by the exe path
// passed in. This information is used for crash reporting.
google_breakpad::CustomClientInfo* GetCustomInfo(const wchar_t* exe_path) {
  std::wstring product;
  std::wstring version;
  scoped_ptr<FileVersionInfo>
      version_info(FileVersionInfo::CreateFileVersionInfo(FilePath(exe_path)));
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
  std::wstring user_sid = kSystemPrincipalSid;

  if (!system_install) {
    if (!base::win::GetUserSidString(&user_sid)) {
      return NULL;
    }
  }

  std::wstring pipe_name = kGoogleUpdatePipeName;
  pipe_name += user_sid;

  google_breakpad::ExceptionHandler* breakpad =
      new google_breakpad::ExceptionHandler(
          temp_directory.ToWStringHack(), NULL, NULL, NULL,
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
  VLOG(1) << "Command Line: " << cmd_line.command_line_string();

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
    InstallUtil::WriteInstallerResult(system_install,
        installer_state.state_key(), installer::OS_NOT_SUPPORTED,
        IDS_INSTALL_OS_NOT_SUPPORTED_BASE, NULL);
    return installer::OS_NOT_SUPPORTED;
  }

  // Initialize COM for use later.
  AutoCom auto_com;
  if (!auto_com.Init(system_install)) {
    InstallUtil::WriteInstallerResult(system_install,
        installer_state.state_key(), installer::OS_ERROR,
        IDS_INSTALL_OS_ERROR_BASE, NULL);
    return installer::OS_ERROR;
  }

  // Some command line options don't work with SxS install/uninstall
  if (InstallUtil::IsChromeSxSProcess()) {
    if (system_install ||
        prefs.is_multi_install() ||
        cmd_line.HasSwitch(installer::switches::kForceUninstall) ||
        cmd_line.HasSwitch(installer::switches::kMakeChromeDefault) ||
        cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser) ||
        cmd_line.HasSwitch(
            installer::switches::kRemoveChromeRegistration) ||
        cmd_line.HasSwitch(installer::switches::kInactiveUserToast) ||
        cmd_line.HasSwitch(installer::switches::kSystemLevelToast)) {
      return installer::SXS_OPTION_NOT_SUPPORTED;
    }
  }

  int exit_code = 0;
  if (HandleNonInstallCmdLineOptions(original_state, cmd_line, installer_state,
                                     &exit_code))
    return exit_code;

  if (system_install && !IsUserAnAdmin()) {
    if (base::win::GetVersion() >= base::win::VERSION_VISTA &&
        !cmd_line.HasSwitch(installer::switches::kRunAsAdmin)) {
      CommandLine new_cmd(CommandLine::NO_PROGRAM);
      new_cmd.AppendArguments(cmd_line, true);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      new_cmd.AppendSwitch(installer::switches::kRunAsAdmin);
      DWORD exit_code = installer::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(new_cmd, &exit_code);
      return exit_code;
    } else {
      LOG(ERROR) << "Non admin user can not install system level Chrome.";
      InstallUtil::WriteInstallerResult(system_install,
          installer_state.state_key(), installer::INSUFFICIENT_RIGHTS,
          IDS_INSTALL_INSUFFICIENT_RIGHTS_BASE, NULL);
      return installer::INSUFFICIENT_RIGHTS;
    }
  }

  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  // If --uninstall option is given, uninstall the identified product(s)
  if (is_uninstall) {
    const Products& products = installer_state.products();
    for (size_t i = 0; i < products.size(); ++i) {
      install_status = UninstallProduct(original_state, installer_state,
          cmd_line, *products[i]);
    }
  } else {
    // If --uninstall option is not specified, we assume it is install case.
    VLOG(1) << "Installing to " << installer_state.target_path().value();
    install_status = InstallProducts(original_state, cmd_line, prefs,
        &installer_state);
  }

  const Product* cf_install =
      installer_state.FindProduct(BrowserDistribution::CHROME_FRAME);

  if (cf_install &&
      !cmd_line.HasSwitch(installer::switches::kForceUninstall)) {
    if (install_status == installer::UNINSTALL_REQUIRES_REBOOT) {
      ShowRebootDialog();
    } else if (is_uninstall) {
      // Only show the message box if Chrome Frame was the only product being
      // uninstalled.
      if (installer_state.products().size() == 1U) {
        ::MessageBoxW(NULL,
                      installer::GetLocalizedString(
                          IDS_UNINSTALL_COMPLETE_BASE).c_str(),
                      cf_install->distribution()->GetApplicationName().c_str(),
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
