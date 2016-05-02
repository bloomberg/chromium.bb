// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_main.h"

#include <windows.h>
#include <msi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/process_startup_helper.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/setup/archive_patch_helper.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/installer_crash_reporting.h"
#include "chrome/installer/setup/installer_metrics.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/util/browser_distribution.h"
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
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/user_experiment.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crash/content/app/crash_switches.h"
#include "components/crash/content/app/run_as_crashpad_handler_win.h"
#include "content/public/common/content_switches.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/installer/util/updating_app_registration_data.h"
#endif

using installer::InstallerState;
using installer::InstallationState;
using installer::InstallationValidator;
using installer::MasterPreferences;
using installer::Product;
using installer::ProductState;
using installer::Products;

namespace {

const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";
const wchar_t kDisplayVersion[] = L"DisplayVersion";
const wchar_t kMsiDisplayVersionOverwriteDelay[] = L"10";  // seconds as string
const wchar_t kMsiProductIdPrefix[] = L"EnterpriseProduct";

// Overwrite an existing DisplayVersion as written by the MSI installer
// with the real version number of Chrome.
LONG OverwriteDisplayVersion(const base::string16& path,
                             const base::string16& value,
                             REGSAM wowkey) {
  base::win::RegKey key;
  LONG result = 0;
  base::string16 existing;
  if ((result = key.Open(HKEY_LOCAL_MACHINE, path.c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE | wowkey))
      != ERROR_SUCCESS) {
    VLOG(1) << "Skipping DisplayVersion update because registry key " << path
            << " does not exist in "
            << (wowkey == KEY_WOW64_64KEY ? "64" : "32") << "bit hive";
    return result;
  }
  if ((result = key.ReadValue(kDisplayVersion, &existing)) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DisplayVersion: " << kDisplayVersion
               << " not found under " << path;
    return result;
  }
  if ((result = key.WriteValue(kDisplayVersion, value.c_str()))
      != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DisplayVersion: " << kDisplayVersion
               << " could not be written under " << path;
    return result;
  }
  VLOG(1) << "Set DisplayVersion at " << path << " to " << value
          << " from " << existing;
  return ERROR_SUCCESS;
}

LONG OverwriteDisplayVersions(const base::string16& product,
                              const base::string16& value) {
  // The version is held in two places.  Frist change it in the MSI Installer
  // registry entry.  It is held under a "squashed guid" key.
  base::string16 reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\"
      L"%ls\\Products\\%ls\\InstallProperties", kSystemPrincipalSid,
      installer::GuidToSquid(product).c_str());
  LONG result1 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_64KEY);

  // The display version also exists under the Unininstall registry key with
  // the original guid.  Check both WOW64_64 and WOW64_32.
  reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{%ls}",
      product.c_str());
  // Consider the operation a success if either of these succeeds.
  LONG result2 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_64KEY);
  LONG result3 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_32KEY);

  return result1 != ERROR_SUCCESS ? result1 :
      (result2 != ERROR_SUCCESS ? result3 : ERROR_SUCCESS);
}

void DelayedOverwriteDisplayVersions(const base::FilePath& setup_exe,
                                     const std::string& id,
                                     const Version& version) {
  // This process has to be able to exit so we launch ourselves with
  // instructions on what to do and then return.
  base::CommandLine command_line(setup_exe);
  command_line.AppendSwitchASCII(installer::switches::kSetDisplayVersionProduct,
                                 id);
  command_line.AppendSwitchASCII(installer::switches::kSetDisplayVersionValue,
                                 version.GetString());
  command_line.AppendSwitchNative(installer::switches::kDelay,
                                  kMsiDisplayVersionOverwriteDelay);

  base::LaunchOptions launch_options;
  launch_options.force_breakaway_from_job_ = true;
  base::Process writer = base::LaunchProcess(command_line, launch_options);
  if (!writer.IsValid()) {
    PLOG(ERROR) << "Failed to set DisplayVersion: "
                << "could not launch subprocess to make desired changes."
                << " <<" << command_line.GetCommandLineString() << ">>";
  }
}

// Returns NULL if no compressed archive is available for processing, otherwise
// returns a patch helper configured to uncompress and patch.
std::unique_ptr<installer::ArchivePatchHelper> CreateChromeArchiveHelper(
    const base::FilePath& setup_exe,
    const base::CommandLine& command_line,
    const installer::InstallerState& installer_state,
    const base::FilePath& working_directory) {
  // A compressed archive is ordinarily given on the command line by the mini
  // installer. If one was not given, look for chrome.packed.7z next to the
  // running program.
  base::FilePath compressed_archive(
      command_line.GetSwitchValuePath(installer::switches::kInstallArchive));
  bool compressed_archive_specified = !compressed_archive.empty();
  if (!compressed_archive_specified) {
    compressed_archive = setup_exe.DirName().Append(
        installer::kChromeCompressedArchive);
  }

  // Fail if no compressed archive is found.
  if (!base::PathExists(compressed_archive)) {
    if (compressed_archive_specified) {
      LOG(ERROR) << installer::switches::kInstallArchive << "="
                 << compressed_archive.value() << " not found.";
    }
    return std::unique_ptr<installer::ArchivePatchHelper>();
  }

  // chrome.7z is either extracted directly from the compressed archive into the
  // working dir or is the target of patching in the working dir.
  base::FilePath target(working_directory.Append(installer::kChromeArchive));
  DCHECK(!base::PathExists(target));

  // Specify an empty path for the patch source since it isn't yet known that
  // one is needed. It will be supplied in UncompressAndPatchChromeArchive if it
  // is.
  return std::unique_ptr<installer::ArchivePatchHelper>(
      new installer::ArchivePatchHelper(working_directory, compressed_archive,
                                        base::FilePath(), target));
}

