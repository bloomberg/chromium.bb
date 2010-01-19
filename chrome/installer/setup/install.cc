// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>
#include <time.h>

#include "chrome/installer/setup/install.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

// Build-time generated include file.
#include "installer_util_strings.h"
#include "registered_dlls.h"

namespace {

std::wstring AppendPath(const std::wstring& parent_path,
                        const std::wstring& path) {
  std::wstring new_path(parent_path);
  file_util::AppendToPath(&new_path, path);
  return new_path;
}

void AddChromeToMediaPlayerList() {
  std::wstring reg_path(installer::kMediaPlayerRegPath);
  // registry paths can also be appended like file system path
  file_util::AppendToPath(&reg_path, installer_util::kChromeExe);
  LOG(INFO) << "Adding Chrome to Media player list at " << reg_path;
  scoped_ptr<WorkItem> work_item(WorkItem::CreateCreateRegKeyWorkItem(
      HKEY_LOCAL_MACHINE, reg_path));

  // if the operation fails we log the error but still continue
  if (!work_item.get()->Do())
    LOG(ERROR) << "Could not add Chrome to media player inclusion list.";
}

void AddInstallerCopyTasks(const std::wstring& exe_path,
                           const std::wstring& archive_path,
                           const std::wstring& temp_path,
                           const std::wstring& install_path,
                           const std::wstring& new_version,
                           WorkItemList* install_list,
                           bool system_level) {
  std::wstring installer_dir(installer::GetInstallerPathUnderChrome(
      install_path, new_version));
  install_list->AddCreateDirWorkItem(
      FilePath::FromWStringHack(installer_dir));

  std::wstring exe_dst(installer_dir);
  std::wstring archive_dst(installer_dir);
  file_util::AppendToPath(&exe_dst,
      file_util::GetFilenameFromPath(exe_path));
  file_util::AppendToPath(&archive_dst,
      file_util::GetFilenameFromPath(archive_path));

  install_list->AddCopyTreeWorkItem(exe_path, exe_dst, temp_path,
                                    WorkItem::ALWAYS);
  if (system_level) {
    install_list->AddCopyTreeWorkItem(archive_path, archive_dst, temp_path,
                                      WorkItem::ALWAYS);
  } else {
    install_list->AddMoveTreeWorkItem(archive_path, archive_dst, temp_path);
  }
}

void AppendUninstallCommandLineFlags(std::wstring* uninstall_cmd_line,
                                     bool is_system) {
  DCHECK(uninstall_cmd_line);
  uninstall_cmd_line->append(L" --");
  uninstall_cmd_line->append(installer_util::switches::kUninstall);

  if (InstallUtil::IsChromeFrameProcess()) {
    uninstall_cmd_line->append(L" --");
    uninstall_cmd_line->append(installer_util::switches::kDeleteProfile);
    uninstall_cmd_line->append(L" --");
    uninstall_cmd_line->append(installer_util::switches::kChromeFrame);
  }

  // Propagate the verbose logging switch to uninstalls too.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(installer_util::switches::kVerboseLogging)) {
    uninstall_cmd_line->append(L" --");
    uninstall_cmd_line->append(installer_util::switches::kVerboseLogging);
  }

  if (is_system) {
    uninstall_cmd_line->append(L" --");
    uninstall_cmd_line->append(installer_util::switches::kSystemLevel);
  }
}

