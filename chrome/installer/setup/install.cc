// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install.h"

#include <shlobj.h>
#include <time.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/util/auto_launch_util.h"
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
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/work_item_list.h"

// Build-time generated include file.
#include "registered_dlls.h"  // NOLINT

using installer::InstallerState;
using installer::InstallationState;
using installer::Product;

namespace {

void AddChromeToMediaPlayerList() {
  std::wstring reg_path(installer::kMediaPlayerRegPath);
  // registry paths can also be appended like file system path
  reg_path.push_back(FilePath::kSeparators[0]);
  reg_path.append(installer::kChromeExe);
  VLOG(1) << "Adding Chrome to Media player list at " << reg_path;
  scoped_ptr<WorkItem> work_item(WorkItem::CreateCreateRegKeyWorkItem(
      HKEY_LOCAL_MACHINE, reg_path));

  // if the operation fails we log the error but still continue
  if (!work_item.get()->Do())
    LOG(ERROR) << "Could not add Chrome to media player inclusion list.";
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
// |options|: bitfield for which the options come from ChromeShortcutOptions.
void CreateOrUpdateChromeShortcuts(const InstallerState& installer_state,
                                   const FilePath& setup_path,
                                   const Version& new_version,
                                   installer::InstallStatus install_status,
                                   const Product& product,
                                   uint32 options) {
  // TODO(tommi): Change this function to use WorkItemList.
  DCHECK(product.is_chrome());

  // Information used for all shortcut types
  BrowserDistribution* browser_dist = product.distribution();
  const std::wstring product_name(browser_dist->GetAppShortCutName());
  const std::wstring product_desc(browser_dist->GetAppDescription());
  // Chrome link target
  FilePath chrome_exe(
      installer_state.target_path().Append(installer::kChromeExe));

  // Create Start Menu shortcuts.
  // The location of Start->Programs->Google Chrome folder
  FilePath start_menu_folder_path;
  int dir_enum = installer_state.system_install() ?
      base::DIR_COMMON_START_MENU : base::DIR_START_MENU;
  if (!PathService::Get(dir_enum, &start_menu_folder_path)) {
    LOG(ERROR) << "Failed to get location for shortcut.";
  } else {
    start_menu_folder_path = start_menu_folder_path.Append(product_name);

    // Create/update Chrome link (points to chrome.exe) & Uninstall Chrome link
    // (which points to setup.exe) under this folder only if:
    // - This is a new install or install repair
    // OR
    // - The shortcut already exists in case of updates (user may have deleted
    //   shortcuts since our install. So on updates we only update if shortcut
    //   already exists)

    // Chrome link (launches Chrome)
    FilePath chrome_link(start_menu_folder_path.Append(product_name + L".lnk"));

    if ((install_status == installer::FIRST_INSTALL_SUCCESS) ||
        (install_status == installer::INSTALL_REPAIRED)) {
      if (!file_util::PathExists(start_menu_folder_path))
        file_util::CreateDirectoryW(start_menu_folder_path);

      VLOG(1) << "Creating shortcut to " << chrome_exe.value() << " at "
              << chrome_link.value();
      if (ShellUtil::UpdateChromeShortcut(browser_dist, chrome_exe.value(),
              chrome_link.value(), L"", product_desc, chrome_exe.value(),
              browser_dist->GetIconIndex(),
              ShellUtil::SHORTCUT_DUAL_MODE |
              ShellUtil::SHORTCUT_CREATE_ALWAYS)) {
        if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
          VLOG(1) << "Pinning new shortcut at " << chrome_link.value()
                  << " to taskbar";
          if (!file_util::TaskbarPinShortcutLink(chrome_link.value().c_str())) {
            LOG(ERROR) << "Failed to pin shortcut to taskbar: "
                       << chrome_link.value();
          }
        }
      } else {
        LOG(ERROR) << "Failed to create start menu shortcut at "
                   << chrome_link.value();
      }
    } else if (file_util::PathExists(chrome_link)) {
      VLOG(1) << "Updating shortcut at " << chrome_link.value()
              << " to point to " << chrome_exe.value();
      if (!ShellUtil::UpdateChromeShortcut(browser_dist, chrome_exe.value(),
              chrome_link.value(), L"", product_desc, chrome_exe.value(),
              browser_dist->GetIconIndex(), ShellUtil::SHORTCUT_DUAL_MODE)) {
        LOG(ERROR) << "Failed to update start menu shortcut at "
                   << chrome_link.value();
      }
    } else {
      VLOG(1) << "No Start Menu shortcut to update (status: " << install_status
              << ").";
    }

    // Create/update uninstall link if we are not an MSI install. MSI
    // installations are, for the time being, managed only through the
    // Add/Remove Programs dialog.
    // TODO(robertshield): We could add a shortcut to msiexec /X {GUID} here.
    if (!installer_state.is_msi()) {
      // Uninstall Chrome link
      FilePath uninstall_link(start_menu_folder_path.Append(
          browser_dist->GetUninstallLinkName() + L".lnk"));
      if ((install_status == installer::FIRST_INSTALL_SUCCESS) ||
          (install_status == installer::INSTALL_REPAIRED) ||
          (file_util::PathExists(uninstall_link))) {
        if (!file_util::PathExists(start_menu_folder_path))
          file_util::CreateDirectory(start_menu_folder_path);

        FilePath setup_exe(installer_state.GetInstallerDirectory(new_version)
            .Append(setup_path.BaseName()));

        CommandLine arguments(CommandLine::NO_PROGRAM);
        AppendUninstallCommandLineFlags(installer_state, product, &arguments);
        VLOG(1) << "Creating/updating uninstall link at "
                << uninstall_link.value();
        if (!file_util::CreateOrUpdateShortcutLink(setup_exe.value().c_str(),
                uninstall_link.value().c_str(), NULL,
                arguments.GetCommandLineString().c_str(), NULL,
                setup_exe.value().c_str(), 0, NULL,
                file_util::SHORTCUT_CREATE_ALWAYS)) {
          LOG(ERROR) << "Failed to create/update uninstall link in start menu "
                     << " at " << uninstall_link.value();
        }
      }
    }
  }

  // Update Desktop and Quick Launch shortcuts. If --create-all-shortcuts
  // is specified we want to create them, otherwise we update them only if
  // they exist.
  ShellUtil::ShellChange desktop_level = ShellUtil::CURRENT_USER;
  int quick_launch_levels = ShellUtil::CURRENT_USER;
  if (installer_state.system_install()) {
    desktop_level = ShellUtil::SYSTEM_LEVEL;
    quick_launch_levels |= ShellUtil::SYSTEM_LEVEL;
  }

  VLOG(1) << "Creating/updating desktop shortcut for " << chrome_exe.value()
          << " will create new: "
          << ((options & ShellUtil::SHORTCUT_CREATE_ALWAYS) != 0);
  if (!ShellUtil::CreateChromeDesktopShortcut(browser_dist,
          chrome_exe.value(), product_desc, L"", L"", chrome_exe.value(),
          browser_dist->GetIconIndex(), desktop_level, options)) {
    LOG(WARNING) << "Did not create/update desktop shortcut for "
                 << chrome_exe.value();
  }

  VLOG(1) << "Creating/updating quick launch shortcut for "
          << chrome_exe.value() << " will create new: "
          << ((options & ShellUtil::SHORTCUT_CREATE_ALWAYS) != 0);
  if (!ShellUtil::CreateChromeQuickLaunchShortcut(
          browser_dist, chrome_exe.value(), quick_launch_levels, options)) {
    LOG(WARNING) << "Did not create/update quick launch shortcut for "
                 << chrome_exe.value();
  }
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
// temp_path: the path of working directory used during installation. This path
//            does not need to exist.
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
    const FilePath& temp_path,
    const Version& new_version,
    scoped_ptr<Version>* current_version) {
  DCHECK(current_version);

  installer_state.UpdateStage(installer::BUILDING);

  current_version->reset(installer_state.GetCurrentVersion(original_state));
  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());

