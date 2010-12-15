// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_handle_win.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/package_properties.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"

#include "installer_util_strings.h"  // NOLINT

using installer::Product;
using installer::ProductPackageMapping;
using installer::Products;
using installer::Package;
using installer::Packages;
using installer::Version;
using installer::MasterPreferences;

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
                    const Package& installation,
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
      installer::GetVersionFromArchiveDir(installation.path()));

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

    FilePath existing_archive(
        installation.path().Append(archive_version->GetString()));
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
    const Package& installation) {
  FilePath chrome_exe(installation.path().Append(installer::kChromeExe));
  FilePath chrome_old_exe(installation.path().Append(
      installer::kChromeOldExe));
  FilePath chrome_new_exe(installation.path().Append(
      installer::kChromeNewExe));

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

  HKEY reg_root = installation.system_level() ? HKEY_LOCAL_MACHINE :
                                                HKEY_CURRENT_USER;
  const Products& products = installation.products();
  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];
    BrowserDistribution* browser_dist = product->distribution();
    std::wstring version_key(browser_dist->GetVersionKey());
    install_list->AddDeleteRegValueWorkItem(reg_root,
                                            version_key,
                                            google_update::kRegOldVersionField,
                                            true);
    install_list->AddDeleteRegValueWorkItem(reg_root,
                                            version_key,
                                            google_update::kRegRenameCmdField,
                                            true);
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

bool CheckPreInstallConditions(const Package& installation,
                               installer::InstallStatus& status) {
  const Products& products = installation.products();
  DCHECK(products.size());

  bool is_first_install = true;
  bool system_level = installation.system_level();

  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];
    BrowserDistribution* browser_dist = product->distribution();
    scoped_ptr<Version> installed_version(product->GetInstalledVersion());
    // TODO(tommi): If the current install is for a different distribution than
    // that might already be installed, then this check is incorrect.
    if (installed_version.get())
      is_first_install = false;

    // Check to avoid simultaneous per-user and per-machine installs.
    scoped_ptr<Version> chrome_version(
        InstallUtil::GetChromeVersion(browser_dist, !system_level));

    if (chrome_version.get()) {
      LOG(ERROR) << "Already installed version " << chrome_version->GetString()
                 << " conflicts with the current install mode.";
      if (!system_level && is_first_install) {
        // This is user-level install and there is a system-level chrome
        // installation. Instruct Omaha to launch the existing one. There
        // should be no error dialog.
        FilePath chrome_exe(installer::GetChromeInstallPath(!system_level,
                                                            browser_dist));
        if (chrome_exe.empty()) {
          // If we failed to construct install path. Give up.
          status = installer::OS_ERROR;
          product->WriteInstallerResult(status, IDS_INSTALL_OS_ERROR_BASE,
                                        NULL);
        } else {
          status = installer::EXISTING_VERSION_LAUNCHED;
          chrome_exe = chrome_exe.Append(installer::kChromeExe);
          CommandLine cmd(chrome_exe);
          cmd.AppendSwitch(switches::kFirstRun);
          product->WriteInstallerResult(status, 0, NULL);
          VLOG(1) << "Launching existing system-level chrome instead.";
          base::LaunchApp(cmd, false, false, NULL);
        }
        return false;
      }

      // If the following compile assert fires it means that the InstallStatus
      // enumeration changed which will break the contract between the old
      // chrome installed and the new setup.exe that is trying to upgrade.
      COMPILE_ASSERT(installer::SXS_OPTION_NOT_SUPPORTED == 33,
                     dont_change_enum);

      // This is an update, not an install. Omaha should know the difference
      // and not show a dialog.
      status = system_level ? installer::USER_LEVEL_INSTALL_EXISTS :
                              installer::SYSTEM_LEVEL_INSTALL_EXISTS;
      int str_id = system_level ? IDS_INSTALL_USER_LEVEL_EXISTS_BASE :
                                  IDS_INSTALL_SYSTEM_LEVEL_EXISTS_BASE;
      product->WriteInstallerResult(status, str_id, NULL);
      return false;
    }
  }

  // If no previous installation of Chrome, make sure installation directory
  // either does not exist or can be deleted (i.e. is not locked by some other
  // process).
  if (is_first_install) {
    if (file_util::PathExists(installation.path()) &&
        !file_util::Delete(installation.path(), true)) {
      LOG(ERROR) << "Installation directory " << installation.path().value()
                 << " exists and can not be deleted.";
      status = installer::INSTALL_DIR_IN_USE;
      int str_id = IDS_INSTALL_DIR_IN_USE_BASE;
      WriteInstallerResult(products, status, str_id, NULL);
      return false;
    }
  }

  return true;
}

