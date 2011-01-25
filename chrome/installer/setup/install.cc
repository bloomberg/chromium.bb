// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install.h"

#include <shlobj.h>
#include <time.h>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

// Build-time generated include file.
#include "installer_util_strings.h"  // NOLINT
#include "registered_dlls.h"  // NOLINT

using installer::InstallerState;
using installer::InstallationState;
using installer::Product;

namespace {

void AddChromeToMediaPlayerList() {
  std::wstring reg_path(installer::kMediaPlayerRegPath);
  // registry paths can also be appended like file system path
  file_util::AppendToPath(&reg_path, installer::kChromeExe);
  VLOG(1) << "Adding Chrome to Media player list at " << reg_path;
  scoped_ptr<WorkItem> work_item(WorkItem::CreateCreateRegKeyWorkItem(
      HKEY_LOCAL_MACHINE, reg_path));

  // if the operation fails we log the error but still continue
  if (!work_item.get()->Do())
    LOG(ERROR) << "Could not add Chrome to media player inclusion list.";
}

void AddInstallerCopyTasks(const InstallerState& installer_state,
                           const FilePath& setup_path,
                           const FilePath& archive_path,
                           const FilePath& temp_path,
                           const Version& new_version,
                           WorkItemList* install_list) {
  DCHECK(install_list);
  FilePath installer_dir(installer_state.GetInstallerDirectory(new_version));
  install_list->AddCreateDirWorkItem(installer_dir);

  FilePath exe_dst(installer_dir.Append(setup_path.BaseName()));
  FilePath archive_dst(installer_dir.Append(archive_path.BaseName()));

  install_list->AddCopyTreeWorkItem(setup_path.value(), exe_dst.value(),
                                    temp_path.value(), WorkItem::ALWAYS);
  if (installer_state.system_install()) {
    install_list->AddCopyTreeWorkItem(archive_path.value(), archive_dst.value(),
                                      temp_path.value(), WorkItem::ALWAYS);
  } else {
    install_list->AddMoveTreeWorkItem(archive_path.value(), archive_dst.value(),
                                      temp_path.value());
  }
}

// Copy master preferences file provided to installer, in the same folder
// as chrome.exe so Chrome first run can find it. This function will be called
// only on the first install of Chrome.
void CopyPreferenceFileForFirstRun(const InstallerState& installer_state,
                                   const FilePath& prefs_source_path) {
  FilePath prefs_dest_path(installer_state.target_path().AppendASCII(
      installer::kDefaultMasterPrefs));
  if (!file_util::CopyFile(prefs_source_path, prefs_dest_path)) {
    VLOG(1) << "Failed to copy master preferences from:"
            << prefs_source_path.value() << " gle: " << ::GetLastError();
  }
}

// This method creates Chrome shortcuts in Start->Programs for all users or
// only for current user depending on whether it is system wide install or
// user only install.
//
// If first_install is true, it creates shortcuts for launching and
// uninstalling chrome.
// If first_install is false, the function only updates the shortcut for
// uninstalling chrome. According to
// http://blogs.msdn.com/oldnewthing/archive/2005/11/24/496690.aspx,
// updating uninstall shortcut should not trigger Windows "new application
// installed" notification.
//
// If the shortcuts do not exist, the function does not recreate them during
// update.
bool CreateOrUpdateChromeShortcuts(const InstallerState& installer_state,
                                   const FilePath& setup_path,
                                   const Version& new_version,
                                   installer::InstallStatus install_status,
                                   const Product& product,
                                   bool create_all_shortcut,
                                   bool alt_shortcut) {
  // TODO(tommi): Change this function to use WorkItemList.
  DCHECK(product.is_chrome());

  FilePath shortcut_path;
  int dir_enum = installer_state.system_install() ?
      base::DIR_COMMON_START_MENU : base::DIR_START_MENU;
  if (!PathService::Get(dir_enum, &shortcut_path)) {
    LOG(ERROR) << "Failed to get location for shortcut.";
    return false;
  }

  BrowserDistribution* browser_dist = product.distribution();

  // The location of Start->Programs->Google Chrome folder
  const std::wstring product_name(browser_dist->GetAppShortCutName());
  const std::wstring product_desc(browser_dist->GetAppDescription());
  shortcut_path = shortcut_path.Append(product_name);

  // Create/update Chrome link (points to chrome.exe) & Uninstall Chrome link
  // (which points to setup.exe) under this folder only if:
  // - This is a new install or install repair
  // OR
  // - The shortcut already exists in case of updates (user may have deleted
  //   shortcuts since our install. So on updates we only update if shortcut
  //   already exists)
  bool ret = true;
  FilePath chrome_link(shortcut_path);  // Chrome link (launches Chrome)
  chrome_link = chrome_link.Append(product_name + L".lnk");
  // Chrome link target
  FilePath chrome_exe(
      installer_state.target_path().Append(installer::kChromeExe));

  if ((install_status == installer::FIRST_INSTALL_SUCCESS) ||
      (install_status == installer::INSTALL_REPAIRED)) {
    if (!file_util::PathExists(shortcut_path))
      file_util::CreateDirectoryW(shortcut_path);

    VLOG(1) << "Creating shortcut to " << chrome_exe.value() << " at "
            << chrome_link.value();
    ret = ShellUtil::UpdateChromeShortcut(browser_dist, chrome_exe.value(),
        chrome_link.value(), product_desc, true);
  } else if (file_util::PathExists(chrome_link)) {
    VLOG(1) << "Updating shortcut at " << chrome_link.value()
            << " to point to " << chrome_exe.value();
    ret = ShellUtil::UpdateChromeShortcut(browser_dist, chrome_exe.value(),
        chrome_link.value(), product_desc, false);
  } else {
    VLOG(1)
        << "not first or repaired install, link file doesn't exist. status: "
        << install_status;
  }

  // Create/update uninstall link if we are not an MSI install. MSI
  // installations are, for the time being, managed only through the
  // Add/Remove Programs dialog.
  // TODO(robertshield): We could add a shortcut to msiexec /X {GUID} here.
  if (ret && !installer_state.is_msi()) {
    FilePath uninstall_link(shortcut_path);  // Uninstall Chrome link
    uninstall_link = uninstall_link.Append(
        browser_dist->GetUninstallLinkName() + L".lnk");
    if ((install_status == installer::FIRST_INSTALL_SUCCESS) ||
        (install_status == installer::INSTALL_REPAIRED) ||
        (file_util::PathExists(uninstall_link))) {
      if (!file_util::PathExists(shortcut_path))
        file_util::CreateDirectory(shortcut_path);

      FilePath setup_exe(installer_state.GetInstallerDirectory(new_version)
          .Append(setup_path.BaseName()));

      CommandLine arguments(CommandLine::NO_PROGRAM);
      AppendUninstallCommandLineFlags(installer_state, product, &arguments);
      VLOG(1) << "Creating/updating uninstall link at "
              << uninstall_link.value();
      ret = file_util::CreateShortcutLink(setup_exe.value().c_str(),
          uninstall_link.value().c_str(),
          NULL,
          arguments.command_line_string().c_str(),
          NULL,
          setup_exe.value().c_str(),
          0,
          NULL);
    }
  }

  // Update Desktop and Quick Launch shortcuts. If --create-new-shortcuts
  // is specified we want to create them, otherwise we update them only if
  // they exist.
  if (ret) {
    if (installer_state.system_install()) {
      ret = ShellUtil::CreateChromeDesktopShortcut(product.distribution(),
          chrome_exe.value(), product_desc, ShellUtil::SYSTEM_LEVEL,
          alt_shortcut, create_all_shortcut);
      if (ret) {
        ret = ShellUtil::CreateChromeQuickLaunchShortcut(
            product.distribution(), chrome_exe.value(),
            ShellUtil::CURRENT_USER | ShellUtil::SYSTEM_LEVEL,
            create_all_shortcut);
      }
    } else {
      ret = ShellUtil::CreateChromeDesktopShortcut(product.distribution(),
          chrome_exe.value(), product_desc, ShellUtil::CURRENT_USER,
          alt_shortcut, create_all_shortcut);
      if (ret) {
        ret = ShellUtil::CreateChromeQuickLaunchShortcut(
            product.distribution(), chrome_exe.value(), ShellUtil::CURRENT_USER,
            create_all_shortcut);
      }
    }
  }

  return ret;
}

void RegisterChromeOnMachine(const InstallerState& installer_state,
                             const Product& product,
                             bool make_chrome_default) {
  DCHECK(product.is_chrome());

  // Try to add Chrome to Media Player shim inclusion list. We don't do any
  // error checking here because this operation will fail if user doesn't
  // have admin rights and we want to ignore the error.
  AddChromeToMediaPlayerList();

  // Is --make-chrome-default option is given we make Chrome default browser
  // otherwise we only register it on the machine as a valid browser.
  FilePath chrome_exe(
      installer_state.target_path().Append(installer::kChromeExe));
  VLOG(1) << "Registering Chrome as browser: " << chrome_exe.value();
  if (make_chrome_default) {
    int level = ShellUtil::CURRENT_USER;
    if (installer_state.system_install())
      level = level | ShellUtil::SYSTEM_LEVEL;
    ShellUtil::MakeChromeDefault(product.distribution(), level,
                                 chrome_exe.value(), true);
  } else {
    ShellUtil::RegisterChromeBrowser(product.distribution(), chrome_exe.value(),
                                     L"", false);
  }
}

// This function installs a new version of Chrome to the specified location.
//
// setup_path: Path to the executable (setup.exe) as it will be copied
//           to Chrome install folder after install is complete
// archive_path: Path to the archive (chrome.7z) as it will be copied
//               to Chrome install folder after install is complete
// src_path: the path that contains a complete and unpacked Chrome package
//           to be installed.
// temp_dir: the path of working directory used during installation. This path
//           does not need to exist.
// new_version: new Chrome version that needs to be installed
// current_version: returns the current active version (if any)
//
// This function makes best effort to do installation in a transactional
// manner. If failed it tries to rollback all changes on the file system
// and registry. For example, if package exists before calling the
// function, it rolls back all new file and directory changes under
// package. If package does not exist before calling the function
// (typical new install), the function creates package during install
// and removes the whole directory during rollback.
installer::InstallStatus InstallNewVersion(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const FilePath& setup_path,
    const FilePath& archive_path,
    const FilePath& src_path,
    const FilePath& temp_dir,
    const Version& new_version,
    scoped_ptr<Version>* current_version) {
  DCHECK(current_version);

  current_version->reset(installer_state.GetCurrentVersion(original_state));
  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());