// Returns the MSI product ID from the ClientState key that is populated for MSI
// installs.  This property is encoded in a value name whose format is
// "EnterpriseId<GUID>" where <GUID> is the MSI product id.  <GUID> is in the
// format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.  The id will be returned if
// found otherwise this method will return an empty string.
//
// This format is strange and its provenance is shrouded in mystery but it has
// the data we need, so use it.
base::string16 FindMsiProductId(const InstallerState& installer_state,
                                const Product* product) {
  HKEY reg_root = installer_state.root_key();
  BrowserDistribution* dist = product->distribution();
  DCHECK(dist);

  base::win::RegistryValueIterator value_iter(reg_root,
                                              dist->GetStateKey().c_str());
  for (; value_iter.Valid(); ++value_iter) {
    base::string16 value_name(value_iter.Name());
    if (base::StartsWith(value_name, kMsiProductIdPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return value_name.substr(arraysize(kMsiProductIdPrefix) - 1);
    }
  }
  return base::string16();
}

// Workhorse for producing an uncompressed archive (chrome.7z) given a
// chrome.packed.7z containing either a patch file based on the version of
// chrome being updated or the full uncompressed archive. Returns true on
// success, in which case |archive_type| is populated based on what was found.
// Returns false on failure, in which case |install_status| contains the error
// code and the result is written to the registry (via WriteInstallerResult).
bool UncompressAndPatchChromeArchive(
    const installer::InstallationState& original_state,
    const installer::InstallerState& installer_state,
    installer::ArchivePatchHelper* archive_helper,
    installer::ArchiveType* archive_type,
    installer::InstallStatus* install_status,
    const base::Version& previous_version) {
  installer_state.UpdateStage(installer::UNCOMPRESSING);
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (!archive_helper->Uncompress(NULL)) {
    *install_status = installer::UNCOMPRESSION_FAILED;
    installer_state.WriteInstallerResult(*install_status,
                                         IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                         NULL);
    return false;
  }
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  bool has_full_archive = base::PathExists(archive_helper->target());
  if (installer_state.is_background_mode()) {
    UMA_HISTOGRAM_BOOLEAN("Setup.Install.HasArchivePatch.background",
                          !has_full_archive);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Setup.Install.HasArchivePatch", !has_full_archive);
  }

  // Short-circuit if uncompression produced the uncompressed archive rather
  // than a patch file.
  if (has_full_archive) {
    *archive_type = installer::FULL_ARCHIVE_TYPE;
    // Uncompression alone hopefully takes less than 3 minutes even on slow
    // machines.
    if (installer_state.is_background_mode()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Setup.Install.UncompressFullArchiveTime.background", elapsed_time);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Setup.Install.UncompressFullArchiveTime", elapsed_time);
    }
    return true;
  }

  if (installer_state.is_background_mode()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Setup.Install.UncompressArchivePatchTime.background", elapsed_time);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Setup.Install.UncompressArchivePatchTime", elapsed_time);
  }

  // Find the installed version's archive to serve as the source for patching.
  base::FilePath patch_source(installer::FindArchiveToPatch(original_state,
                                                            installer_state,
                                                            previous_version));
  if (patch_source.empty()) {
    LOG(ERROR) << "Failed to find archive to patch.";
    *install_status = installer::DIFF_PATCH_SOURCE_MISSING;
    installer_state.WriteInstallerResult(*install_status,
                                         IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                         NULL);
    return false;
  }
  archive_helper->set_patch_source(patch_source);

  // Try courgette first. Failing that, try bspatch.
  // Patch application sometimes takes a very long time, so use 100 buckets for
  // up to an hour.
  start_time = base::TimeTicks::Now();
  installer_state.UpdateStage(installer::ENSEMBLE_PATCHING);
  if (!archive_helper->EnsemblePatch()) {
    installer_state.UpdateStage(installer::BINARY_PATCHING);
    if (!archive_helper->BinaryPatch()) {
      *install_status = installer::APPLY_DIFF_PATCH_FAILED;
      installer_state.WriteInstallerResult(
          *install_status, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, NULL);
      return false;
    }
  }

  // Record patch time only if it was successful.
  elapsed_time = base::TimeTicks::Now() - start_time;
  if (installer_state.is_background_mode()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Setup.Install.ApplyArchivePatchTime.background", elapsed_time);
  } else {
    UMA_HISTOGRAM_LONG_TIMES(
        "Setup.Install.ApplyArchivePatchTime", elapsed_time);
  }

  *archive_type = installer::INCREMENTAL_ARCHIVE_TYPE;
  return true;
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
                         GetDisplayName();
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
  const base::FilePath &target_path = installer_state->target_path();
  base::FilePath chrome_exe(target_path.Append(installer::kChromeExe));
  base::FilePath chrome_new_exe(target_path.Append(installer::kChromeNewExe));
  base::FilePath chrome_old_exe(target_path.Append(installer::kChromeOldExe));

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
  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
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

  // Add work items to delete the "opv", "cpv", and "cmd" values from all
  // products we're operating on (which including the multi-install binaries).
  const Products& products = installer_state->products();
  HKEY reg_root = installer_state->root_key();
  base::string16 version_key;
  for (Products::const_iterator it = products.begin(); it < products.end();
       ++it) {
    version_key = (*it)->distribution()->GetVersionKey();
    install_list->AddDeleteRegValueWorkItem(reg_root,
                                            version_key,
                                            KEY_WOW64_32KEY,
                                            google_update::kRegOldVersionField);
    install_list->AddDeleteRegValueWorkItem(
        reg_root,
        version_key,
        KEY_WOW64_32KEY,
        google_update::kRegCriticalVersionField);
    install_list->AddDeleteRegValueWorkItem(reg_root,
                                            version_key,
                                            KEY_WOW64_32KEY,
                                            google_update::kRegRenameCmdField);
  }
  // old_chrome.exe is still in use in most cases, so ignore failures here.
  // Make sure this is the last item in the list because it cannot be rolled
  // back.
  install_list->AddDeleteTreeWorkItem(chrome_old_exe, temp_path.path())->
      set_ignore_failure(true);

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

// If only the binaries are being updated, fail.
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
    const Product* binaries =
        installer_state->FindProduct(BrowserDistribution::CHROME_BINARIES);
    const ProductState* chrome_state =
        original_state.GetProductState(system_level,
                                       BrowserDistribution::CHROME_BROWSER);

    if (binaries) {
      if (products.size() == 1) {
        // There are no products aside from the binaries, so there is no update
        // to be applied. This can happen after multi-install Chrome Frame is
        // migrated to single-install. This is treated as an update failure
        // unless the binaries are not in-use, in which case they will be
        // uninstalled and success will be reported (see handling in wWinMain).
        VLOG(1) << "No products to be updated.";
        *status = installer::UNUSED_BINARIES;
        installer_state->WriteInstallerResult(*status, 0, NULL);
        return false;
      }
    } else {
      // This will only be hit if --multi-install is given with no products.
      return true;
    }

    if (!chrome && chrome_state) {
      // A product other than Chrome is being installed in multi-install mode,
      // and Chrome is already present. Add Chrome to the set of products
      // (making it multi-install in the process) so that it is updated, too.
      std::unique_ptr<Product> multi_chrome(
          new Product(BrowserDistribution::GetSpecificDistribution(
              BrowserDistribution::CHROME_BROWSER)));
      multi_chrome->SetOption(installer::kOptionMultiInstall, true);
      chrome = installer_state->AddProduct(&multi_chrome);
      VLOG(1) << "Upgrading existing Chrome browser in multi-install mode.";
    }
  }  // else migrate multi-install Chrome to single-install.

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
          base::FilePath install_path(installer::GetChromeInstallPath(
              true,  // system
              browser_dist));
          if (install_path.empty()) {
            // Give up if we failed to construct the install path.
            *status = installer::OS_ERROR;
            installer_state->WriteInstallerResult(*status,
                                                  IDS_INSTALL_OS_ERROR_BASE,
                                                  NULL);
          } else {
            *status = installer::EXISTING_VERSION_LAUNCHED;
            base::FilePath chrome_exe =
                install_path.Append(installer::kChromeExe);
            base::CommandLine cmd(chrome_exe);
            cmd.AppendSwitch(switches::kForceFirstRun);
            installer_state->WriteInstallerResult(*status, 0, NULL);
            VLOG(1) << "Launching existing system-level chrome instead.";
            base::LaunchProcess(cmd, base::LaunchOptions());
          }
        } else {
          // It's no longer possible for |product| to be anything other than
          // Chrome.
          NOTREACHED();
        }
        return false;
      }
    }
  }

  return true;
}