installer::InstallStatus InstallChrome(const CommandLine& cmd_line,
    const Package& installation, const MasterPreferences& prefs) {
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  if (!CheckPreInstallConditions(installation, install_status))
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
  bool system_level = installation.system_level();
  const Products& products = installation.products();

  // Create a temp folder where we will unpack Chrome archive. If it fails,
  // then we are doomed, so return immediately and no cleanup is required.
  FilePath temp_path;
  if (!file_util::CreateNewTempDirectory(L"chrome_", &temp_path)) {
    LOG(ERROR) << "Could not create temporary path.";
    WriteInstallerResult(products, installer::TEMP_DIR_FAILED,
        IDS_INSTALL_TEMP_DIR_FAILED_BASE, NULL);
    return installer::TEMP_DIR_FAILED;
  }
  VLOG(1) << "created path " << temp_path.value();

  FilePath unpack_path(temp_path.Append(installer::kInstallSourceDir));
  bool incremental_install = false;
  if (UnPackArchive(archive, installation, temp_path, unpack_path,
                    incremental_install)) {
    install_status = installer::UNCOMPRESSION_FAILED;
    WriteInstallerResult(products, install_status,
        IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, NULL);
  } else {
    VLOG(1) << "unpacked to " << unpack_path.value();
    FilePath src_path(unpack_path.Append(installer::kInstallSourceChromeDir));
    scoped_ptr<Version>
        installer_version(installer::GetVersionFromArchiveDir(src_path));
    if (!installer_version.get()) {
      LOG(ERROR) << "Did not find any valid version in installer.";
      install_status = installer::INVALID_ARCHIVE;
      WriteInstallerResult(products, install_status,
          IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
    } else {
      // TODO(tommi): Move towards having only a single version that is common
      // to all products.  Only the package should have a version since it
      // represents all the binaries.  When a single product is upgraded, all
      // currently installed product for the shared binary installation, should
      // (or rather must) be upgraded.
      VLOG(1) << "version to install: " << installer_version->GetString();
      bool higher_version_installed = false;
      for (size_t i = 0; i < installation.products().size(); ++i) {
        const Product* product = installation.products()[i];
        scoped_ptr<Version> v(product->GetInstalledVersion());
        if (v.get() && v->IsHigherThan(installer_version.get())) {
          LOG(ERROR) << "Higher version is already installed.";
          higher_version_installed = true;
          install_status = installer::HIGHER_VERSION_EXISTS;

          if (product->distribution()->GetType() !=
              BrowserDistribution::CHROME_BROWSER) {
            // TODO(robertshield): We should take the installer result text
            // strings from the Product.
            product->WriteInstallerResult(install_status,
                IDS_INSTALL_HIGHER_VERSION_BASE, NULL);
          } else {
            product->WriteInstallerResult(install_status,
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
        install_status = installer::InstallOrUpdateChrome(cmd_line.GetProgram(),
            archive_to_copy, temp_path, prefs_source_path, prefs,
            *installer_version, installation);

        int install_msg_base = IDS_INSTALL_FAILED_BASE;
        std::wstring chrome_exe;
        if (install_status == installer::SAME_VERSION_REPAIR_FAILED) {
          if (FindProduct(products, BrowserDistribution::CHROME_FRAME)) {
            install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_CF_BASE;
          } else {
            install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_BASE;
          }
        } else if (install_status != installer::INSTALL_FAILED) {
          if (installation.path().empty()) {
            // If we failed to construct install path, it means the OS call to
            // get %ProgramFiles% or %AppData% failed. Report this as failure.
            install_msg_base = IDS_INSTALL_OS_ERROR_BASE;
            install_status = installer::OS_ERROR;
          } else {
            chrome_exe = installation.path()
                .Append(installer::kChromeExe).value();
            chrome_exe = L"\"" + chrome_exe + L"\"";
            install_msg_base = 0;
          }
        }

        const Product* chrome_install =
            FindProduct(products, BrowserDistribution::CHROME_BROWSER);

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

        WriteInstallerResult(products, install_status,
            install_msg_base, write_chrome_launch_string ? &chrome_exe : NULL);

        if (install_status == installer::FIRST_INSTALL_SUCCESS) {
          VLOG(1) << "First install successful.";
          if (chrome_install) {
            // We never want to launch Chrome in system level install mode.
            bool do_not_launch_chrome = false;
            prefs.GetBool(
                installer::master_preferences::kDoNotLaunchChrome,
                &do_not_launch_chrome);
            if (!installation.system_level() && !do_not_launch_chrome)
              chrome_install->LaunchChrome();
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
          *installer_version, *product, installation.system_level());
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
    product->distribution()->UpdateDiffInstallStatus(system_level,
        incremental_install, install_status);
  }

  return install_status;
}

installer::InstallStatus UninstallChrome(const CommandLine& cmd_line,
                                              const Product& product) {
  VLOG(1) << "Uninstalling Chome";

  scoped_ptr<Version> installed_version(product.GetInstalledVersion());
  if (installed_version.get())
    VLOG(1) << "version on the system: " << installed_version->GetString();

  bool force = cmd_line.HasSwitch(installer::switches::kForceUninstall);
  if (!installed_version.get() && !force) {
    LOG(ERROR) << "No Chrome installation found for uninstall.";
    product.WriteInstallerResult(installer::CHROME_NOT_INSTALLED,
        IDS_UNINSTALL_FAILED_BASE, NULL);
    return installer::CHROME_NOT_INSTALLED;
  }

  bool remove_all = !cmd_line.HasSwitch(
      installer::switches::kDoNotRemoveSharedItems);

  return installer::UninstallChrome(cmd_line.GetProgram(), product, remove_all,
      force, cmd_line);
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
bool HandleNonInstallCmdLineOptions(const CommandLine& cmd_line,
                                    int& exit_code,
                                    const ProductPackageMapping& installs) {
  DCHECK(installs.products().size());
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

    exit_code = BrowserDistribution::GetInstallReturnCode(status);
    if (exit_code) {
      LOG(WARNING) << "setup.exe patching failed.";
      WriteInstallerResult(installs.products(), status,
          IDS_SETUP_PATCH_FAILED_BASE, NULL);
    }
    file_util::Delete(temp_path, true);
    return true;
  } else if (cmd_line.HasSwitch(installer::switches::kShowEula)) {
    // Check if we need to show the EULA. If it is passed as a command line
    // then the dialog is shown and regardless of the outcome setup exits here.
    std::wstring inner_frame =
        cmd_line.GetSwitchValueNative(installer::switches::kShowEula);
    exit_code = ShowEULADialog(inner_frame);
    if (installer::EULA_REJECTED != exit_code)
      GoogleUpdateSettings::SetEULAConsent(true);
    return true;
  } else if (cmd_line.HasSwitch(
      installer::switches::kRegisterChromeBrowser)) {
    const Product* chrome_install =
        FindProduct(installs.products(), BrowserDistribution::CHROME_BROWSER);
    DCHECK(chrome_install);
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
      exit_code = ShellUtil::RegisterChromeBrowser(
          chrome_install->distribution(), chrome_exe, suffix, false);
    } else {
      LOG(ERROR) << "Can't register browser - Chrome distribution not found";
      exit_code = installer::UNKNOWN_STATUS;
    }
    return true;
  } else if (cmd_line.HasSwitch(installer::switches::kRenameChromeExe)) {
    // If --rename-chrome-exe is specified, we want to rename the executables
    // and exit.
    const Packages& packages = installs.packages();
    DCHECK_EQ(1U, packages.size());
    for (size_t i = 0; i < packages.size(); ++i)
      exit_code = RenameChromeExecutables(*packages[i].get());
    return true;
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
        FindProduct(installs.products(), BrowserDistribution::CHROME_BROWSER);
    DCHECK(chrome_install);
    if (chrome_install) {
      installer::DeleteChromeRegistrationKeys(chrome_install->distribution(),
          HKEY_LOCAL_MACHINE, suffix, tmp);
    }
    exit_code = tmp;
    return true;
  } else if (cmd_line.HasSwitch(installer::switches::kInactiveUserToast)) {
    // Launch the inactive user toast experiment.
    int flavor = -1;
    base::StringToInt(cmd_line.GetSwitchValueNative(
        installer::switches::kInactiveUserToast), &flavor);
    DCHECK_NE(-1, flavor);
    if (flavor == -1) {
      exit_code = installer::UNKNOWN_STATUS;
    } else {
      const Products& products = installs.products();
      for (size_t i = 0; i < products.size(); ++i) {
        const Product* product = products[i];
        BrowserDistribution* browser_dist = product->distribution();
        browser_dist->InactiveUserToastExperiment(flavor, *product);
      }
    }
    return true;
  } else if (cmd_line.HasSwitch(installer::switches::kSystemLevelToast)) {
    const Products& products = installs.products();
    for (size_t i = 0; i < products.size(); ++i) {
      const Product* product = products[i];
      BrowserDistribution* browser_dist = product->distribution();
      // We started as system-level and have been re-launched as user level
      // to continue with the toast experiment.
      scoped_ptr<Version> installed_version(
          InstallUtil::GetChromeVersion(browser_dist, installs.system_level()));
      browser_dist->LaunchUserExperiment(installer::REENTRY_SYS_UPDATE,
                                         *installed_version, *product, true);
    }
    return true;
  }
  return false;
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
  ScopedHandle scoped_handle(token);

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

void PopulateInstallations(const MasterPreferences& prefs,
                           ProductPackageMapping* installations) {
  DCHECK(installations);
  if (prefs.install_chrome()) {
    VLOG(1) << "Install distribution: Chrome";
    installations->AddDistribution(BrowserDistribution::CHROME_BROWSER, prefs);
  }

  if (prefs.install_chrome_frame()) {
    VLOG(1) << "Install distribution: Chrome Frame";
    installations->AddDistribution(BrowserDistribution::CHROME_FRAME, prefs);
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* command_line, int show_command) {
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  CommandLine::Init(0, NULL);

  const MasterPreferences& prefs =
      installer::MasterPreferences::ForCurrentProcess();
  installer::InitInstallerLogging(prefs);

  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  VLOG(1) << "Command Line: " << cmd_line.command_line_string();

  bool system_install = false;
  prefs.GetBool(installer::master_preferences::kSystemLevel,
                &system_install);
  VLOG(1) << "system install is " << system_install;

  ProductPackageMapping installations(system_install);
  PopulateInstallations(prefs, &installations);

  // Check to make sure current system is WinXP or later. If not, log
  // error message and get out.
  if (!InstallUtil::IsOSSupported()) {
    LOG(ERROR) << "Chrome only supports Windows XP or later.";
    WriteInstallerResult(installations.products(),
        installer::OS_NOT_SUPPORTED, IDS_INSTALL_OS_NOT_SUPPORTED_BASE,
        NULL);
    return installer::OS_NOT_SUPPORTED;
  }

  // Initialize COM for use later.
  AutoCom auto_com;
  if (!auto_com.Init(system_install)) {
    WriteInstallerResult(installations.products(),
        installer::OS_ERROR, IDS_INSTALL_OS_ERROR_BASE, NULL);
    return installer::OS_ERROR;
  }

  // Some command line options don't work with SxS install/uninstall
  if (InstallUtil::IsChromeSxSProcess()) {
    if (system_install ||
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
  if (HandleNonInstallCmdLineOptions(cmd_line, exit_code, installations))
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
      WriteInstallerResult(installations.products(),
          installer::INSUFFICIENT_RIGHTS,
          IDS_INSTALL_INSUFFICIENT_RIGHTS_BASE, NULL);
      return installer::INSUFFICIENT_RIGHTS;
    }
  }

  bool is_uninstall = cmd_line.HasSwitch(installer::switches::kUninstall);

  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  // If --uninstall option is given, uninstall chrome
  if (is_uninstall) {
    DCHECK_EQ(1U, installations.products().size()) <<
        "We currently really only support uninstalling one distribution "
        "at a time";
    for (size_t i = 0; i < installations.products().size(); ++i) {
      install_status = UninstallChrome(cmd_line,
                                       *installations.products()[i]);
    }
  } else {
    // If --uninstall option is not specified, we assume it is install case.
    const Packages& packages = installations.packages();
    VLOG(1) << "Installing to " << packages.size() << " target paths";
    for (size_t i = 0; i < packages.size(); ++i) {
      install_status = InstallChrome(cmd_line, *packages[i].get(), prefs);
    }
  }

  const Product* cf_install =
      FindProduct(installations.products(), BrowserDistribution::CHROME_FRAME);

  if (cf_install &&
      !cmd_line.HasSwitch(installer::switches::kForceUninstall)) {
    if (install_status == installer::UNINSTALL_REQUIRES_REBOOT) {
      ShowRebootDialog();
    } else if (is_uninstall) {
      ::MessageBoxW(NULL,
                    installer::GetLocalizedString(
                        IDS_UNINSTALL_COMPLETE_BASE).c_str(),
                    cf_install->distribution()->GetApplicationName().c_str(),
                    MB_OK);
    }
  }

  int return_code = 0;
  // MSI demands that custom actions always return 0 (ERROR_SUCCESS) or it will
  // rollback the action. If we're uninstalling we want to avoid this, so always
  // report success, squashing any more informative return codes.
  // TODO(tommi): Fix this loop when IsMsi has been moved out of the Product
  // class.
  for (size_t i = 0; i < installations.products().size(); ++i) {
    const Product* product = installations.products()[i];
    if (!(product->IsMsi() && is_uninstall)) {
      // Note that we allow the status installer::UNINSTALL_REQUIRES_REBOOT
      // to pass through, since this is only returned on uninstall which is
      // never invoked directly by Google Update.
      return_code = BrowserDistribution::GetInstallReturnCode(install_status);
    }
  }

  VLOG(1) << "Installation complete, returning: " << return_code;

  return return_code;
}