  AddInstallWorkItems(original_state,
                      installer_state,
                      setup_path,
                      archive_path,
                      src_path,
                      temp_path,
                      new_version,
                      current_version,
                      install_list.get());

  FilePath new_chrome_exe(
      installer_state.target_path().Append(installer::kChromeNewExe));

  installer_state.UpdateStage(installer::EXECUTING);

  if (!install_list->Do()) {
    installer_state.UpdateStage(installer::ROLLINGBACK);
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

  installer_state.UpdateStage(installer::REFRESHING_POLICY);

  installer::RefreshElevationPolicy();

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

void EscapeXmlAttributeValueInSingleQuotes(string16* att_value) {
  ReplaceChars(*att_value, L"&", L"&amp;", att_value);
  ReplaceChars(*att_value, L"'", L"&apos;", att_value);
  ReplaceChars(*att_value, L"<", L"&lt;", att_value);
}

bool CreateVisualElementsManifest(const FilePath& src_path,
                                  const Version& version) {
  // Construct the relative path to the versioned VisualElements directory.
  string16 elements_dir(ASCIIToUTF16(version.GetString()));
  elements_dir.push_back(FilePath::kSeparators[0]);
  elements_dir.append(installer::kVisualElements);

  // Some distributions of Chromium may not include visual elements. Only
  // proceed if this distribution does.
  if (!file_util::PathExists(src_path.Append(elements_dir))) {
    VLOG(1) << "No visual elements found, not writing "
            << installer::kVisualElementsManifest << " to " << src_path.value();
    return true;
  } else {
    // A printf_p-style format string for generating the visual elements
    // manifest. Required arguments, in order, are:
    //   - Localized display name for the product.
    //   - Relative path to the VisualElements directory.
    static const char kManifestTemplate[] =
        "<Application>\r\n"
        "  <VisualElements\r\n"
        "      DisplayName='%1$ls'\r\n"
        "      Logo='%2$ls\\Logo.png'\r\n"
        "      SmallLogo='%2$ls\\SmallLogo.png'\r\n"
        "      ForegroundText='light'\r\n"
        "      BackgroundColor='white'>\r\n"
        "    <DefaultTile ShowName='allLogos'/>\r\n"
        "    <SplashScreen Image='%2$ls\\splash-620x300.png'/>\r\n"
        "  </VisualElements>\r\n"
        "</Application>";

    const string16 manifest_template(ASCIIToUTF16(kManifestTemplate));

    BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BROWSER);
    // TODO(grt): http://crbug.com/75152 Write a reference to a localized
    // resource for |display_name|.
    string16 display_name(dist->GetAppShortCutName());
    EscapeXmlAttributeValueInSingleQuotes(&display_name);

    // Fill the manifest with the desired values.
    string16 manifest16(base::StringPrintf(manifest_template.c_str(),
                                           display_name.c_str(),
                                           elements_dir.c_str()));

    // Write the manifest to |src_path|.
    const std::string manifest(UTF16ToUTF8(manifest16));
    if (file_util::WriteFile(
            src_path.Append(installer::kVisualElementsManifest),
            manifest.c_str(), manifest.size())) {
      VLOG(1) << "Successfully wrote " << installer::kVisualElementsManifest
              << " to " << src_path.value();
      return true;
    } else {
      PLOG(ERROR) << "Error writing " << installer::kVisualElementsManifest
                  << " to " << src_path.value();
      return false;
    }
  }
}

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

