// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the methods useful for uninstalling Chrome.

#include "chrome/installer/setup/uninstall.h"

#include <windows.h>

#include <vector>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "rlz/lib/rlz_lib.h"

// Build-time generated include file.
#include "registered_dlls.h"  // NOLINT

using base::win::RegKey;
using installer::InstallStatus;
using installer::MasterPreferences;

namespace {

// Avoid leaving behind a Temp dir.  If one exists, ask SelfCleaningTempDir to
// clean it up for us.  This may involve scheduling it for deletion after
// reboot.  Don't report that a reboot is required in this case, however.
// TODO(erikwright): Shouldn't this still lead to
// ScheduleParentAndGrandparentForDeletion?
void DeleteInstallTempDir(const FilePath& target_path) {
  FilePath temp_path(target_path.DirName().Append(installer::kInstallTempDir));
  if (file_util::DirectoryExists(temp_path)) {
    installer::SelfCleaningTempDir temp_dir;
    if (!temp_dir.Initialize(target_path.DirName(),
                             installer::kInstallTempDir) ||
        !temp_dir.Delete()) {
      LOG(ERROR) << "Failed to delete temp dir " << temp_path.value();
    }
  }
}

// Iterates over the list of distribution types in |dist_types|, and
// adds to |update_list| the work item to update the corresponding "ap"
// registry value specified in |channel_info|.
void AddChannelValueUpdateWorkItems(
    const installer::InstallationState& original_state,
    const installer::InstallerState& installer_state,
    const installer::ChannelInfo& channel_info,
    const std::vector<BrowserDistribution::Type>& dist_types,
    WorkItemList* update_list) {
  const bool system_level = installer_state.system_install();
  const HKEY reg_root = installer_state.root_key();
  for (size_t i = 0; i < dist_types.size(); ++i) {
    BrowserDistribution::Type dist_type = dist_types[i];
    const installer::ProductState* product_state =
        original_state.GetProductState(system_level, dist_type);
    // Only modify other products if they're installed and multi.
    if (product_state != NULL &&
        product_state->is_multi_install() &&
        !product_state->channel().Equals(channel_info)) {
      BrowserDistribution* other_dist =
          BrowserDistribution::GetSpecificDistribution(dist_type);
      update_list->AddSetRegValueWorkItem(reg_root, other_dist->GetStateKey(),
          google_update::kRegApField, channel_info.value(), true);
    } else {
      LOG_IF(ERROR,
             product_state != NULL && product_state->is_multi_install())
          << "Channel value for "
          << BrowserDistribution::GetSpecificDistribution(
                 dist_type)->GetAppShortCutName()
          << " is somehow already set to the desired new value of "
          << channel_info.value();
    }
  }
}

// Makes appropriate changes to the Google Update "ap" value in the registry.
// Specifically, removes the flags associated with this product ("-chrome" or
// "-chromeframe[-readymode]") from the "ap" values for all other
// installed products and for the multi-installer package.
void ProcessGoogleUpdateItems(
    const installer::InstallationState& original_state,
    const installer::InstallerState& installer_state,
    const installer::Product& product) {
  DCHECK(installer_state.is_multi_install());
  const bool system_level = installer_state.system_install();
  BrowserDistribution* distribution = product.distribution();
  const installer::ProductState* product_state =
      original_state.GetProductState(system_level, distribution->GetType());
  DCHECK(product_state != NULL);
  installer::ChannelInfo channel_info;

  // Remove product's flags from the channel value.
  channel_info.set_value(product_state->channel().value());
  const bool modified = product.SetChannelFlags(false, &channel_info);

  // Apply the new channel value to all other products and to the multi package.
  if (modified) {
    scoped_ptr<WorkItemList>
        update_list(WorkItem::CreateNoRollbackWorkItemList());
    std::vector<BrowserDistribution::Type> dist_types;
    for (size_t i = 0; i < BrowserDistribution::NUM_TYPES; ++i) {
      BrowserDistribution::Type other_dist_type =
          static_cast<BrowserDistribution::Type>(i);
      if (distribution->GetType() != other_dist_type)
        dist_types.push_back(other_dist_type);
    }
    AddChannelValueUpdateWorkItems(original_state, installer_state,
                                   channel_info, dist_types,
                                   update_list.get());
    bool success = update_list->Do();
    LOG_IF(ERROR, !success) << "Failed updating channel values.";
  }
}

void ProcessOnOsUpgradeWorkItems(
    const installer::InstallerState& installer_state,
    const installer::Product& product) {
  scoped_ptr<WorkItemList> work_item_list(
      WorkItem::CreateNoRollbackWorkItemList());
  AddOsUpgradeWorkItems(installer_state, FilePath(), Version(), product,
                        work_item_list.get());
  if (!work_item_list->Do())
    LOG(ERROR) << "Failed to remove on-os-upgrade command.";
}

// Adds or removes the quick-enable-cf command to the binaries' version key in
// the registry as needed.
void ProcessQuickEnableWorkItems(
    const installer::InstallerState& installer_state,
    const installer::InstallationState& machine_state) {
  scoped_ptr<WorkItemList> work_item_list(
      WorkItem::CreateNoRollbackWorkItemList());

  AddQuickEnableChromeFrameWorkItems(installer_state, machine_state, FilePath(),
                                     Version(), work_item_list.get());

  AddQuickEnableApplicationLauncherWorkItems(installer_state, machine_state,
                                             FilePath(), Version(),
                                             work_item_list.get());
  if (!work_item_list->Do())
    LOG(ERROR) << "Failed to update quick-enable-cf command.";
}

void ProcessIELowRightsPolicyWorkItems(
    const installer::InstallerState& installer_state) {
  scoped_ptr<WorkItemList> work_items(WorkItem::CreateNoRollbackWorkItemList());
  AddDeleteOldIELowRightsPolicyWorkItems(installer_state, work_items.get());
  work_items->Do();
  installer::RefreshElevationPolicy();
}

void ClearRlzProductState() {
  const rlz_lib::AccessPoint points[] = {rlz_lib::CHROME_OMNIBOX,
                                         rlz_lib::CHROME_HOME_PAGE,
                                         rlz_lib::NO_ACCESS_POINT};

  rlz_lib::ClearProductState(rlz_lib::CHROME, points);

  // If chrome has been reactivated, clear all events for this brand as well.
  string16 reactivation_brand_wide;
  if (GoogleUpdateSettings::GetReactivationBrand(&reactivation_brand_wide)) {
    std::string reactivation_brand(WideToASCII(reactivation_brand_wide));
    rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
    rlz_lib::ClearProductState(rlz_lib::CHROME, points);
  }
}

// Decides whether setup.exe and the installer archive should be removed based
// on the original and installer states:
// * non-multi product being uninstalled: remove both
// * any multi product left besides App Host: keep both
// * only App Host left: keep setup.exe
void CheckShouldRemoveSetupAndArchive(
    const installer::InstallationState& original_state,
    const installer::InstallerState& installer_state,
    bool* remove_setup,
    bool* remove_archive) {
  *remove_setup = true;
  *remove_archive = true;

  // If any multi-install product is left (other than App Host) we must leave
  // the installer and archive. For the App Host, we only leave the installer.
  if (!installer_state.is_multi_install()) {
    VLOG(1) << "Removing all installer files for a non-multi installation.";
  } else {
    for (size_t i = 0; i < BrowserDistribution::NUM_TYPES; ++i) {
      BrowserDistribution::Type dist_type =
          static_cast<BrowserDistribution::Type>(i);
      const installer::ProductState* product_state =
          original_state.GetProductState(
              installer_state.system_install(), dist_type);
      if (product_state && product_state->is_multi_install() &&
          !installer_state.FindProduct(dist_type)) {
        // setup.exe will not be removed as there is a remaining multi-install
        // product.
        *remove_setup = false;
        // As a special case, we can still remove the actual archive if the
        // only remaining product is the App Host.
        if (dist_type != BrowserDistribution::CHROME_APP_HOST) {
          VLOG(1) << "Keeping all installer files due to a remaining "
                  << "multi-install product.";
          *remove_archive = false;
          return;
        }
        VLOG(1) << "Keeping setup.exe due to a remaining "
                << "app-host installation.";
      }
    }
    VLOG(1) << "Removing the installer archive.";
    if (remove_setup)
      VLOG(1) << "Removing setup.exe.";
  }
}

// Removes all files from the installer directory, leaving setup.exe iff
// |remove_setup| is false.
// Returns false in case of an error.
bool RemoveInstallerFiles(const FilePath& install_directory,
                          bool remove_setup) {
  using file_util::FileEnumerator;
  FileEnumerator file_enumerator(
      install_directory,
      false,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
  bool success = true;

  FilePath setup_exe_base_name(installer::kSetupExe);

  while (true) {
    FilePath to_delete(file_enumerator.Next());
    if (to_delete.empty())
      break;
    if (!remove_setup && to_delete.BaseName() == setup_exe_base_name)
      continue;

    VLOG(1) << "Deleting install path " << to_delete.value();
    if (!file_util::Delete(to_delete, true)) {
      LOG(ERROR) << "Failed to delete path: " << to_delete.value();
      success = false;
    }
  }

  return success;
}

}  // namespace

namespace installer {

// Kills all Chrome processes, immediately.
void CloseAllChromeProcesses() {
  base::CleanupProcesses(installer::kChromeExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
  base::CleanupProcesses(installer::kNaClExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
}

// Attempts to close the Chrome Frame helper process by sending WM_CLOSE
// messages to its window, or just killing it if that doesn't work.
void CloseChromeFrameHelperProcess() {
  HWND window = FindWindow(installer::kChromeFrameHelperWndClass, NULL);
  if (!::IsWindow(window))
    return;

  const DWORD kWaitMs = 3000;

  DWORD pid = 0;
  ::GetWindowThreadProcessId(window, &pid);
  DCHECK_NE(pid, 0U);
  base::win::ScopedHandle process(::OpenProcess(SYNCHRONIZE, FALSE, pid));
  PLOG_IF(INFO, !process) << "Failed to open process: " << pid;

  bool kill = true;
  if (SendMessageTimeout(window, WM_CLOSE, 0, 0, SMTO_BLOCK, kWaitMs, NULL) &&
      process) {
    VLOG(1) << "Waiting for " << installer::kChromeFrameHelperExe;
    DWORD wait = ::WaitForSingleObject(process, kWaitMs);
    if (wait != WAIT_OBJECT_0) {
      LOG(WARNING) << "Wait for " << installer::kChromeFrameHelperExe
                   << " to exit failed or timed out.";
    } else {
      kill = false;
      VLOG(1) << installer::kChromeFrameHelperExe << " exited normally.";
    }
  }

  if (kill) {
    VLOG(1) << installer::kChromeFrameHelperExe << " hung.  Killing.";
    base::CleanupProcesses(installer::kChromeFrameHelperExe, base::TimeDelta(),
                           content::RESULT_CODE_HUNG, NULL);
  }
}

// Deletes shortcuts at |install_level| from Start menu, Desktop,
// Quick Launch, taskbar, and secondary tiles on the Start Screen (Win8+).
// Only shortcuts pointing to |target_exe| will be removed.
void DeleteShortcuts(const InstallerState& installer_state,
                     const Product& product,
                     const FilePath& target_exe) {
  BrowserDistribution* dist = product.distribution();

  // The per-user shortcut for this user, if present on a system-level install,
  // has already been deleted in chrome_browser_main_win.cc::DoUninstallTasks().
  ShellUtil::ShellChange install_level = installer_state.system_install() ?
      ShellUtil::SYSTEM_LEVEL : ShellUtil::CURRENT_USER;

  VLOG(1) << "Deleting Desktop shortcut.";
  if (!ShellUtil::RemoveShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist,
                                 target_exe, install_level, NULL)) {
    LOG(WARNING) << "Failed to delete Desktop shortcut.";
  }
  // Also try to delete the alternate desktop shortcut. It is not sufficient
  // to do so upon failure of the above call as ERROR_FILE_NOT_FOUND on
  // delete is considered success.
  if (!ShellUtil::RemoveShortcut(
          ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist, target_exe, install_level,
          &dist->GetAlternateApplicationName())) {
    LOG(WARNING) << "Failed to delete alternate Desktop shortcut.";
  }

  VLOG(1) << "Deleting Quick Launch shortcut.";
  if (!ShellUtil::RemoveShortcut(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                                 dist, target_exe, install_level, NULL)) {
    LOG(WARNING) << "Failed to delete Quick Launch shortcut.";
  }

  VLOG(1) << "Deleting Start Menu shortcuts.";
  if (!ShellUtil::RemoveShortcut(ShellUtil::SHORTCUT_LOCATION_START_MENU, dist,
                                 target_exe, install_level, NULL)) {
    LOG(WARNING) << "Failed to delete Start Menu shortcuts.";
  }

  // Although the shortcut removal calls above will unpin their shortcut if they
  // result in a deletion (i.e. shortcut existed and pointed to |target_exe|),
  // it is possible for shortcuts to remain pinned while their parent shortcut
  // has been deleted or changed to point to another |target_exe|. Make sure all
  // pinned-to-taskbar shortcuts that point to |target_exe| are unpinned.
  ShellUtil::RemoveTaskbarShortcuts(target_exe.value());

  ShellUtil::RemoveStartScreenShortcuts(product.distribution(),
                                        target_exe.value());
}

bool ScheduleParentAndGrandparentForDeletion(const FilePath& path) {
  FilePath parent_dir = path.DirName();
  bool ret = ScheduleFileSystemEntityForDeletion(parent_dir.value().c_str());
  if (!ret) {
    LOG(ERROR) << "Failed to schedule parent dir for deletion: "
               << parent_dir.value();
  } else {
    FilePath grandparent_dir(parent_dir.DirName());
    ret = ScheduleFileSystemEntityForDeletion(grandparent_dir.value().c_str());
    if (!ret) {
      LOG(ERROR) << "Failed to schedule grandparent dir for deletion: "
                 << grandparent_dir.value();
    }
  }
  return ret;
}

enum DeleteResult {
  DELETE_SUCCEEDED,
  DELETE_NOT_EMPTY,
  DELETE_FAILED,
  DELETE_REQUIRES_REBOOT,
};

// Deletes the given directory if it is empty. Returns DELETE_SUCCEEDED if the
// directory is deleted, DELETE_NOT_EMPTY if it is not empty, and DELETE_FAILED
// otherwise.
DeleteResult DeleteEmptyDir(const FilePath& path) {
  if (!file_util::IsDirectoryEmpty(path))
    return DELETE_NOT_EMPTY;

  if (file_util::Delete(path, true))
    return DELETE_SUCCEEDED;

  LOG(ERROR) << "Failed to delete folder: " << path.value();
  return DELETE_FAILED;
}

void GetLocalStateFolders(const Product& product,
                          std::vector<FilePath>* paths) {
  // Obtain the location of the user profile data.
  product.GetUserDataPaths(paths);
  LOG_IF(ERROR, paths->empty())
      << "Could not retrieve user's profile directory.";
}

// Creates a copy of the local state file and returns a path to the copy.
FilePath BackupLocalStateFile(
    const std::vector<FilePath>& local_state_folders) {
  FilePath backup;

  // Copy the first local state file that is found.
  for (size_t i = 0; i < local_state_folders.size(); ++i) {
    const FilePath& local_state_folder = local_state_folders[i];
    FilePath state_file(local_state_folder.Append(chrome::kLocalStateFilename));
    if (!file_util::PathExists(state_file))
      continue;
    if (!file_util::CreateTemporaryFile(&backup))
      LOG(ERROR) << "Failed to create temporary file for Local State.";
    else
      file_util::CopyFile(state_file, backup);
    break;
  }
  return backup;
}

// Deletes all user data directories for a product.
DeleteResult DeleteLocalState(const std::vector<FilePath>& local_state_folders,
                              bool schedule_on_failure) {
  if (local_state_folders.empty())
    return DELETE_SUCCEEDED;

  DeleteResult result = DELETE_SUCCEEDED;
  for (size_t i = 0; i < local_state_folders.size(); ++i) {
    const FilePath& user_local_state = local_state_folders[i];
    VLOG(1) << "Deleting user profile " << user_local_state.value();
    if (!file_util::Delete(user_local_state, true)) {
      LOG(ERROR) << "Failed to delete user profile dir: "
                 << user_local_state.value();
      if (schedule_on_failure) {
        ScheduleDirectoryForDeletion(user_local_state.value().c_str());
        result = DELETE_REQUIRES_REBOOT;
      } else {
        result = DELETE_FAILED;
      }
    }
  }

  if (result == DELETE_REQUIRES_REBOOT) {
    ScheduleParentAndGrandparentForDeletion(local_state_folders[0]);
  } else {
    const FilePath user_data_dir(local_state_folders[0].DirName());
    if (!user_data_dir.empty() &&
        DeleteEmptyDir(user_data_dir) == DELETE_SUCCEEDED) {
      const FilePath product_dir(user_data_dir.DirName());
      if (!product_dir.empty())
        DeleteEmptyDir(product_dir);
    }
  }

  return result;
}

// Moves setup to a temporary file, outside of the install folder. Also attempts
// to change the current directory to the TMP directory. On Windows, each
// process has a handle to its CWD. If setup.exe's CWD happens to be within the
// install directory, deletion will fail as a result of the open handle.
bool MoveSetupOutOfInstallFolder(const InstallerState& installer_state,
                                 const FilePath& setup_exe) {
  bool ret = false;
  FilePath tmp_dir;
  FilePath temp_file;
  if (!PathService::Get(base::DIR_TEMP, &tmp_dir)) {
    NOTREACHED();
  } else if (!file_util::CreateTemporaryFileInDir(tmp_dir, &temp_file)) {
    LOG(ERROR) << "Failed to create temporary file for setup.exe.";
  } else {
    VLOG(1) << "Changing current directory to: " << tmp_dir.value();
    if (!file_util::SetCurrentDirectory(tmp_dir))
      PLOG(ERROR) << "Failed to change the current directory.";

    VLOG(1) << "Attempting to move setup to: " << temp_file.value();
    ret = file_util::Move(setup_exe, temp_file);
    PLOG_IF(ERROR, !ret) << "Failed to move setup to " << temp_file.value();

    // We cannot delete the file right away, but try to delete it some other
    // way. Either with the help of a different process or the system.
    if (ret && !file_util::DeleteAfterReboot(temp_file)) {
      static const uint32 kDeleteAfterMs = 10 * 1000;
      installer::DeleteFileFromTempProcess(temp_file, kDeleteAfterMs);
    }
  }
  return ret;
}

DeleteResult DeleteApplicationProductAndVendorDirectories(
    const FilePath& application_directory) {
  DeleteResult result(DeleteEmptyDir(application_directory));
  if (result == DELETE_SUCCEEDED) {
    // Now check and delete if the parent directories are empty
    // For example Google\Chrome or Chromium
    const FilePath product_directory(application_directory.DirName());
    if (!product_directory.empty()) {
        result = DeleteEmptyDir(product_directory);
        if (result == DELETE_SUCCEEDED) {
          const FilePath vendor_directory(product_directory.DirName());
          if (!vendor_directory.empty())
            result = DeleteEmptyDir(vendor_directory);
        }
    }
  }
  if (result == DELETE_NOT_EMPTY)
    result = DELETE_SUCCEEDED;
  return result;
}

DeleteResult DeleteAppHostFilesAndFolders(const InstallerState& installer_state,
                                          const Version& installed_version) {
  const FilePath& target_path = installer_state.target_path();
  if (target_path.empty()) {
    LOG(ERROR) << "DeleteAppHostFilesAndFolders: no installation destination "
               << "path.";
    return DELETE_FAILED;  // Nothing else we can do to uninstall, so we return.
  }

  DeleteInstallTempDir(target_path);

  DeleteResult result = DELETE_SUCCEEDED;

  FilePath app_host_exe(target_path.Append(installer::kChromeAppHostExe));
  if (!file_util::Delete(app_host_exe, false)) {
    result = DELETE_FAILED;
    LOG(ERROR) << "Failed to delete path: " << app_host_exe.value();
  }

  return result;
}

DeleteResult DeleteChromeFilesAndFolders(const InstallerState& installer_state,
                                         const FilePath& installer_path) {
  const FilePath& target_path = installer_state.target_path();
  if (target_path.empty()) {
    LOG(ERROR) << "DeleteChromeFilesAndFolders: no installation destination "
               << "path.";
    return DELETE_FAILED;  // Nothing else we can do to uninstall, so we return.
  }

  DeleteInstallTempDir(target_path);

  DeleteResult result = DELETE_SUCCEEDED;

  FilePath installer_directory;
  if (target_path.IsParent(installer_path))
    installer_directory = installer_path.DirName();

  // Enumerate all the files in target_path recursively (breadth-first).
  // We delete a file or folder unless it is a parent/child of the installer
  // directory. For parents of the installer directory, we will later recurse
  // and delete all the children (that are not also parents/children of the
  // installer directory).
  using file_util::FileEnumerator;
  FileEnumerator file_enumerator(
      target_path, true, FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
  while (true) {
    FilePath to_delete(file_enumerator.Next());
    if (to_delete.empty())
      break;
    if (to_delete.BaseName().value() == installer::kChromeAppHostExe)
      continue;
    if (!installer_directory.empty() &&
        (to_delete == installer_directory ||
         installer_directory.IsParent(to_delete) ||
         to_delete.IsParent(installer_directory))) {
      continue;
    }

    VLOG(1) << "Deleting install path " << to_delete.value();
    if (!file_util::Delete(to_delete, true)) {
      LOG(ERROR) << "Failed to delete path (1st try): " << to_delete.value();
      if (installer_state.FindProduct(BrowserDistribution::CHROME_FRAME)) {
        // We don't try killing Chrome processes for Chrome Frame builds since
        // that is unlikely to help. Instead, schedule files for deletion and
        // return a value that will trigger a reboot prompt.
        FileEnumerator::FindInfo find_info;
        file_enumerator.GetFindInfo(&find_info);
        if (FileEnumerator::IsDirectory(find_info))
          ScheduleDirectoryForDeletion(to_delete.value().c_str());
        else
          ScheduleFileSystemEntityForDeletion(to_delete.value().c_str());
        result = DELETE_REQUIRES_REBOOT;
      } else {
        // Try closing any running Chrome processes and deleting files once
        // again.
        CloseAllChromeProcesses();
        if (!file_util::Delete(to_delete, true)) {
          LOG(ERROR) << "Failed to delete path (2nd try): "
                     << to_delete.value();
          result = DELETE_FAILED;
          break;
        }
      }
    }
  }

  return result;
}

// This method checks if Chrome is currently running or if the user has
// cancelled the uninstall operation by clicking Cancel on the confirmation
// box that Chrome pops up.
InstallStatus IsChromeActiveOrUserCancelled(
    const InstallerState& installer_state,
    const Product& product) {
  int32 exit_code = content::RESULT_CODE_NORMAL_EXIT;
  CommandLine options(CommandLine::NO_PROGRAM);
  options.AppendSwitch(installer::switches::kUninstall);

  // Here we want to save user from frustration (in case of Chrome crashes)
  // and continue with the uninstallation as long as chrome.exe process exit
  // code is NOT one of the following:
  // - UNINSTALL_CHROME_ALIVE - chrome.exe is currently running
  // - UNINSTALL_USER_CANCEL - User cancelled uninstallation
  // - HUNG - chrome.exe was killed by HuntForZombieProcesses() (until we can
  //          give this method some brains and not kill chrome.exe launched
  //          by us, we will not uninstall if we get this return code).
  VLOG(1) << "Launching Chrome to do uninstall tasks.";
  if (product.LaunchChromeAndWait(installer_state.target_path(), options,
                                  &exit_code)) {
    VLOG(1) << "chrome.exe launched for uninstall confirmation returned: "
            << exit_code;
    if ((exit_code == chrome::RESULT_CODE_UNINSTALL_CHROME_ALIVE) ||
        (exit_code == chrome::RESULT_CODE_UNINSTALL_USER_CANCEL) ||
        (exit_code == content::RESULT_CODE_HUNG))
      return installer::UNINSTALL_CANCELLED;

    if (exit_code == chrome::RESULT_CODE_UNINSTALL_DELETE_PROFILE)
      return installer::UNINSTALL_DELETE_PROFILE;
  } else {
    PLOG(ERROR) << "Failed to launch chrome.exe for uninstall confirmation.";
  }

  return installer::UNINSTALL_CONFIRMED;
}

bool ShouldDeleteProfile(const InstallerState& installer_state,
                         const CommandLine& cmd_line, InstallStatus status,
                         const Product& product) {
  bool should_delete = false;

  // Chrome Frame uninstallations always want to delete the profile (we have no
  // UI to prompt otherwise and the profile stores no useful data anyway)
  // unless they are managed by MSI. MSI uninstalls will explicitly include
  // the --delete-profile flag to distinguish them from MSI upgrades.
  if (product.is_chrome_frame() && !installer_state.is_msi()) {
    should_delete = true;
  } else {
    should_delete =
        status == installer::UNINSTALL_DELETE_PROFILE ||
        cmd_line.HasSwitch(installer::switches::kDeleteProfile);
  }

  return should_delete;
}

// Removes XP-era filetype registration making Chrome the default browser.
// MSDN (see http://msdn.microsoft.com/library/windows/desktop/cc144148.aspx)
// tells us not to do this, but certain applications break following
// uninstallation if we don't.
void RemoveFiletypeRegistration(const InstallerState& installer_state,
                                HKEY root,
                                const string16& browser_entry_suffix) {
  string16 classes_path(ShellUtil::kRegClasses);
  classes_path.push_back(FilePath::kSeparators[0]);

  const string16 prog_id(ShellUtil::kChromeHTMLProgId + browser_entry_suffix);

  // Delete each filetype association if it references this Chrome.  Take care
  // not to delete the association if it references a system-level install of
  // Chrome (only a risk if the suffix is empty).  Don't delete the whole key
  // since other apps may have stored data there.
  std::vector<const wchar_t*> cleared_assocs;
  if (installer_state.system_install() ||
      !browser_entry_suffix.empty() ||
      !base::win::RegKey(HKEY_LOCAL_MACHINE, (classes_path + prog_id).c_str(),
                         KEY_QUERY_VALUE).Valid()) {
    InstallUtil::ValueEquals prog_id_pred(prog_id);
    for (const wchar_t* const* filetype = &ShellUtil::kFileAssociations[0];
         *filetype != NULL; ++filetype) {
      if (InstallUtil::DeleteRegistryValueIf(
              root, (classes_path + *filetype).c_str(), NULL,
              prog_id_pred) == InstallUtil::DELETED) {
        cleared_assocs.push_back(*filetype);
      }
    }
  }

  // For all filetype associations in HKLM that have just been removed, attempt
  // to restore some reasonable value.  We have no definitive way of knowing
  // what handlers are the most appropriate, so we use a fixed mapping based on
  // the default values for a fresh install of Windows.
  if (root == HKEY_LOCAL_MACHINE) {
    string16 assoc;
    base::win::RegKey key;

    for (size_t i = 0; i < cleared_assocs.size(); ++i) {
      const wchar_t* replacement_prog_id = NULL;
      assoc.assign(cleared_assocs[i]);

      // Inelegant, but simpler than a pure data-driven approach.
      if (assoc == L".htm" || assoc == L".html")
        replacement_prog_id = L"htmlfile";
      else if (assoc == L".xht" || assoc == L".xhtml")
        replacement_prog_id = L"xhtmlfile";

      if (!replacement_prog_id) {
        LOG(WARNING) << "No known replacement ProgID for " << assoc
                     << " files.";
      } else if (key.Open(HKEY_LOCAL_MACHINE,
                          (classes_path + replacement_prog_id).c_str(),
                          KEY_QUERY_VALUE) == ERROR_SUCCESS &&
                 (key.Open(HKEY_LOCAL_MACHINE, (classes_path + assoc).c_str(),
                           KEY_SET_VALUE) != ERROR_SUCCESS ||
                  key.WriteValue(NULL, replacement_prog_id) != ERROR_SUCCESS)) {
        // The replacement ProgID is registered on the computer but the attempt
        // to set it for the filetype failed.
        LOG(ERROR) << "Failed to restore system-level filetype association "
                   << assoc << " = " << replacement_prog_id;
      }
    }
  }
}

bool DeleteChromeRegistrationKeys(const InstallerState& installer_state,
                                  BrowserDistribution* dist,
                                  HKEY root,
                                  const string16& browser_entry_suffix,
                                  InstallStatus* exit_code) {
  DCHECK(exit_code);
  if (!dist->CanSetAsDefault()) {
    // We should have never set those keys.
    return true;
  }

  FilePath chrome_exe(installer_state.target_path().Append(kChromeExe));

  // Delete Software\Classes\ChromeHTML.
  // For user-level installs we now only write these entries in HKCU, but since
  // old installs did install them to HKLM we will try to remove them in HKLM as
  // well anyways.
  const string16 prog_id(ShellUtil::kChromeHTMLProgId + browser_entry_suffix);
  string16 reg_prog_id(ShellUtil::kRegClasses);
  reg_prog_id.push_back(FilePath::kSeparators[0]);
  reg_prog_id.append(prog_id);
  InstallUtil::DeleteRegistryKey(root, reg_prog_id);

  // Delete Software\Classes\Chrome (Same comment as above applies for this too)
  string16 reg_app_id(ShellUtil::kRegClasses);
  reg_app_id.push_back(FilePath::kSeparators[0]);
  // Append the requested suffix manually here (as ShellUtil::GetBrowserModelId
  // would otherwise try to figure out the currently installed suffix).
  reg_app_id.append(dist->GetBaseAppId() + browser_entry_suffix);
  InstallUtil::DeleteRegistryKey(root, reg_app_id);

  // Delete all Start Menu Internet registrations that refer to this Chrome.
  {
    using base::win::RegistryKeyIterator;
    InstallUtil::ProgramCompare open_command_pred(chrome_exe);
    string16 client_name;
    string16 client_key;
    string16 open_key;
    for (RegistryKeyIterator iter(root, ShellUtil::kRegStartMenuInternet);
         iter.Valid(); ++iter) {
      client_name.assign(iter.Name());
      client_key.assign(ShellUtil::kRegStartMenuInternet)
          .append(1, L'\\')
          .append(client_name);
      open_key.assign(client_key).append(ShellUtil::kRegShellOpen);
      if (InstallUtil::DeleteRegistryKeyIf(root, client_key, open_key, NULL,
              open_command_pred) != InstallUtil::NOT_FOUND) {
        // Delete the default value of SOFTWARE\Clients\StartMenuInternet if it
        // references this Chrome (i.e., if it was made the default browser).
        InstallUtil::DeleteRegistryValueIf(
            root, ShellUtil::kRegStartMenuInternet, NULL,
            InstallUtil::ValueEquals(client_name));
        // Also delete the value for the default user if we're operating in
        // HKLM.
        if (root == HKEY_LOCAL_MACHINE) {
          InstallUtil::DeleteRegistryValueIf(
              HKEY_USERS,
              string16(L".DEFAULT\\").append(
                  ShellUtil::kRegStartMenuInternet).c_str(),
              NULL, InstallUtil::ValueEquals(client_name));
        }
      }
    }
  }

  // Delete Software\RegisteredApplications\Chromium
  InstallUtil::DeleteRegistryValue(
      root, ShellUtil::kRegRegisteredApplications,
      dist->GetBaseAppName() + browser_entry_suffix);

  // Delete the App Paths and Applications keys that let Explorer find Chrome:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ee872121
  string16 app_key(ShellUtil::kRegClasses);
  app_key.push_back(FilePath::kSeparators[0]);
  app_key.append(L"Applications");
  app_key.push_back(FilePath::kSeparators[0]);
  app_key.append(installer::kChromeExe);
  InstallUtil::DeleteRegistryKey(root, app_key);

  string16 app_path_key(ShellUtil::kAppPathsRegistryKey);
  app_path_key.push_back(FilePath::kSeparators[0]);
  app_path_key.append(installer::kChromeExe);
  InstallUtil::DeleteRegistryKey(root, app_path_key);

  // Cleanup OpenWithList and OpenWithProgids:
  // http://msdn.microsoft.com/en-us/library/bb166549
  string16 file_assoc_key;
  string16 open_with_list_key;
  string16 open_with_progids_key;
  for (int i = 0; ShellUtil::kFileAssociations[i] != NULL; ++i) {
    file_assoc_key.assign(ShellUtil::kRegClasses);
    file_assoc_key.push_back(FilePath::kSeparators[0]);
    file_assoc_key.append(ShellUtil::kFileAssociations[i]);
    file_assoc_key.push_back(FilePath::kSeparators[0]);

    open_with_list_key.assign(file_assoc_key);
    open_with_list_key.append(L"OpenWithList");
    open_with_list_key.push_back(FilePath::kSeparators[0]);
    open_with_list_key.append(installer::kChromeExe);
    InstallUtil::DeleteRegistryKey(root, open_with_list_key);

    open_with_progids_key.assign(file_assoc_key);
    open_with_progids_key.append(ShellUtil::kRegOpenWithProgids);
    InstallUtil::DeleteRegistryValue(root, open_with_progids_key, prog_id);
  }

  // Cleanup in case Chrome had been made the default browser.

  // Delete the default value of SOFTWARE\Clients\StartMenuInternet if it
  // references this Chrome.  Do this explicitly here for the case where HKCU is
  // being processed; the iteration above will have no hits since registration
  // lives in HKLM.
  InstallUtil::DeleteRegistryValueIf(
      root, ShellUtil::kRegStartMenuInternet, NULL,
      InstallUtil::ValueEquals(dist->GetBaseAppName() + browser_entry_suffix));

  // Delete each protocol association if it references this Chrome.
  InstallUtil::ProgramCompare open_command_pred(chrome_exe);
  string16 parent_key(ShellUtil::kRegClasses);
  parent_key.push_back(FilePath::kSeparators[0]);
  const string16::size_type base_length = parent_key.size();
  string16 child_key;
  for (const wchar_t* const* proto =
           &ShellUtil::kPotentialProtocolAssociations[0];
       *proto != NULL;
       ++proto) {
    parent_key.resize(base_length);
    parent_key.append(*proto);
    child_key.assign(parent_key).append(ShellUtil::kRegShellOpen);
    InstallUtil::DeleteRegistryKeyIf(root, parent_key, child_key, NULL,
                                     open_command_pred);
  }

  RemoveFiletypeRegistration(installer_state, root, browser_entry_suffix);

  *exit_code = installer::UNINSTALL_SUCCESSFUL;
  return true;
}

void RemoveChromeLegacyRegistryKeys(BrowserDistribution* dist,
                                    const string16& chrome_exe) {
  // We used to register Chrome to handle crx files, but this turned out
  // to be not worth the hassle. Remove these old registry entries if
  // they exist. See: http://codereview.chromium.org/210007

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kChromeExtProgId[] = L"ChromeExt";
#else
const wchar_t kChromeExtProgId[] = L"ChromiumExt";
#endif

  HKEY roots[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  for (size_t i = 0; i < arraysize(roots); ++i) {
    string16 suffix;
    if (roots[i] == HKEY_LOCAL_MACHINE)
      suffix = ShellUtil::GetCurrentInstallationSuffix(dist, chrome_exe);

    // Delete Software\Classes\ChromeExt,
    string16 ext_prog_id(ShellUtil::kRegClasses);
    ext_prog_id.push_back(FilePath::kSeparators[0]);
    ext_prog_id.append(kChromeExtProgId);
    ext_prog_id.append(suffix);
    InstallUtil::DeleteRegistryKey(roots[i], ext_prog_id);

    // Delete Software\Classes\.crx,
    string16 ext_association(ShellUtil::kRegClasses);
    ext_association.append(L"\\");
    ext_association.append(chrome::kExtensionFileExtension);
    InstallUtil::DeleteRegistryKey(roots[i], ext_association);
  }
}

// Builds and executes a work item list to remove DelegateExecute verb handler
// work items for |product|.  This will be a noop for products whose
// corresponding BrowserDistribution implementations do not publish a CLSID via
// GetCommandExecuteImplClsid.
bool ProcessDelegateExecuteWorkItems(const InstallerState& installer_state,
                                     const Product& product) {
  scoped_ptr<WorkItemList> item_list(WorkItem::CreateNoRollbackWorkItemList());
  AddDelegateExecuteWorkItems(installer_state, FilePath(), Version(), product,
                              item_list.get());
  return item_list->Do();
}

// Removes Active Setup entries from the registry. This cannot be done through
// a work items list as usual because of different paths based on conditionals,
// but otherwise respects the no rollback/best effort uninstall mentality.
// This will only apply for system-level installs of Chrome/Chromium and will be
// a no-op for all other types of installs.
void UninstallActiveSetupEntries(const InstallerState& installer_state,
                                 const Product& product) {
  VLOG(1) << "Uninstalling registry entries for ActiveSetup.";
  BrowserDistribution* distribution = product.distribution();

  if (!product.is_chrome() || !installer_state.system_install()) {
    const char* install_level =
        installer_state.system_install() ? "system" : "user";
    VLOG(1) << "No Active Setup processing to do for " << install_level
            << "-level " << distribution->GetAppShortCutName();
    return;
  }

  const string16 active_setup_path(
      InstallUtil::GetActiveSetupPath(distribution));
  InstallUtil::DeleteRegistryKey(HKEY_LOCAL_MACHINE, active_setup_path);

  // Windows leaves keys behind in HKCU\\Software\\(Wow6432Node\\)?Microsoft\\
  //     Active Setup\\Installed Components\\{guid}
  // for every user that logged in since system-level Chrome was installed.
  // This is a problem because Windows compares the value of the Version subkey
  // in there with the value of the Version subkey in the matching HKLM entries
  // before running Chrome's Active Setup so if Chrome was to be reinstalled
  // with a lesser version (e.g. switching back to a more stable channel), the
  // affected users would not have Chrome's Active Setup called until Chrome
  // eventually updated passed that user's registered Version.
  //
  // It is however very hard to delete those values as the registry hives for
  // other users are not loaded by default under HKEY_USERS (unless a user is
  // logged on or has a process impersonating him).
  //
  // Following our best effort uninstall practices, try to delete the value in
  // all users hives. If a given user's hive is not loaded, try to load it to
  // proceed with the deletion (failure to do so is ignored).

  static const wchar_t kProfileList[] =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\";

  // Windows automatically adds Wow6432Node when creating/deleting the HKLM key,
  // but doesn't seem to do so when manually deleting the user-level keys it
  // created.
  string16 alternate_active_setup_path(active_setup_path);
  alternate_active_setup_path.insert(arraysize("Software\\") - 1,
                                     L"Wow6432Node\\");

  // These two privileges are required by RegLoadKey() and RegUnloadKey() below.
  ScopedTokenPrivilege se_restore_name_privilege(SE_RESTORE_NAME);
  ScopedTokenPrivilege se_backup_name_privilege(SE_BACKUP_NAME);
  if (!se_restore_name_privilege.is_enabled() ||
      !se_backup_name_privilege.is_enabled()) {
    // This is not a critical failure as those privileges aren't required to
    // clean hives that are already loaded, but attempts to LoadRegKey() below
    // will fail.
    LOG(WARNING) << "Failed to enable privileges required to load registry "
                    "hives.";
  }

  for (base::win::RegistryKeyIterator it(HKEY_LOCAL_MACHINE, kProfileList);
       it.Valid(); ++it) {
    const wchar_t* profile_sid = it.Name();

    // First check if this user's registry hive needs to be loaded in
    // HKEY_USERS.
    base::win::RegKey user_reg_root_probe(
        HKEY_USERS, profile_sid, KEY_READ);
    bool loaded_hive = false;
    if (!user_reg_root_probe.Valid()) {
      VLOG(1) << "Attempting to load registry hive for " << profile_sid;

      string16 reg_profile_info_path(kProfileList);
      reg_profile_info_path.append(profile_sid);
      base::win::RegKey reg_profile_info_key(
          HKEY_LOCAL_MACHINE, reg_profile_info_path.c_str(), KEY_READ);

      string16 profile_path;
      LONG result = reg_profile_info_key.ReadValue(L"ProfileImagePath",
                                                   &profile_path);
      if (result != ERROR_SUCCESS) {
        LOG(ERROR) << "Error reading ProfileImagePath: " << result;
        continue;
      }
      FilePath registry_hive_file(profile_path);
      registry_hive_file = registry_hive_file.AppendASCII("NTUSER.DAT");

      result = RegLoadKey(HKEY_USERS, profile_sid,
                          registry_hive_file.value().c_str());
      if (result != ERROR_SUCCESS) {
        LOG(ERROR) << "Error loading registry hive: " << result;
        continue;
      }

      VLOG(1) << "Loaded registry hive for " << profile_sid;
      loaded_hive = true;
    }

    base::win::RegKey user_reg_root(
        HKEY_USERS, profile_sid, KEY_ALL_ACCESS);

    LONG result = user_reg_root.DeleteKey(active_setup_path.c_str());
    if (result != ERROR_SUCCESS) {
      result = user_reg_root.DeleteKey(alternate_active_setup_path.c_str());
      if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        LOG(ERROR) << "Failed to delete key at " << active_setup_path
                   << " and at " << alternate_active_setup_path
                   << ", result: " << result;
      }
    }

    if (loaded_hive) {
      user_reg_root.Close();
      if (RegUnLoadKey(HKEY_USERS, profile_sid) == ERROR_SUCCESS)
        VLOG(1) << "Unloaded registry hive for " << profile_sid;
      else
        LOG(ERROR) << "Error unloading registry hive for " << profile_sid;
    }
  }
}

bool ProcessChromeFrameWorkItems(const InstallationState& original_state,
                                 const InstallerState& installer_state,
                                 const FilePath& setup_path,
                                 const Product& product) {
  if (!product.is_chrome_frame())
    return false;

  scoped_ptr<WorkItemList> item_list(WorkItem::CreateNoRollbackWorkItemList());
  AddChromeFrameWorkItems(original_state, installer_state, setup_path,
                          Version(), product, item_list.get());
  return item_list->Do();
}

InstallStatus UninstallProduct(const InstallationState& original_state,
                               const InstallerState& installer_state,
                               const FilePath& setup_path,
                               const Product& product,
                               bool remove_all,
                               bool force_uninstall,
                               const CommandLine& cmd_line) {
  InstallStatus status = installer::UNINSTALL_CONFIRMED;
  BrowserDistribution* browser_dist = product.distribution();
  const string16 chrome_exe(
      installer_state.target_path().Append(installer::kChromeExe).value());

  const string16 suffix(ShellUtil::GetCurrentInstallationSuffix(browser_dist,
                                                                chrome_exe));

  bool is_chrome = product.is_chrome();

  VLOG(1) << "UninstallProduct: " << browser_dist->GetAppShortCutName();

  if (force_uninstall) {
    // Since --force-uninstall command line option is used, we are going to
    // do silent uninstall. Try to close all running Chrome instances.
    // NOTE: We don't do this for Chrome Frame.
    if (is_chrome)
      CloseAllChromeProcesses();
  } else if (is_chrome) {
    // no --force-uninstall so lets show some UI dialog boxes.
    status = IsChromeActiveOrUserCancelled(installer_state, product);
    if (status != installer::UNINSTALL_CONFIRMED &&
        status != installer::UNINSTALL_DELETE_PROFILE)
      return status;

    // Check if we need admin rights to cleanup HKLM (the conditions for
    // requiring a cleanup are the same as the conditions to do the actual
    // cleanup where DeleteChromeRegistrationKeys() is invoked for
    // HKEY_LOCAL_MACHINE below). If we do, try to launch another uninstaller
    // (silent) in elevated mode to do HKLM cleanup.
    // And continue uninstalling in the current process also to do HKCU cleanup.
    if (remove_all &&
        ShellUtil::QuickIsChromeRegisteredInHKLM(
            browser_dist, chrome_exe, suffix) &&
        !::IsUserAnAdmin() &&
        base::win::GetVersion() >= base::win::VERSION_VISTA &&
        !cmd_line.HasSwitch(installer::switches::kRunAsAdmin)) {
      CommandLine new_cmd(CommandLine::NO_PROGRAM);
      new_cmd.AppendArguments(cmd_line, true);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      new_cmd.AppendSwitch(installer::switches::kRunAsAdmin);
      // Append --remove-chrome-registration to remove registry keys only.
      new_cmd.AppendSwitch(installer::switches::kRemoveChromeRegistration);
      if (!suffix.empty()) {
        new_cmd.AppendSwitchNative(
            installer::switches::kRegisterChromeBrowserSuffix, suffix);
      }
      DWORD exit_code = installer::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(new_cmd, &exit_code);
    }
  }

  if (is_chrome) {
    // Chrome is not in use so lets uninstall Chrome by deleting various files
    // and registry entries. Here we will just make best effort and keep going
    // in case of errors.
    ClearRlzProductState();
    // Delete the key that delegate_execute might make.
    if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
      InstallUtil::DeleteRegistryKey(HKEY_CURRENT_USER,
                                     chrome::kMetroRegistryPath);
    }

    auto_launch_util::DisableAllAutoStartFeatures(
        ASCIIToUTF16(chrome::kInitialProfile));

    DeleteShortcuts(installer_state, product, FilePath(chrome_exe));

  } else if (product.is_chrome_app_host()) {
    // TODO(huangs): Remove this check once we have system-level App Host.
    DCHECK(!installer_state.system_install());
    const FilePath app_host_exe(
        installer_state.target_path().Append(installer::kChromeAppHostExe));
    DeleteShortcuts(installer_state, product, app_host_exe);
  }

  // Delete the registry keys (Uninstall key and Version key).
  HKEY reg_root = installer_state.root_key();

  // Note that we must retrieve the distribution-specific data before deleting
  // product.GetVersionKey().
  string16 distribution_data(browser_dist->GetDistributionData(reg_root));

  // Remove Control Panel uninstall link.
  if (product.ShouldCreateUninstallEntry()) {
    InstallUtil::DeleteRegistryKey(reg_root,
                                   browser_dist->GetUninstallRegPath());
  }

  // Remove Omaha product key.
  InstallUtil::DeleteRegistryKey(reg_root, browser_dist->GetVersionKey());

  // Also try to delete the MSI value in the ClientState key (it might not be
  // there). This is due to a Google Update behaviour where an uninstall and a
  // rapid reinstall might result in stale values from the old ClientState key
  // being picked up on reinstall.
  product.SetMsiMarker(installer_state.system_install(), false);

  InstallStatus ret = installer::UNKNOWN_STATUS;

  if (is_chrome) {
    // Remove all Chrome registration keys.
    // Registration data is put in HKCU for both system level and user level
    // installs.
    DeleteChromeRegistrationKeys(installer_state, browser_dist,
                                 HKEY_CURRENT_USER, suffix, &ret);

    // If the user's Chrome is registered with a suffix: it is possible that old
    // unsuffixed registrations were left in HKCU (e.g. if this install was
    // previously installed with no suffix in HKCU (old suffix rules if the user
    // is not an admin (or declined UAC at first run)) and later had to be
    // suffixed when fully registered in HKLM (e.g. when later making Chrome
    // default through the UI)).
    // Remove remaining HKCU entries with no suffix if any.
    if (!suffix.empty()) {
      DeleteChromeRegistrationKeys(installer_state, browser_dist,
                                   HKEY_CURRENT_USER, string16(), &ret);

      // For similar reasons it is possible in very few installs (from
      // 21.0.1180.0 and fixed shortly after) to be installed with the new-style
      // suffix, but have some old-style suffix registrations left behind.
      string16 old_style_suffix;
      if (ShellUtil::GetOldUserSpecificRegistrySuffix(&old_style_suffix) &&
          suffix != old_style_suffix) {
        DeleteChromeRegistrationKeys(installer_state, browser_dist,
                                     HKEY_CURRENT_USER, old_style_suffix, &ret);
      }
    }

    // Chrome is registered in HKLM for all system-level installs and for
    // user-level installs for which Chrome has been made the default browser.
    // Always remove the HKLM registration for system-level installs.  For
    // user-level installs, only remove it if both: 1) this uninstall isn't a
    // self destruct following the installation of a system-level Chrome
    // (because the system-level Chrome owns the HKLM registration now), and 2)
    // this user has made Chrome their default browser (i.e. has shell
    // integration entries registered with |suffix| (note: |suffix| will be the
    // empty string if required as it is obtained by
    // GetCurrentInstallationSuffix() above)).
    // TODO(gab): This can still leave parts of a suffixed install behind. To be
    // able to remove them we would need to be able to remove only suffixed
    // entries (as it is now some of the registry entries (e.g. App Paths) are
    // unsuffixed; thus removing suffixed installs is prohibited in HKLM if
    // !|remove_all| for now).
    if (installer_state.system_install() ||
        (remove_all &&
         ShellUtil::QuickIsChromeRegisteredInHKLM(
             browser_dist, chrome_exe, suffix))) {
      DeleteChromeRegistrationKeys(installer_state, browser_dist,
                                   HKEY_LOCAL_MACHINE, suffix, &ret);
    }

    ProcessDelegateExecuteWorkItems(installer_state, product);

    ProcessOnOsUpgradeWorkItems(installer_state, product);

    UninstallActiveSetupEntries(installer_state, product);

    // Notify the shell that associations have changed since Chrome was likely
    // unregistered.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  }

  if (product.is_chrome_frame()) {
    ProcessChromeFrameWorkItems(original_state, installer_state, setup_path,
                                product);
  }

  if (installer_state.is_multi_install())
    ProcessGoogleUpdateItems(original_state, installer_state, product);

  ProcessQuickEnableWorkItems(installer_state, original_state);

  // Get the state of the installed product (if any)
  const ProductState* product_state =
      original_state.GetProductState(installer_state.system_install(),
                                     browser_dist->GetType());

  // Delete shared registry keys as well (these require admin rights) if
  // remove_all option is specified.
  if (remove_all) {
    if (!InstallUtil::IsChromeSxSProcess() && is_chrome) {
      // Delete media player registry key that exists only in HKLM.
      // We don't delete this key in SxS uninstall or Chrome Frame uninstall
      // as we never set the key for those products.
      string16 reg_path(installer::kMediaPlayerRegPath);
      reg_path.push_back(FilePath::kSeparators[0]);
      reg_path.append(installer::kChromeExe);
      InstallUtil::DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path);
    }

    // Unregister any dll servers that we may have registered for this
    // product.
    if (product_state != NULL) {
      std::vector<FilePath> com_dll_list;
      product.AddComDllList(&com_dll_list);
      FilePath dll_folder = installer_state.target_path().AppendASCII(
          product_state->version().GetString());

      scoped_ptr<WorkItemList> unreg_work_item_list(
          WorkItem::CreateWorkItemList());

      AddRegisterComDllWorkItems(dll_folder,
                                 com_dll_list,
                                 installer_state.system_install(),
                                 false,  // Unregister
                                 true,   // May fail
                                 unreg_work_item_list.get());
      unreg_work_item_list->Do();
    }

    if (product.is_chrome_frame())
      ProcessIELowRightsPolicyWorkItems(installer_state);
  }

  // Close any Chrome Frame helper processes that may be running.
  if (product.is_chrome_frame()) {
    VLOG(1) << "Closing the Chrome Frame helper process";
    CloseChromeFrameHelperProcess();
  }

  if (product_state == NULL)
    return installer::UNINSTALL_SUCCESSFUL;

  // Finally delete all the files from Chrome folder after moving setup.exe
  // and the user's Local State to a temp location.
  bool delete_profile = ShouldDeleteProfile(installer_state, cmd_line, status,
                                            product);
  ret = installer::UNINSTALL_SUCCESSFUL;

  // When deleting files, we must make sure that we're either a "single"
  // (aka non-multi) installation or we are the Chrome Binaries.

  std::vector<FilePath> local_state_folders;
  GetLocalStateFolders(product, &local_state_folders);
  FilePath backup_state_file(BackupLocalStateFile(local_state_folders));

  if (product.is_chrome_app_host()) {
    DeleteAppHostFilesAndFolders(installer_state, product_state->version());
  } else if (!installer_state.is_multi_install() ||
             product.is_chrome_binaries()) {
    DeleteResult delete_result = DeleteChromeFilesAndFolders(
        installer_state, cmd_line.GetProgram());
    if (delete_result == DELETE_FAILED) {
      ret = installer::UNINSTALL_FAILED;
    } else if (delete_result == DELETE_REQUIRES_REBOOT) {
      ret = installer::UNINSTALL_REQUIRES_REBOOT;
    }
  }

  if (delete_profile)
    DeleteLocalState(local_state_folders, product.is_chrome_frame());

  if (!force_uninstall) {
    VLOG(1) << "Uninstallation complete. Launching post-uninstall operations.";
    browser_dist->DoPostUninstallOperations(product_state->version(),
        backup_state_file, distribution_data);
  }

  // Try and delete the preserved local state once the post-install
  // operations are complete.
  if (!backup_state_file.empty())
    file_util::Delete(backup_state_file, false);

  // If user-level Chrome is being uninstalled and system-level Chrome is
  // present, launch the system-level Active Setup command to do post-install
  // tasks for this user (i.e., create shortcuts).
  if (product.is_chrome() && !installer_state.system_install() &&
      original_state.GetProductState(true, browser_dist->GetType())) {
    InstallUtil::TriggerActiveSetupCommand();
  }

  return ret;
}

void CleanUpInstallationDirectoryAfterUninstall(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const CommandLine& cmd_line,
    installer::InstallStatus* uninstall_status) {
  if (*uninstall_status != installer::UNINSTALL_SUCCESSFUL &&
      *uninstall_status != installer::UNINSTALL_REQUIRES_REBOOT) {
    return;
  }
  const FilePath target_path(installer_state.target_path());
  if (target_path.empty()) {
    LOG(ERROR) << "No installation destination path.";
    *uninstall_status = installer::UNINSTALL_FAILED;
    return;
  }
  FilePath setup_exe(cmd_line.GetProgram());
  file_util::AbsolutePath(&setup_exe);
  if (!target_path.IsParent(setup_exe)) {
    LOG(INFO) << "setup.exe is not in target path. Skipping installer cleanup.";
    return;
  }
  FilePath install_directory(setup_exe.DirName());

  bool remove_setup = true;
  bool remove_archive = true;
  CheckShouldRemoveSetupAndArchive(original_state, installer_state,
                                   &remove_setup, &remove_archive);
  if (!remove_archive)
    return;

  if (remove_setup) {
    // In order to be able to remove the folder in which we're running, we
    // need to move setup.exe out of the install folder.
    // TODO(tommi): What if the temp folder is on a different volume?
    MoveSetupOutOfInstallFolder(installer_state, setup_exe);
  }

  // Remove files from "...\<product>\Application\<version>\Installer"
  if (!RemoveInstallerFiles(install_directory, remove_setup)) {
    *uninstall_status = installer::UNINSTALL_FAILED;
    return;
  }

  if (!remove_setup)
    return;

  // Try to remove the empty directory hierarchy.

  // Delete "...\<product>\Application\<version>\Installer"
  if (DeleteEmptyDir(install_directory) != DELETE_SUCCEEDED) {
    *uninstall_status = installer::UNINSTALL_FAILED;
    return;
  }

  // Delete "...\<product>\Application\<version>"
  DeleteResult delete_result = DeleteEmptyDir(install_directory.DirName());
  if (delete_result == DELETE_FAILED ||
      (delete_result == DELETE_NOT_EMPTY &&
       *uninstall_status != installer::UNINSTALL_REQUIRES_REBOOT)) {
    *uninstall_status = installer::UNINSTALL_FAILED;
    return;
  }

  if (*uninstall_status == installer::UNINSTALL_REQUIRES_REBOOT) {
    // Delete the Application directory at reboot if empty.
    ScheduleFileSystemEntityForDeletion(target_path.value().c_str());

    // If we need a reboot to continue, schedule the parent directories for
    // deletion unconditionally. If they are not empty, the session manager
    // will not delete them on reboot.
    ScheduleParentAndGrandparentForDeletion(target_path);
  } else if (DeleteApplicationProductAndVendorDirectories(target_path) ==
             installer::DELETE_FAILED) {
    *uninstall_status = installer::UNINSTALL_FAILED;
  }
}

}  // namespace installer