// Initializes |temp_path| to "Temp" within the target directory, and
// |unpack_path| to a random directory beginning with "source" within
// |temp_path|. Returns false on error.
bool CreateTemporaryAndUnpackDirectories(
    const InstallerState& installer_state,
    installer::SelfCleaningTempDir* temp_path,
    base::FilePath* unpack_path) {
  DCHECK(temp_path && unpack_path);

  if (!temp_path->Initialize(installer_state.target_path().DirName(),
                             installer::kInstallTempDir)) {
    PLOG(ERROR) << "Could not create temporary path.";
    return false;
  }
  VLOG(1) << "Created path " << temp_path->path().value();

  if (!base::CreateTemporaryDirInDir(temp_path->path(),
                                     installer::kInstallSourceDir,
                                     unpack_path)) {
    PLOG(ERROR) << "Could not create temporary path for unpacked archive.";
    return false;
  }

  return true;
}

installer::InstallStatus UninstallProduct(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
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
    LOG(ERROR) << product.distribution()->GetDisplayName()
               << " not found for uninstall.";
    return installer::CHROME_NOT_INSTALLED;
  }

  return installer::UninstallProduct(
      original_state, installer_state, setup_exe, product, remove_all,
      force_uninstall, cmd_line);
}

installer::InstallStatus UninstallProducts(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line) {
  const Products& products = installer_state.products();

  // System-level Chrome will be launched via this command if its program gets
  // set below.
  base::CommandLine system_level_cmd(base::CommandLine::NO_PROGRAM);

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
      const base::FilePath system_exe_path(
          installer::GetChromeInstallPath(true, dist)
              .Append(installer::kChromeExe));
      system_level_cmd.SetProgram(system_exe_path);
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
    prod_status = UninstallProduct(original_state, installer_state, setup_exe,
        cmd_line, remove_all, force, **it);
    if (prod_status != installer::UNINSTALL_SUCCESSFUL)
      install_status = prod_status;
  }

  installer::CleanUpInstallationDirectoryAfterUninstall(
      original_state, installer_state, setup_exe, &install_status);

  // The app and vendor dirs may now be empty. Make a last-ditch attempt to
  // delete them.
  installer::DeleteChromeDirectoriesIfEmpty(installer_state.target_path());

  // Trigger Active Setup if it was requested for the chrome product. This needs
  // to be done after the UninstallProduct calls as some of them might
  // otherwise terminate the process launched by TriggerActiveSetupCommand().
  if (chrome && cmd_line.HasSwitch(installer::switches::kTriggerActiveSetup))
    InstallUtil::TriggerActiveSetupCommand();

  if (!system_level_cmd.GetProgram().empty())
    base::LaunchProcess(system_level_cmd, base::LaunchOptions());

  // Tell Google Update that an uninstall has taken place.
  // Ignore the return value: success or failure of Google Update
  // has no bearing on the success or failure of Chrome's uninstallation.
  google_update::UninstallGoogleUpdate(installer_state.system_install());

  return install_status;
}

// Uninstall the binaries if they are the only product present and they're not
// in-use.
void UninstallBinariesIfUnused(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    installer::InstallStatus* install_status) {
  // Early exit if the binaries are still in use.
  if (*install_status != installer::UNUSED_BINARIES ||
      installer_state.AreBinariesInUse(original_state)) {
    return;
  }

  LOG(INFO) << "Uninstalling unused binaries";
  installer_state.UpdateStage(installer::UNINSTALLING_BINARIES);

  // Simulate the uninstall as coming from the installed version.
  const ProductState* binaries_state =
      original_state.GetProductState(installer_state.system_install(),
                                     BrowserDistribution::CHROME_BINARIES);
  const base::CommandLine& uninstall_cmd(binaries_state->uninstall_command());
  MasterPreferences uninstall_prefs(uninstall_cmd);
  InstallerState uninstall_state;
  uninstall_state.Initialize(uninstall_cmd, uninstall_prefs, original_state);

  *install_status = UninstallProducts(original_state, uninstall_state,
                                      uninstall_cmd.GetProgram(),
                                      uninstall_cmd);

  // Report that the binaries were uninstalled if they were. This translates
  // into a successful install return code.
  if (IsUninstallSuccess(*install_status)) {
    *install_status = installer::UNUSED_BINARIES_UNINSTALLED;
    installer_state.WriteInstallerResult(*install_status, 0, NULL);
  }
}