// This method adds work items to create (or update) Chrome uninstall entry in
// Control Panel->Add/Remove Programs list.
void AddUninstallShortcutWorkItems(HKEY reg_root,
                                   const std::wstring& exe_path,
                                   const std::wstring& install_path,
                                   const std::wstring& product_name,
                                   const std::wstring& new_version,
                                   WorkItemList* install_list) {
  std::wstring uninstall_cmd(L"\"");
  uninstall_cmd.append(installer::GetInstallerPathUnderChrome(install_path,
                                                              new_version));
  file_util::AppendToPath(&uninstall_cmd,
                          file_util::GetFilenameFromPath(exe_path));
  uninstall_cmd.append(L"\"");

  AppendUninstallCommandLineFlags(&uninstall_cmd,
                                  reg_root == HKEY_LOCAL_MACHINE);

  // Create DisplayName, UninstallString and InstallLocation keys
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring uninstall_reg = dist->GetUninstallRegPath();
  install_list->AddCreateRegKeyWorkItem(reg_root, uninstall_reg);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
      installer_util::kUninstallDisplayNameField, product_name, true);
  install_list->AddSetRegValueWorkItem(reg_root,
                                       uninstall_reg,
                                       installer_util::kUninstallStringField,
                                       uninstall_cmd, true);
  install_list->AddSetRegValueWorkItem(reg_root,
                                       uninstall_reg,
                                       L"InstallLocation", install_path, true);

  // DisplayIcon, NoModify and NoRepair
  std::wstring chrome_icon = AppendPath(install_path,
                                        installer_util::kChromeExe);
  ShellUtil::GetChromeIcon(chrome_icon);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"DisplayIcon", chrome_icon, true);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"NoModify", 1, true);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"NoRepair", 1, true);

  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"Publisher",
                                       dist->GetPublisherName(), true);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"Version", new_version.c_str(), true);
  install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                       L"DisplayVersion",
                                       new_version.c_str(), true);
  time_t rawtime = time(NULL);
  struct tm timeinfo = {0};
  localtime_s(&timeinfo, &rawtime);
  wchar_t buffer[9];
  if (wcsftime(buffer, 9, L"%Y%m%d", &timeinfo) == 8) {
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"InstallDate",
                                         buffer, false);
  }
}