  AddInstallWorkItems(original_state,
                      installer_state,
                      setup_path,
                      archive_path,
                      src_path,
                      temp_dir,
                      new_version,
                      current_version,
                      install_list.get());

  FilePath new_chrome_exe(
      installer_state.target_path().Append(installer::kChromeNewExe));

  if (!install_list->Do()) {
    installer::InstallStatus result =
        file_util::PathExists(new_chrome_exe) && current_version->get() &&
        new_version.Equals(*current_version->get()) ?
        installer::SAME_VERSION_REPAIR_FAILED :
        installer::INSTALL_FAILED;
    LOG(ERROR) << "Install failed, rolling back... result: " << result;
    install_list->Rollback();
    LOG(ERROR) << "Rollback complete. ";
    return result;
  }

  if (!current_version->get()) {
    VLOG(1) << "First install of version " << new_version.GetString();
    return installer::FIRST_INSTALL_SUCCESS;
  }

  if (new_version.Equals(**current_version)) {
    VLOG(1) << "Install repaired of version " << new_version.GetString();
    return installer::INSTALL_REPAIRED;
  }

  if (new_version.CompareTo(**current_version) > 0) {
    if (file_util::PathExists(new_chrome_exe)) {
      VLOG(1) << "Version updated to " << new_version.GetString()
              << " while running " << (*current_version)->GetString();
      return installer::IN_USE_UPDATED;
    }
    VLOG(1) << "Version updated to " << new_version.GetString();
    return installer::NEW_VERSION_UPDATED;
  }