// This function is a short-term repair for the damage documented in
// http://crbug.com/456602. Briefly: canaries from 42.0.2293.0 through
// 42.0.2302.0 (inclusive) contained a bug that broke normal Chrome installed at
// user-level. This function detects the broken state during a canary update and
// repairs it by calling on the existing Chrome's installer to fix itself.
// TODO(grt): Remove this once the majority of impacted canary clients have
// picked it up.
void RepairChromeIfBroken(const InstallationState& original_state,
                          const InstallerState& installer_state) {
#if !defined(GOOGLE_CHROME_BUILD)
  // Chromium does not support SxS installation, so there is no work to be done.
  return;
#else  // GOOGLE_CHROME_BUILD
  // Nothing to do if not a per-user SxS install/update.
  if (!InstallUtil::IsChromeSxSProcess() ||
      installer_state.system_install() ||
      installer_state.is_multi_install()) {
    return;
  }

  // When running a side-by-side install, BrowserDistribution provides no way
  // to create or access a GoogleChromeDistribution (by design).
  static const base::char16 kChromeGuid[] =
      L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
  static const base::char16 kChromeBinariesGuid[] =
      L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

  UpdatingAppRegistrationData chrome_reg_data(kChromeGuid);
  UpdatingAppRegistrationData binaries_reg_data(kChromeBinariesGuid);

  // Nothing to do if the binaries are installed.
  base::win::RegKey key;
  base::string16 version_str;
  if (key.Open(HKEY_CURRENT_USER,
               binaries_reg_data.GetVersionKey().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(google_update::kRegVersionField,
                    &version_str) == ERROR_SUCCESS) {
    return;
  }

  // Nothing to do if Chrome is not installed.
  if (key.Open(HKEY_CURRENT_USER,
               chrome_reg_data.GetVersionKey().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) != ERROR_SUCCESS ||
      key.ReadValue(google_update::kRegVersionField,
                    &version_str) != ERROR_SUCCESS) {
    return;
  }

  // Nothing to do if Chrome is not multi-install.
  base::string16 setup_args;
  if (key.Open(HKEY_CURRENT_USER,
               chrome_reg_data.GetStateKey().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) != ERROR_SUCCESS) {
    LOG(ERROR) << "RepairChrome: Failed to open Chrome's ClientState key.";
    return;
  }
  if (key.ReadValue(installer::kUninstallArgumentsField,
                    &setup_args) != ERROR_SUCCESS) {
    LOG(ERROR) << "RepairChrome: Failed to read Chrome's UninstallArguments.";
    return;
  }
  if (setup_args.find(base::UTF8ToUTF16(installer::switches::kMultiInstall)) ==
      base::string16::npos) {
    LOG(INFO) << "RepairChrome: Not repairing single-install Chrome.";
    return;
  }

  // Generate a command line to run Chrome's installer.
  base::string16 setup_path;
  if (key.ReadValue(installer::kUninstallStringField,
                    &setup_path) != ERROR_SUCCESS) {
    LOG(ERROR) << "RepairChrome: Failed to read Chrome's UninstallString.";
    return;
  }

  // Replace --uninstall with --do-not-launch-chrome to cause chrome to
  // self-repair.
  base::ReplaceFirstSubstringAfterOffset(
      &setup_args, 0, base::UTF8ToUTF16(installer::switches::kUninstall),
      base::UTF8ToUTF16(installer::switches::kDoNotLaunchChrome));
  base::CommandLine setup_command(base::CommandLine::NO_PROGRAM);
  InstallUtil::ComposeCommandLine(setup_path, setup_args, &setup_command);

  // Run Chrome's installer so that it repairs itself. Break away from any job
  // in which this operation is running so that Google Update doesn't wait
  // around for the repair. Retry once without the attempt to break away in case
  // this process doesn't have JOB_OBJECT_LIMIT_BREAKAWAY_OK.
  base::LaunchOptions launch_options;
  launch_options.force_breakaway_from_job_ = true;
  while (true) {
    if (base::LaunchProcess(setup_command, launch_options).IsValid()) {
      LOG(INFO) << "RepairChrome: Launched repair command \""
                << setup_command.GetCommandLineString() << "\"";
      break;
    } else {
      PLOG(ERROR) << "RepairChrome: Failed launching repair command \""
                  << setup_command.GetCommandLineString() << "\"";
      if (launch_options.force_breakaway_from_job_) {
        LOG(ERROR) << "RepairChrome: Will retry without breakaway.";
        launch_options.force_breakaway_from_job_ = false;
      } else {
        break;
      }
    }
  }
#endif  // GOOGLE_CHROME_BUILD
}

installer::InstallStatus InstallProducts(
    const InstallationState& original_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    const MasterPreferences& prefs,
    InstallerState* installer_state,
    base::FilePath* installer_directory) {
  DCHECK(installer_state);
  const bool system_install = installer_state->system_install();
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  installer::ArchiveType archive_type = installer::UNKNOWN_ARCHIVE_TYPE;
  installer_state->UpdateStage(installer::PRECONDITIONS);
  // The stage provides more fine-grained information than -multifail, so remove
  // the -multifail suffix from the Google Update "ap" value.
  BrowserDistribution::GetSpecificDistribution(installer_state->state_type())->
      UpdateInstallStatus(system_install, archive_type, install_status);

  // Drop to background processing mode if the process was started below the
  // normal process priority class. This is done here because InstallProducts-
  // Helper has read-only access to the state and because the action also
  // affects everything else that runs below.
  bool entered_background_mode = installer::AdjustProcessPriority();
  installer_state->set_background_mode(entered_background_mode);
  VLOG_IF(1, entered_background_mode) << "Entered background processing mode.";

  if (CheckPreInstallConditions(original_state, installer_state,
                                &install_status)) {
    VLOG(1) << "Installing to " << installer_state->target_path().value();
    install_status = InstallProductsHelper(
        original_state, setup_exe, cmd_line, prefs, *installer_state,
        installer_directory, &archive_type);
  } else {
    // CheckPreInstallConditions must set the status on failure.
    DCHECK_NE(install_status, installer::UNKNOWN_STATUS);
  }

  // Delete the master preferences file if present. Note that we do not care
  // about rollback here and we schedule for deletion on reboot if the delete
  // fails. As such, we do not use DeleteTreeWorkItem.
  if (cmd_line.HasSwitch(installer::switches::kInstallerData)) {
    base::FilePath prefs_path(cmd_line.GetSwitchValuePath(
        installer::switches::kInstallerData));
    if (!base::DeleteFile(prefs_path, false)) {
      LOG(ERROR) << "Failed deleting master preferences file "
                 << prefs_path.value()
                 << ", scheduling for deletion after reboot.";
      ScheduleFileSystemEntityForDeletion(prefs_path);
    }
  }

  const Products& products = installer_state->products();
  for (Products::const_iterator it = products.begin(); it < products.end();
       ++it) {
    (*it)->distribution()->UpdateInstallStatus(
        system_install, archive_type, install_status);
  }

  UninstallBinariesIfUnused(original_state, *installer_state, &install_status);

  RepairChromeIfBroken(original_state, *installer_state);

  installer_state->UpdateStage(installer::NO_STAGE);
  return install_status;
}

installer::InstallStatus ShowEULADialog(const base::string16& inner_frame) {
  VLOG(1) << "About to show EULA";
  base::string16 eula_path = installer::GetLocalizedEulaResource();
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
  base::FilePath eula_sentinel;
  if (!InstallUtil::GetEULASentinelFilePath(&eula_sentinel))
    return false;

  return (base::CreateDirectory(eula_sentinel.DirName()) &&
          base::WriteFile(eula_sentinel, "", 0) != -1);
}

void ActivateMetroChrome() {
  // Check to see if we're per-user or not. Need to do this since we may
  // not have been invoked with --system-level even for a machine install.
  base::FilePath exe_path;
  PathService::Get(base::FILE_EXE, &exe_path);
  bool is_per_user_install = InstallUtil::IsPerUserInstall(exe_path);

  base::string16 app_model_id = ShellUtil::GetBrowserModelId(
      BrowserDistribution::GetDistribution(), is_per_user_install);

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
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line) {
  BrowserDistribution* chrome_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER);

  // Only proceed with registering a dev chrome if no real Chrome installation
  // of the same distribution are present on this system.
  const ProductState* existing_chrome =
      original_state.GetProductState(false,
                                     BrowserDistribution::CHROME_BROWSER);
  if (!existing_chrome) {
    existing_chrome =
      original_state.GetProductState(true, BrowserDistribution::CHROME_BROWSER);
  }
  if (existing_chrome) {
    static const wchar_t kPleaseUninstallYourChromeMessage[] =
        L"You already have a full-installation (non-dev) of %1ls, please "
        L"uninstall it first using Add/Remove Programs in the control panel.";
    base::string16 name(chrome_dist->GetDisplayName());
    base::string16 message(
        base::StringPrintf(kPleaseUninstallYourChromeMessage, name.c_str()));

    LOG(ERROR) << "Aborting operation: another installation of " << name
               << " was found, as a last resort (if the product is not present "
                  "in Add/Remove Programs), try executing: "
               << existing_chrome->uninstall_command().GetCommandLineString();
    MessageBox(NULL, message.c_str(), NULL, MB_ICONERROR);
    return installer::INSTALL_FAILED;
  }

  base::FilePath chrome_exe(
      cmd_line.GetSwitchValuePath(installer::switches::kRegisterDevChrome));
  if (chrome_exe.empty())
    chrome_exe = setup_exe.DirName().Append(installer::kChromeExe);
  if (!chrome_exe.IsAbsolute())
    chrome_exe = base::MakeAbsoluteFilePath(chrome_exe);

  installer::InstallStatus status = installer::FIRST_INSTALL_SUCCESS;
  if (base::PathExists(chrome_exe)) {
    Product chrome(chrome_dist);

    // Create the Start menu shortcut and pin it to the Win7+ taskbar.
    ShellUtil::ShortcutProperties shortcut_properties(ShellUtil::CURRENT_USER);
    chrome.AddDefaultShortcutProperties(chrome_exe, &shortcut_properties);
    shortcut_properties.set_pin_to_taskbar(true);
    ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, chrome_dist,
        shortcut_properties, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS);

    // Register Chrome at user-level and make it default.
    if (ShellUtil::CanMakeChromeDefaultUnattended()) {
      ShellUtil::MakeChromeDefault(chrome_dist, ShellUtil::CURRENT_USER,
                                   chrome_exe, true);
    } else {
      ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_dist, chrome_exe);
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
                                    const base::FilePath& setup_exe,
                                    const base::CommandLine& cmd_line,
                                    InstallerState* installer_state,
                                    int* exit_code) {
  // This option is independent of all others so doesn't belong in the if/else
  // block below.
  if (cmd_line.HasSwitch(installer::switches::kDelay)) {
    const std::string delay_seconds_string(
        cmd_line.GetSwitchValueASCII(installer::switches::kDelay));
    int delay_seconds;
    if (base::StringToInt(delay_seconds_string, &delay_seconds) &&
        delay_seconds > 0) {
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(delay_seconds));
    }
  }

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
      base::FilePath compressed_archive(cmd_line.GetSwitchValuePath(
          installer::switches::kUpdateSetupExe));
      VLOG(1) << "Opening archive " << compressed_archive.value();
      if (installer::ArchivePatchHelper::UncompressAndPatch(
              temp_path.path(),
              compressed_archive,
              setup_exe,
              cmd_line.GetSwitchValuePath(installer::switches::kNewSetupExe))) {
        status = installer::NEW_VERSION_UPDATED;
      }
      if (!temp_path.Delete()) {
        // PLOG would be nice, but Delete() doesn't leave a meaningful value in
        // the Windows last-error code.
        LOG(WARNING) << "Scheduling temporary path " << temp_path.path().value()
                     << " for deletion at reboot.";
        ScheduleDirectoryForDeletion(temp_path.path());
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
    base::string16 inner_frame =
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
        original_state, *installer_state, setup_exe, cmd_line);
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
      base::FilePath chrome_exe(cmd_line.GetSwitchValuePath(
          installer::switches::kRegisterChromeBrowser));
      base::string16 suffix;
      if (cmd_line.HasSwitch(
          installer::switches::kRegisterChromeBrowserSuffix)) {
        suffix = cmd_line.GetSwitchValueNative(
            installer::switches::kRegisterChromeBrowserSuffix);
      }
      if (cmd_line.HasSwitch(installer::switches::kRegisterURLProtocol)) {
        base::string16 protocol = cmd_line.GetSwitchValueNative(
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
    base::string16 suffix;
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
      std::unique_ptr<FileVersionInfo> version_info(
          FileVersionInfo::CreateFileVersionInfo(setup_exe));
      const base::Version installed_version(
          base::UTF16ToUTF8(version_info->product_version()));
      if (installed_version.IsValid()) {
        installer::HandleOsUpgradeForBrowser(*installer_state, *chrome_install,
                                             installed_version);
        status = installer::INSTALL_REPAIRED;
      } else {
        LOG(DFATAL) << "Failed to extract product version from "
                    << setup_exe.value();
      }
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
      // This code is called (via setup.exe relaunch) only if a product is known
      // to run user experiments, so no check is required.
      const Products& products = installer_state->products();
      for (Products::const_iterator it = products.begin(); it < products.end();
           ++it) {
        const Product& product = **it;
        installer::InactiveUserToastExperiment(
            flavor, base::ASCIIToUTF16(experiment_group), product,
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
                   << browser_dist->GetDisplayName()
                   << " found for system-level toast.";
      } else {
        product.LaunchUserExperiment(
            setup_exe, installer::REENTRY_SYS_UPDATE, true);
      }
    }
  } else if (cmd_line.HasSwitch(installer::switches::kPatch)) {
    const std::string patch_type_str(
        cmd_line.GetSwitchValueASCII(installer::switches::kPatch));
    const base::FilePath input_file(
        cmd_line.GetSwitchValuePath(installer::switches::kInputFile));
    const base::FilePath patch_file(
        cmd_line.GetSwitchValuePath(installer::switches::kPatchFile));
    const base::FilePath output_file(
        cmd_line.GetSwitchValuePath(installer::switches::kOutputFile));

    if (patch_type_str == installer::kCourgette) {
      *exit_code = installer::CourgettePatchFiles(input_file,
                                                  patch_file,
                                                  output_file);
    } else if (patch_type_str == installer::kBsdiff) {
      *exit_code = installer::BsdiffPatchFiles(input_file,
                                               patch_file,
                                               output_file);
    } else {
      *exit_code = installer::PATCH_INVALID_ARGUMENTS;
    }
  } else if (cmd_line.HasSwitch(installer::switches::kReenableAutoupdates)) {
    // setup.exe has been asked to attempt to reenable updates for Chrome.
    bool updates_enabled = GoogleUpdateSettings::ReenableAutoupdates();
    *exit_code = updates_enabled ? installer::REENABLE_UPDATES_SUCCEEDED :
                                   installer::REENABLE_UPDATES_FAILED;
  } else if (cmd_line.HasSwitch(
                 installer::switches::kSetDisplayVersionProduct)) {
    const base::string16 registry_product(
        cmd_line.GetSwitchValueNative(
            installer::switches::kSetDisplayVersionProduct));
    const base::string16 registry_value(
        cmd_line.GetSwitchValueNative(
            installer::switches::kSetDisplayVersionValue));
    *exit_code = OverwriteDisplayVersions(registry_product, registry_value);
  } else {
    handled = false;
  }

  return handled;
}