// Copy master preferences file provided to installer, in the same folder
// as chrome.exe so Chrome first run can find it. This function will be called
// only on the first install of Chrome.
void CopyPreferenceFileForFirstRun(bool system_level,
                                   const std::wstring& prefs_source_path) {
  FilePath prefs_dest_path = FilePath::FromWStringHack(
      installer::GetChromeInstallPath(system_level));
  prefs_dest_path = prefs_dest_path.Append(installer_util::kDefaultMasterPrefs);
  if (!file_util::CopyFile(FilePath::FromWStringHack(prefs_source_path),
                           prefs_dest_path)) {
    LOG(INFO) << "Failed to copy master preferences.";
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
bool CreateOrUpdateChromeShortcuts(const std::wstring& exe_path,
                                   const std::wstring& install_path,
                                   const std::wstring& new_version,
                                   installer_util::InstallStatus install_status,
                                   bool system_install,
                                   bool create_all_shortcut,
                                   bool alt_shortcut) {
  FilePath shortcut_path;
  int dir_enum = (system_install) ? base::DIR_COMMON_START_MENU :
                                    base::DIR_START_MENU;
  if (!PathService::Get(dir_enum, &shortcut_path)) {
    LOG(ERROR) << "Failed to get location for shortcut.";
    return false;
  }

  // The location of Start->Programs->Google Chrome folder
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  const std::wstring& product_name = dist->GetApplicationName();
  const std::wstring& product_desc = dist->GetAppDescription();
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
  std::wstring chrome_exe(install_path);  // Chrome link target
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);

  if ((install_status == installer_util::FIRST_INSTALL_SUCCESS) ||
      (install_status == installer_util::INSTALL_REPAIRED)) {
    if (!file_util::PathExists(shortcut_path))
      file_util::CreateDirectoryW(shortcut_path);

    LOG(INFO) << "Creating shortcut to " << chrome_exe << " at "
              << chrome_link.value();
    ret = ret && ShellUtil::UpdateChromeShortcut(chrome_exe,
                                                 chrome_link.value(),
                                                 product_desc, true);
  } else if (file_util::PathExists(chrome_link)) {
    LOG(INFO) << "Updating shortcut at " << chrome_link.value()
              << " to point to " << chrome_exe;
    ret = ret && ShellUtil::UpdateChromeShortcut(chrome_exe,
                                                 chrome_link.value(),
                                                 product_desc, false);
  }

  // Create/update uninstall link
  FilePath uninstall_link(shortcut_path);  // Uninstall Chrome link
  uninstall_link = uninstall_link.Append(
      dist->GetUninstallLinkName() + L".lnk");
  if ((install_status == installer_util::FIRST_INSTALL_SUCCESS) ||
      (install_status == installer_util::INSTALL_REPAIRED) ||
      (file_util::PathExists(uninstall_link))) {
    if (!file_util::PathExists(shortcut_path))
      file_util::CreateDirectoryW(shortcut_path);
    std::wstring setup_exe(installer::GetInstallerPathUnderChrome(install_path,
                                                                  new_version));
    file_util::AppendToPath(&setup_exe,
                            file_util::GetFilenameFromPath(exe_path));

    std::wstring arguments;
    AppendUninstallCommandLineFlags(&arguments, system_install);

    LOG(INFO) << "Creating/updating uninstall link at "
              << uninstall_link.value();
    ret = ret && file_util::CreateShortcutLink(setup_exe.c_str(),
                                               uninstall_link.value().c_str(),
                                               NULL,
                                               arguments.c_str(),
                                               NULL, setup_exe.c_str(), 0,
                                               NULL);
  }

  // Update Desktop and Quick Launch shortcuts. If --create-new-shortcuts
  // is specified we want to create them, otherwise we update them only if
  // they exist.
  if (system_install) {
    ret = ret && ShellUtil::CreateChromeDesktopShortcut(chrome_exe,
        product_desc, ShellUtil::SYSTEM_LEVEL, alt_shortcut,
        create_all_shortcut);
    ret = ret && ShellUtil::CreateChromeQuickLaunchShortcut(chrome_exe,
        ShellUtil::CURRENT_USER | ShellUtil::SYSTEM_LEVEL, create_all_shortcut);
  } else {
    ret = ret && ShellUtil::CreateChromeDesktopShortcut(chrome_exe,
        product_desc, ShellUtil::CURRENT_USER, alt_shortcut,
        create_all_shortcut);
    ret = ret && ShellUtil::CreateChromeQuickLaunchShortcut(chrome_exe,
        ShellUtil::CURRENT_USER, create_all_shortcut);
  }

  return ret;
}

// After a successful copying of all the files, this function is called to
// do a few post install tasks:
// - Handle the case of in-use-update by updating "opv" key or deleting it if
//   not required.
// - Register any new dlls and unregister old dlls.
// If these operations are successful, the function returns true, otherwise
// false.
bool DoPostInstallTasks(HKEY reg_root,
                        const std::wstring& exe_path,
                        const std::wstring& install_path,
                        const std::wstring& new_chrome_exe,
                        const std::wstring& current_version,
                        const installer::Version& new_version) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring version_key = dist->GetVersionKey();

  if (file_util::PathExists(FilePath::FromWStringHack(new_chrome_exe))) {
    // Looks like this was in use update. So make sure we update the 'opv' key
    // with the current version that is active and 'cmd' key with the rename
    // command to run.
    if (current_version.empty()) {
      LOG(ERROR) << "New chrome.exe exists but current version is empty!";
      return false;
    }
    scoped_ptr<WorkItemList> inuse_list(WorkItem::CreateWorkItemList());
    inuse_list->AddSetRegValueWorkItem(reg_root,
                                       version_key,
                                       google_update::kRegOldVersionField,
                                       current_version.c_str(),
                                       true);

    std::wstring rename_cmd(installer::GetInstallerPathUnderChrome(
        install_path, new_version.GetString()));
    file_util::AppendToPath(&rename_cmd,
                            file_util::GetFilenameFromPath(exe_path));
    rename_cmd = L"\"" + rename_cmd +
                 L"\" --" + installer_util::switches::kRenameChromeExe;
    if (reg_root == HKEY_LOCAL_MACHINE)
      rename_cmd = rename_cmd + L" --" + installer_util::switches::kSystemLevel;

    if (InstallUtil::IsChromeFrameProcess()) {
      rename_cmd += L" --";
      rename_cmd += installer_util::switches::kChromeFrame;
    }

    inuse_list->AddSetRegValueWorkItem(reg_root,
                                       version_key,
                                       google_update::kRegRenameCmdField,
                                       rename_cmd.c_str(),
                                       true);
    if (!inuse_list->Do()) {
      LOG(ERROR) << "Couldn't write opv/cmd values to registry.";
      inuse_list->Rollback();
      return false;
    }
  } else {
    // Since this was not in-use-update, delete 'opv' and 'cmd' keys.
    scoped_ptr<WorkItemList> inuse_list(WorkItem::CreateWorkItemList());
    inuse_list->AddDeleteRegValueWorkItem(reg_root, version_key,
                                          google_update::kRegOldVersionField,
                                          true);
    inuse_list->AddDeleteRegValueWorkItem(reg_root, version_key,
                                          google_update::kRegRenameCmdField,
                                          true);
    if (!inuse_list->Do()) {
      LOG(ERROR) << "Couldn't delete opv/cmd values from registry.";
      inuse_list->Rollback();
      return false;
    }
  }

  if (InstallUtil::IsChromeFrameProcess()) {
    // Chrome Frame instances of setup.exe should always have at least
    // one DLL to register. Enforce that this is so.
    if (kNumDllsToRegister <= 0) {
      NOTREACHED();
      return false;
    }

    // Now we need to register any self registering components and unregister
    // any that were left from the old version that is being upgraded:
    if (!current_version.empty()) {
      std::wstring old_dll_path(install_path);
      file_util::AppendToPath(&old_dll_path, current_version);
      scoped_ptr<WorkItemList> old_dll_list(WorkItem::CreateWorkItemList());
      if (InstallUtil::BuildDLLRegistrationList(old_dll_path, kDllsToRegister,
                                                kNumDllsToRegister, false,
                                                old_dll_list.get())) {
        // Don't abort the install as a result of a failure to unregister old
        // DLLs.
        old_dll_list->Do();
      }
    }

    std::wstring dll_path(install_path);
    file_util::AppendToPath(&dll_path, new_version.GetString());
    scoped_ptr<WorkItemList> dll_list(WorkItem::CreateWorkItemList());
    if (InstallUtil::BuildDLLRegistrationList(dll_path, kDllsToRegister,
                                              kNumDllsToRegister, true,
                                              dll_list.get())) {
      if (!dll_list->Do()) {
        dll_list->Rollback();
        return false;
      }
    }
  }

  return true;
}