  // Create VisualElementManifest.xml in |src_path| (if required) so that it
  // looks as if it had been extracted from the archive when calling
  // InstallNewVersion() below.
  installer_state.UpdateStage(installer::CREATING_VISUAL_MANIFEST);
  CreateVisualElementsManifest(src_path, new_version);

  scoped_ptr<Version> existing_version;
  InstallStatus result = InstallNewVersion(original_state, installer_state,
      setup_path, archive_path, src_path, install_temp_path, new_version,
      &existing_version);

  // TODO(robertshield): Everything below this line should instead be captured
  // by WorkItems.
  if (!InstallUtil::GetInstallReturnCode(result)) {
    installer_state.UpdateStage(installer::UPDATING_CHANNELS);

    // Update the modifiers on the channel values for the product(s) being
    // installed and for the binaries in case of multi-install.
    installer_state.UpdateChannels();

    installer_state.UpdateStage(installer::COPYING_PREFERENCES_FILE);

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
      installer_state.UpdateStage(installer::CREATING_SHORTCUTS);

      bool create_all_shortcut = false;
      prefs.GetBool(master_preferences::kCreateAllShortcuts,
                    &create_all_shortcut);
      bool alt_shortcut = false;
      prefs.GetBool(master_preferences::kAltShortcutText, &alt_shortcut);
      uint32 shortcut_options = ShellUtil::SHORTCUT_NO_OPTIONS;
      if (create_all_shortcut)
        shortcut_options |= ShellUtil::SHORTCUT_CREATE_ALWAYS;
      if (alt_shortcut)
        shortcut_options |= ShellUtil::SHORTCUT_ALTERNATE;
      CreateOrUpdateChromeShortcuts(installer_state, setup_path,
                                    new_version, result, *chrome_install,
                                    shortcut_options);

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

      installer_state.UpdateStage(installer::REGISTERING_CHROME);

      RegisterChromeOnMachine(installer_state, *chrome_install,
          make_chrome_default || force_chrome_default_for_user);

      if (result == FIRST_INSTALL_SUCCESS) {
        installer_state.UpdateStage(installer::CONFIGURE_AUTO_LAUNCH);

        // Add auto-launch key if specified in master preferences.
        bool auto_launch_chrome = false;
        prefs.GetBool(
            installer::master_preferences::kAutoLaunchChrome,
            &auto_launch_chrome);
        if (auto_launch_chrome) {
          auto_launch_util::EnableForegroundStartAtLogin(
              ASCIIToUTF16(chrome::kInitialProfile),
              installer_state.target_path());
        }
      }
    }

    installer_state.UpdateStage(installer::REMOVING_OLD_VERSIONS);

    installer_state.RemoveOldVersionDirectories(
        new_version,
        existing_version.get(),
        install_temp_path);
  }

  return result;
}

}  // namespace installer