// Uninstalls multi-install Chrome Frame if the current operation is a
// multi-install install or update. The operation is performed directly rather
// than delegated to the existing install since there is no facility in older
// versions of setup.exe to uninstall GCF without touching the binaries. The
// binaries will be uninstalled during later processing if they are not in-use
// (see UninstallBinariesIfUnused). |original_state| and |installer_state| are
// updated to reflect the state of the world following the operation.
void UninstallMultiChromeFrameIfPresent(const base::CommandLine& cmd_line,
                                        const MasterPreferences& prefs,
                                        InstallationState* original_state,
                                        InstallerState* installer_state) {
  // Early exit if not installing or updating.
  if (installer_state->operation() == InstallerState::UNINSTALL)
    return;

  // Early exit if Chrome Frame is not present as multi-install.
  const ProductState* chrome_frame_state =
      original_state->GetProductState(installer_state->system_install(),
                                      BrowserDistribution::CHROME_FRAME);
  if (!chrome_frame_state || !chrome_frame_state->is_multi_install())
    return;

  LOG(INFO) << "Uninstalling multi-install Chrome Frame.";
  installer_state->UpdateStage(installer::UNINSTALLING_CHROME_FRAME);

  // Uninstall Chrome Frame without touching the multi-install binaries.
  // Simulate the uninstall as coming from the installed version.
  const base::CommandLine& uninstall_cmd(
      chrome_frame_state->uninstall_command());
  MasterPreferences uninstall_prefs(uninstall_cmd);
  InstallerState uninstall_state;
  uninstall_state.Initialize(uninstall_cmd, uninstall_prefs, *original_state);
  // Post M32, uninstall_prefs and uninstall_state won't have Chrome Frame in
  // them since they've lost the power to do Chrome Frame installs.
  const Product* chrome_frame_product = uninstall_state.AddProductFromState(
      BrowserDistribution::CHROME_FRAME, *chrome_frame_state);
  if (chrome_frame_product) {
    // No shared state should be left behind.
    const bool remove_all = true;
    // Don't accept no for an answer.
    const bool force_uninstall = true;
    installer::InstallStatus uninstall_status =
        installer::UninstallProduct(*original_state, uninstall_state,
                                    uninstall_cmd.GetProgram(),
                                    *chrome_frame_product, remove_all,
                                    force_uninstall, cmd_line);

    VLOG(1) << "Uninstallation of Chrome Frame returned status "
            << uninstall_status;
  } else {
    LOG(ERROR) << "Chrome Frame not found for uninstall.";
  }

  // Refresh state for the continuation of the original install/update.
  original_state->Initialize();
  installer_state->Initialize(cmd_line, prefs, *original_state);
}

}  // namespace