// This method tells if we are running on 64 bit platform so that we can copy
// one extra exe. If the API call to determine 64 bit fails, we play it safe
// and return true anyway so that the executable can be copied.
bool Is64bit() {
  typedef BOOL (WINAPI *WOW_FUNC)(HANDLE, PBOOL);
  BOOL is64 = FALSE;

  HANDLE handle = GetCurrentProcess();
  HMODULE module = GetModuleHandle(L"kernel32.dll");
  WOW_FUNC p = reinterpret_cast<WOW_FUNC>(GetProcAddress(module,
                                                         "IsWow64Process"));
  if ((p != NULL) && (!(p)(handle, &is64) || (is64 != FALSE))) {
    return true;
  }

  return false;
}

void RegisterChromeOnMachine(const std::wstring& install_path,
                             bool system_level,
                             bool make_chrome_default) {
  // Try to add Chrome to Media Player shim inclusion list. We don't do any
  // error checking here because this operation will fail if user doesn't
  // have admin rights and we want to ignore the error.
  AddChromeToMediaPlayerList();

  // Is --make-chrome-default option is given we make Chrome default browser
  // otherwise we only register it on the machine as a valid browser.
  std::wstring chrome_exe(install_path);
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
  LOG(INFO) << "Registering Chrome as browser";
  if (make_chrome_default) {
    int level = ShellUtil::CURRENT_USER;
    if (system_level)
      level = level | ShellUtil::SYSTEM_LEVEL;
    ShellUtil::MakeChromeDefault(level, chrome_exe, true);
  } else {
    ShellUtil::RegisterChromeBrowser(chrome_exe, L"", false);
  }
}