  LOG(ERROR) << "Not sure how we got here while updating"
             << ", new version: " << new_version.GetString()
             << ", old version: " << (*current_version)->GetString();

  return installer::INSTALL_FAILED;
}

}  // end namespace

namespace installer {

InstallStatus InstallOrUpdateProduct(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const FilePath& setup_path,
    const FilePath& archive_path,
    const FilePath& install_temp_path,
    const FilePath& prefs_path,
    const MasterPreferences& prefs,
    const Version& new_version) {
  FilePath src_path(install_temp_path);
  src_path = src_path.Append(kInstallSourceDir).Append(kInstallSourceChromeDir);

  // TODO(robertshield): Removing the pending on-reboot moves should be done
  // elsewhere.
  const Products& products = installer_state.products();
  DCHECK(products.size());
  if (installer_state.FindProduct(BrowserDistribution::CHROME_FRAME)) {
    // Make sure that we don't end up deleting installed files on next reboot.
    if (!RemoveFromMovesPendingReboot(
            installer_state.target_path().value().c_str())) {
      LOG(ERROR) << "Error accessing pending moves value.";
    }
  }

  scoped_ptr<Version> existing_version;
  InstallStatus result = InstallNewVersion(original_state, installer_state,
      setup_path, archive_path, src_path, install_temp_path, new_version,
      &existing_version);

  // TODO(robertshield): Everything below this line should instead be captured
  // by WorkItems.
  if (!InstallUtil::GetInstallReturnCode(result)) {
    if (result == FIRST_INSTALL_SUCCESS && !prefs_path.empty())
      CopyPreferenceFileForFirstRun(installer_state, prefs_path);

    bool do_not_create_shortcuts = false;
    prefs.GetBool(master_preferences::kDoNotCreateShortcuts,
                  &do_not_create_shortcuts);

    // Currently this only creates shortcuts for Chrome, but for other products
    // we might want to create shortcuts.
    const Product* chrome_install =
        installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER);
    if (chrome_install && !do_not_create_shortcuts) {
      bool create_all_shortcut = false;
      prefs.GetBool(master_preferences::kCreateAllShortcuts,
                    &create_all_shortcut);
      bool alt_shortcut = false;
      prefs.GetBool(master_preferences::kAltShortcutText, &alt_shortcut);
      if (!CreateOrUpdateChromeShortcuts(installer_state, setup_path,
                                         new_version, result, *chrome_install,
                                         create_all_shortcut, alt_shortcut)) {
        PLOG(WARNING) << "Failed to create/update start menu shortcut.";
      }

      bool make_chrome_default = false;
      prefs.GetBool(master_preferences::kMakeChromeDefault,
                    &make_chrome_default);

      // If this is not the user's first Chrome install, but they have chosen
      // Chrome to become their default browser on the download page, we must
      // force it here because the master_preferences file will not get copied
      // into the build.
      bool force_chrome_default_for_user = false;
      if (result == NEW_VERSION_UPDATED ||
          result == INSTALL_REPAIRED) {
        prefs.GetBool(master_preferences::kMakeChromeDefaultForUser,
                      &force_chrome_default_for_user);
      }

      RegisterChromeOnMachine(installer_state, *chrome_install,
          make_chrome_default || force_chrome_default_for_user);
    }

    installer_state.RemoveOldVersionDirectories(existing_version.get() ?
        *existing_version.get() : new_version);
  }

  return result;
}

}  // namespace installer