namespace installer {

InstallStatus InstallProductsHelper(const InstallationState& original_state,
                                    const base::FilePath& setup_exe,
                                    const base::CommandLine& cmd_line,
                                    const MasterPreferences& prefs,
                                    const InstallerState& installer_state,
                                    base::FilePath* installer_directory,
                                    ArchiveType* archive_type) {
  DCHECK(archive_type);
  const bool system_install = installer_state.system_install();
  InstallStatus install_status = UNKNOWN_STATUS;

  // Create a temp folder where we will unpack Chrome archive. If it fails,
  // then we are doomed, so return immediately and no cleanup is required.
  SelfCleaningTempDir temp_path;
  base::FilePath unpack_path;
  if (!CreateTemporaryAndUnpackDirectories(installer_state, &temp_path,
                                           &unpack_path)) {
    installer_state.WriteInstallerResult(TEMP_DIR_FAILED,
                                         IDS_INSTALL_TEMP_DIR_FAILED_BASE,
                                         NULL);
    return TEMP_DIR_FAILED;
  }

  // Uncompress and optionally patch the archive if an uncompressed archive was
  // not specified on the command line and a compressed archive is found.
  *archive_type = UNKNOWN_ARCHIVE_TYPE;
  base::FilePath uncompressed_archive(cmd_line.GetSwitchValuePath(
      switches::kUncompressedArchive));
  if (uncompressed_archive.empty()) {
    base::Version previous_version;
    if (cmd_line.HasSwitch(installer::switches::kPreviousVersion)) {
      previous_version = base::Version(cmd_line.GetSwitchValueASCII(
          installer::switches::kPreviousVersion));
    }

    std::unique_ptr<ArchivePatchHelper> archive_helper(
        CreateChromeArchiveHelper(setup_exe, cmd_line, installer_state,
                                  unpack_path));
    if (archive_helper) {
      VLOG(1) << "Installing Chrome from compressed archive "
              << archive_helper->compressed_archive().value();
      if (!UncompressAndPatchChromeArchive(original_state,
                                           installer_state,
                                           archive_helper.get(),
                                           archive_type,
                                           &install_status,
                                           previous_version)) {
        DCHECK_NE(install_status, UNKNOWN_STATUS);
        return install_status;
      }
      uncompressed_archive = archive_helper->target();
      DCHECK(!uncompressed_archive.empty());
    }
  }

  // Check for an uncompressed archive alongside the current executable if one
  // was not given or generated.
  if (uncompressed_archive.empty())
    uncompressed_archive = setup_exe.DirName().Append(kChromeArchive);

  if (*archive_type == UNKNOWN_ARCHIVE_TYPE) {
    // An archive was not uncompressed or patched above.
    if (uncompressed_archive.empty() ||
        !base::PathExists(uncompressed_archive)) {
      LOG(ERROR) << "Cannot install Chrome without an uncompressed archive.";
      installer_state.WriteInstallerResult(
          INVALID_ARCHIVE, IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
      return INVALID_ARCHIVE;
    }
    *archive_type = FULL_ARCHIVE_TYPE;
  }

  // Unpack the uncompressed archive.
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (LzmaUtil::UnPackArchive(uncompressed_archive.value(),
                              unpack_path.value(),
                              NULL)) {
    installer_state.WriteInstallerResult(
        UNPACKING_FAILED,
        IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
        NULL);
    return UNPACKING_FAILED;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  if (installer_state.is_background_mode()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Setup.Install.UnpackFullArchiveTime.background", elapsed_time);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Setup.Install.UnpackFullArchiveTime", elapsed_time);
  }

  VLOG(1) << "unpacked to " << unpack_path.value();
  base::FilePath src_path(
      unpack_path.Append(kInstallSourceChromeDir));
  std::unique_ptr<Version> installer_version(
      GetMaxVersionFromArchiveDir(src_path));
  if (!installer_version.get()) {
    LOG(ERROR) << "Did not find any valid version in installer.";
    install_status = INVALID_ARCHIVE;
    installer_state.WriteInstallerResult(install_status,
        IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
  } else {
    VLOG(1) << "version to install: " << installer_version->GetString();
    bool proceed_with_installation = true;

    if (!IsDowngradeAllowed(prefs)) {
      uint32_t higher_products = 0;
      static_assert(
          sizeof(higher_products) * 8 > BrowserDistribution::NUM_TYPES,
          "too many distribution types");
      const Products& products = installer_state.products();
      for (Products::const_iterator it = products.begin(); it < products.end();
           ++it) {
        const Product& product = **it;
        const ProductState* product_state = original_state.GetProductState(
            system_install, product.distribution()->GetType());
        if (product_state != NULL &&
            (product_state->version().CompareTo(*installer_version) > 0)) {
          LOG(ERROR) << "Higher version of "
                     << product.distribution()->GetDisplayName()
                     << " is already installed.";
          higher_products |= (1 << product.distribution()->GetType());
        }
      }

      if (higher_products != 0) {
        static_assert(BrowserDistribution::NUM_TYPES == 3,
                      "add support for new products here");
        int message_id = IDS_INSTALL_HIGHER_VERSION_BASE;
        proceed_with_installation = false;
        install_status = HIGHER_VERSION_EXISTS;
        installer_state.WriteInstallerResult(install_status, message_id, NULL);
      }
    }

    if (proceed_with_installation) {
      base::FilePath prefs_source_path(cmd_line.GetSwitchValueNative(
          switches::kInstallerData));
      install_status = InstallOrUpdateProduct(
          original_state, installer_state, setup_exe, uncompressed_archive,
          temp_path.path(), src_path, prefs_source_path, prefs,
          *installer_version);

      int install_msg_base = IDS_INSTALL_FAILED_BASE;
      base::FilePath chrome_exe;
      base::string16 quoted_chrome_exe;
      if (install_status == SAME_VERSION_REPAIR_FAILED) {
        install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_BASE;
      } else if (install_status != INSTALL_FAILED) {
        if (installer_state.target_path().empty()) {
          // If we failed to construct install path, it means the OS call to
          // get %ProgramFiles% or %AppData% failed. Report this as failure.
          install_msg_base = IDS_INSTALL_OS_ERROR_BASE;
          install_status = OS_ERROR;
        } else {
          chrome_exe = installer_state.target_path().Append(kChromeExe);
          quoted_chrome_exe = L"\"" + chrome_exe.value() + L"\"";
          install_msg_base = 0;
        }
      }

      installer_state.UpdateStage(FINISHING);

      // Only do Chrome-specific stuff (like launching the browser) if
      // Chrome was specifically requested (rather than being upgraded as
      // part of a multi-install).
      const Product* chrome_install = prefs.install_chrome() ?
          installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER) :
          NULL;

      bool do_not_register_for_update_launch = false;
      if (chrome_install) {
        prefs.GetBool(master_preferences::kDoNotRegisterForUpdateLaunch,
                      &do_not_register_for_update_launch);
      } else {
        do_not_register_for_update_launch = true;  // Never register.
      }

      bool write_chrome_launch_string =
          (!do_not_register_for_update_launch &&
           install_status != IN_USE_UPDATED);

      installer_state.WriteInstallerResult(install_status, install_msg_base,
          write_chrome_launch_string ? &quoted_chrome_exe : NULL);

      if (install_status == FIRST_INSTALL_SUCCESS) {
        VLOG(1) << "First install successful.";
        if (chrome_install) {
          // We never want to launch Chrome in system level install mode.
          bool do_not_launch_chrome = false;
          prefs.GetBool(master_preferences::kDoNotLaunchChrome,
                        &do_not_launch_chrome);
          if (!system_install && !do_not_launch_chrome)
            chrome_install->LaunchChrome(installer_state.target_path());
        }
      } else if ((install_status == NEW_VERSION_UPDATED) ||
                 (install_status == IN_USE_UPDATED)) {
        const Product* chrome = installer_state.FindProduct(
            BrowserDistribution::CHROME_BROWSER);
        if (chrome != NULL) {
          DCHECK_NE(chrome_exe.value(), base::string16());
          RemoveChromeLegacyRegistryKeys(chrome->distribution(), chrome_exe);
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
    base::FilePath setup_path(setup_exe);
    if (InstallUtil::GetInstallReturnCode(install_status) == 0) {
      setup_path = installer_state.GetInstallerDirectory(*installer_version)
          .Append(setup_path.BaseName());
    }
    const Products& products = installer_state.products();
    for (Products::const_iterator it = products.begin(); it < products.end();
         ++it) {
      const Product& product = **it;
      product.LaunchUserExperiment(setup_path, install_status, system_install);
    }
  }

  // If the installation completed successfully...
  if (InstallUtil::GetInstallReturnCode(install_status) == 0) {
    // Update the DisplayVersion created by an MSI-based install.
    base::FilePath master_preferences_file(
      installer_state.target_path().AppendASCII(
          installer::kDefaultMasterPrefs));
    std::string install_id;
    if (prefs.GetString(installer::master_preferences::kMsiProductId,
                        &install_id)) {
      // A currently active MSI install will have specified the master-
      // preferences file on the command-line that includes the product-id.
      // We must delay the setting of the DisplayVersion until after the
      // grandparent "msiexec" process has exited.
      base::FilePath new_setup =
          installer_state.GetInstallerDirectory(*installer_version)
          .Append(kSetupExe);
      DelayedOverwriteDisplayVersions(
          new_setup, install_id, *installer_version);
    } else {
      // Only when called by the MSI installer do we need to delay setting
      // the DisplayVersion.  In other runs, such as those done by the auto-
      // update action, we set the value immediately.
      const Product* chrome = installer_state.FindProduct(
          BrowserDistribution::CHROME_BROWSER);
      if (chrome != NULL) {
        // Get the app's MSI Product-ID from an entry in ClientState.
        base::string16 app_guid = FindMsiProductId(installer_state, chrome);
        if (!app_guid.empty()) {
          OverwriteDisplayVersions(app_guid,
                                   base::UTF8ToUTF16(
                                       installer_version->GetString()));
        }
      }
    }
    // Return the path to the directory containing the newly installed
    // setup.exe and uncompressed archive if the caller requested it.
    if (installer_directory) {
      *installer_directory =
          installer_state.GetInstallerDirectory(*installer_version);
    }
  }

  // temp_path's dtor will take care of deleting or scheduling itself for
  // deletion at reboot when this scope closes.
  VLOG(1) << "Deleting temporary directory " << temp_path.path().value();

  return install_status;
}

}  // namespace installer

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* command_line, int show_command) {
  // Check to see if the CPU is supported before doing anything else. There's
  // very little than can safely be accomplished if the CPU isn't supported
  // since dependent libraries (e.g., base) may use invalid instructions.
  if (!installer::IsProcessorSupported())
    return installer::CPU_NOT_SUPPORTED;

  // Persist histograms so they can be uploaded later.
  installer::BeginPersistentHistogramStorage();

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, NULL);

  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    return crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess());
  }