// This function installs a new version of Chrome to the specified location.
//
// exe_path: Path to the executable (setup.exe) as it will be copied
//           to Chrome install folder after install is complete
// archive_path: Path to the archive (chrome.7z) as it will be copied
//               to Chrome install folder after install is complete
// src_path: the path that contains a complete and unpacked Chrome package
//           to be installed.
// install_path: the destination path for Chrome to be installed to. This
//               path does not need to exist.
// temp_dir: the path of working directory used during installation. This path
//           does not need to exist.
// reg_root: the root of registry where the function applies settings for the
//           new Chrome version. It should be either HKLM or HKCU.
// new_version: new Chrome version that needs to be installed
// current_version: returns the current active version (if any)
//
// This function makes best effort to do installation in a transactional
// manner. If failed it tries to rollback all changes on the file system
// and registry. For example, if install_path exists before calling the
// function, it rolls back all new file and directory changes under
// install_path. If install_path does not exist before calling the function
// (typical new install), the function creates install_path during install
// and removes the whole directory during rollback.
installer_util::InstallStatus InstallNewVersion(
    const std::wstring& exe_path,
    const std::wstring& archive_path,
    const std::wstring& src_path,
    const std::wstring& install_path,
    const std::wstring& temp_dir,
    const HKEY reg_root,
    const installer::Version& new_version,
    std::wstring* current_version) {
  if (reg_root != HKEY_LOCAL_MACHINE && reg_root != HKEY_CURRENT_USER)
    return installer_util::INSTALL_FAILED;

  if (InstallUtil::IsChromeFrameProcess()) {
    // Make sure that we don't end up deleting installed files on next reboot.
    if (!RemoveFromMovesPendingReboot(install_path.c_str())) {
      LOG(ERROR) << "Error accessing pending moves value.";
    }
  }

  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  // A temp directory that work items need and the actual install directory.
  install_list->AddCreateDirWorkItem(FilePath::FromWStringHack(temp_dir));
  install_list->AddCreateDirWorkItem(FilePath::FromWStringHack(install_path));

  // Delete any new_chrome.exe if present (we will end up creating a new one
  // if required) and then copy chrome.exe
  std::wstring new_chrome_exe = AppendPath(install_path,
                                           installer_util::kChromeNewExe);
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  RegKey chrome_key(reg_root, dist->GetVersionKey().c_str(), KEY_READ);
  if (file_util::PathExists(FilePath::FromWStringHack(new_chrome_exe)))
    chrome_key.ReadValue(google_update::kRegOldVersionField, current_version);
  if (current_version->empty())
    chrome_key.ReadValue(google_update::kRegVersionField, current_version);
  chrome_key.Close();

  install_list->AddDeleteTreeWorkItem(new_chrome_exe, std::wstring());
  install_list->AddCopyTreeWorkItem(
      AppendPath(src_path, installer_util::kChromeExe),
      AppendPath(install_path, installer_util::kChromeExe),
      temp_dir, WorkItem::NEW_NAME_IF_IN_USE, new_chrome_exe);

  // Extra executable for 64 bit systems.
  if (Is64bit()) {
    install_list->AddCopyTreeWorkItem(
        AppendPath(src_path, installer::kWowHelperExe),
        AppendPath(install_path, installer::kWowHelperExe),
        temp_dir, WorkItem::ALWAYS);
  }

  // If it is system level install copy the version folder (since we want to
  // take the permissions of %ProgramFiles% folder) otherwise just move it.
  if (reg_root == HKEY_LOCAL_MACHINE) {
    install_list->AddCopyTreeWorkItem(
        AppendPath(src_path, new_version.GetString()),
        AppendPath(install_path, new_version.GetString()),
        temp_dir, WorkItem::ALWAYS);
  } else {
    install_list->AddMoveTreeWorkItem(
        AppendPath(src_path, new_version.GetString()),
        AppendPath(install_path, new_version.GetString()),
        temp_dir);
  }

  // Copy the default Dictionaries only if the folder doesnt exist already
  install_list->AddCopyTreeWorkItem(
      AppendPath(src_path, installer::kDictionaries),
      AppendPath(install_path, installer::kDictionaries),
      temp_dir, WorkItem::IF_NOT_PRESENT);

  // Copy installer in install directory and
  // add shortcut in Control Panel->Add/Remove Programs.
  AddInstallerCopyTasks(exe_path, archive_path, temp_dir, install_path,
                        new_version.GetString(), install_list.get(),
                        (reg_root == HKEY_LOCAL_MACHINE));
  std::wstring product_name = dist->GetApplicationName();
  AddUninstallShortcutWorkItems(reg_root, exe_path, install_path,
      product_name, new_version.GetString(), install_list.get());

  // Delete any old_chrome.exe if present.
  install_list->AddDeleteTreeWorkItem(
      AppendPath(install_path, installer_util::kChromeOldExe), std::wstring());

  // Create Version key (if not already present) and set the new Chrome
  // version as last step.
  std::wstring version_key = dist->GetVersionKey();
  install_list->AddCreateRegKeyWorkItem(reg_root, version_key);
  install_list->AddSetRegValueWorkItem(reg_root, version_key,
                                       google_update::kRegNameField,
                                       product_name,
                                       true);    // overwrite name also
  install_list->AddSetRegValueWorkItem(reg_root, version_key,
                                       google_update::kRegVersionField,
                                       new_version.GetString(),
                                       true);    // overwrite version

  installer_util::InstallStatus result = installer_util::INSTALL_FAILED;
  if (!install_list->Do() ||
      !DoPostInstallTasks(reg_root, exe_path, install_path,
                          new_chrome_exe, *current_version, new_version)) {
    if (file_util::PathExists(FilePath::FromWStringHack(new_chrome_exe)) &&
        !current_version->empty() &&
        (new_version.GetString() == *current_version))
      result = installer_util::SAME_VERSION_REPAIR_FAILED;
    LOG(ERROR) << "Install failed, rolling back... ";
    install_list->Rollback();
    LOG(ERROR) << "Rollback complete. ";
  } else {
    scoped_ptr<installer::Version> installed_version;
    if (!current_version->empty())
      installed_version.reset(
          installer::Version::GetVersionFromString(*current_version));
    if (!installed_version.get()) {
      LOG(INFO) << "First install of version " << new_version.GetString();
      result = installer_util::FIRST_INSTALL_SUCCESS;
    } else if (new_version.GetString() == installed_version->GetString()) {
      LOG(INFO) << "Install repaired of version " << new_version.GetString();
      result = installer_util::INSTALL_REPAIRED;
    } else if (new_version.IsHigherThan(installed_version.get())) {
      if (file_util::PathExists(FilePath::FromWStringHack(new_chrome_exe))) {
        LOG(INFO) << "Version updated to " << new_version.GetString()
                  << " while running " << installed_version->GetString();
        result = installer_util::IN_USE_UPDATED;
      } else {
        LOG(INFO) << "Version updated to " << new_version.GetString();
        result = installer_util::NEW_VERSION_UPDATED;
      }
    } else {
      LOG(ERROR) << "Not sure how we got here while updating"
                 << ", new version: " << new_version.GetString()
                 << ", old version: " << installed_version->GetString();
    }
  }
  return result;
}

}  // namespace