  // install_util uses chrome paths.
  chrome::RegisterPathProvider();

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  installer::InitInstallerLogging(prefs);

  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  VLOG(1) << "Command Line: " << cmd_line.GetCommandLineString();

  VLOG(1) << "multi install is " << prefs.is_multi_install();
  bool system_install = false;
  prefs.GetBool(installer::master_preferences::kSystemLevel, &system_install);
  VLOG(1) << "system install is " << system_install;

  InstallationState original_state;
  original_state.Initialize();

  InstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, original_state);

  installer::ConfigureCrashReporting(installer_state);
  installer::SetInitialCrashKeys(installer_state);
  installer::SetCrashKeysFromCommandLine(cmd_line);

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(cmd_line);

  const bool is_uninstall = cmd_line.HasSwitch(installer::switches::kUninstall);

  // Check to make sure current system is Win7 or later. If not, log
  // error message and get out.
  if (!InstallUtil::IsOSSupported()) {
    LOG(ERROR) << "Chrome only supports Windows 7 or later.";
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
        cmd_line.HasSwitch(installer::switches::kSelfDestruct) ||
        cmd_line.HasSwitch(installer::switches::kMakeChromeDefault) ||
        cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser) ||
        cmd_line.HasSwitch(installer::switches::kRemoveChromeRegistration) ||
        cmd_line.HasSwitch(installer::switches::kInactiveUserToast) ||
        cmd_line.HasSwitch(installer::switches::kSystemLevelToast)) {
      return installer::SXS_OPTION_NOT_SUPPORTED;
    }
  }

  // Some command line options are no longer supported and must error out.
  if (installer::ContainsUnsupportedSwitch(cmd_line))
    return installer::UNSUPPORTED_OPTION;

  // A variety of installer operations require the path to the current
  // executable. Get it once here for use throughout these operations. Note that
  // the path service is the authoritative source for this path. One might think
  // that CommandLine::GetProgram would suffice, but it won't since
  // CreateProcess may have been called with a command line that is somewhat
  // ambiguous (e.g., an unquoted path with spaces, or a path lacking the file
  // extension), in which case CommandLineToArgv will not yield an argv with the
  // true path to the program at position 0.
  base::FilePath setup_exe;
  PathService::Get(base::FILE_EXE, &setup_exe);

  int exit_code = 0;
  if (HandleNonInstallCmdLineOptions(
          original_state, setup_exe, cmd_line, &installer_state, &exit_code)) {
    return exit_code;
  }

  if (system_install && !IsUserAnAdmin()) {
    if (base::win::GetVersion() >= base::win::VERSION_VISTA &&
        !cmd_line.HasSwitch(installer::switches::kRunAsAdmin)) {
      base::CommandLine new_cmd(base::CommandLine::NO_PROGRAM);
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

  UninstallMultiChromeFrameIfPresent(cmd_line, prefs,
                                     &original_state, &installer_state);

  base::FilePath installer_directory;
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  // If --uninstall option is given, uninstall the identified product(s)
  if (is_uninstall) {
    install_status =
        UninstallProducts(original_state, installer_state, setup_exe, cmd_line);
  } else {
    // If --uninstall option is not specified, we assume it is install case.
    install_status =
        InstallProducts(original_state, setup_exe, cmd_line, prefs,
                        &installer_state, &installer_directory);
  }

  // Validate that the machine is now in a good state following the operation.
  // TODO(grt): change this to log at DFATAL once we're convinced that the
  // validator handles all cases properly.
  InstallationValidator::InstallationType installation_type =
      InstallationValidator::NO_PRODUCTS;
  LOG_IF(ERROR,
         !InstallationValidator::ValidateInstallationType(system_install,
                                                          &installation_type));

  UMA_HISTOGRAM_ENUMERATION("Setup.Install.Result", install_status,
                            installer::MAX_INSTALL_STATUS);

  int return_code = 0;
  // MSI demands that custom actions always return 0 (ERROR_SUCCESS) or it will
  // rollback the action. If we're uninstalling we want to avoid this, so always
  // report success, squashing any more informative return codes.
  if (!(installer_state.is_msi() && is_uninstall)) {
    // Note that we allow the status installer::UNINSTALL_REQUIRES_REBOOT
    // to pass through, since this is only returned on uninstall which is
    // never invoked directly by Google Update.
    return_code = InstallUtil::GetInstallReturnCode(install_status);
  }

  installer::EndPersistentHistogramStorage(installer_state.target_path());
  VLOG(1) << "Installation complete, returning: " << return_code;

  return return_code;
}