std::wstring installer::GetInstallerPathUnderChrome(
    const std::wstring& install_path, const std::wstring& new_version) {
  std::wstring installer_path(install_path);
  file_util::AppendToPath(&installer_path, new_version);
  file_util::AppendToPath(&installer_path, installer_util::kInstallerDir);
  return installer_path;
}

installer_util::InstallStatus installer::InstallOrUpdateChrome(
    const std::wstring& exe_path, const std::wstring& archive_path,
    const std::wstring& install_temp_path, const std::wstring& prefs_path,
    const DictionaryValue* prefs, const Version& new_version,
    const Version* installed_version) {
  bool system_install = false;
  installer_util::GetDistroBooleanPreference(prefs,
      installer_util::master_preferences::kSystemLevel, &system_install);
  std::wstring install_path(GetChromeInstallPath(system_install));
  if (install_path.empty()) {
    LOG(ERROR) << "Could not get installation destination path.";
    return installer_util::INSTALL_FAILED;
  } else {
    LOG(INFO) << "install destination path: " << install_path;
  }

  std::wstring src_path(install_temp_path);
  file_util::AppendToPath(&src_path, std::wstring(kInstallSourceDir));
  file_util::AppendToPath(&src_path, std::wstring(kInstallSourceChromeDir));

  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring current_version;
  installer_util::InstallStatus result = InstallNewVersion(exe_path,
      archive_path, src_path, install_path, install_temp_path, reg_root,
      new_version, &current_version);

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!dist->GetInstallReturnCode(result)) {
    if (result == installer_util::FIRST_INSTALL_SUCCESS)
      CopyPreferenceFileForFirstRun(system_install, prefs_path);

    bool value = false;
    if (!installer_util::GetDistroBooleanPreference(prefs,
        installer_util::master_preferences::kDoNotCreateShortcuts, &value) ||
        !value) {
      bool create_all_shortcut = false;
      installer_util::GetDistroBooleanPreference(prefs,
          installer_util::master_preferences::kCreateAllShortcuts,
          &create_all_shortcut);
      bool alt_shortcut = false;
      installer_util::GetDistroBooleanPreference(prefs,
          installer_util::master_preferences::kAltShortcutText,
          &alt_shortcut);
      if (!CreateOrUpdateChromeShortcuts(exe_path, install_path,
                                         new_version.GetString(), result,
                                         system_install, create_all_shortcut,
                                         alt_shortcut))
        LOG(WARNING) << "Failed to create/update start menu shortcut.";

      bool make_chrome_default = false;
      installer_util::GetDistroBooleanPreference(prefs,
          installer_util::master_preferences::kMakeChromeDefault,
          &make_chrome_default);
      RegisterChromeOnMachine(install_path, system_install,
                              make_chrome_default);
    }

    std::wstring latest_version_to_keep(new_version.GetString());
    if (!current_version.empty())
      latest_version_to_keep.assign(current_version);
    RemoveOldVersionDirs(install_path, latest_version_to_keep);
  }

  return result;
}
