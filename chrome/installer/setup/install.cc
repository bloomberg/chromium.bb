// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install.h"

#include <shlobj.h>
#include <time.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/package.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

// Build-time generated include file.
#include "installer_util_strings.h"  // NOLINT
#include "registered_dlls.h"  // NOLINT

namespace {

using installer::Products;
using installer::Product;
using installer::Package;
using installer::Version;

void AddChromeToMediaPlayerList() {
  std::wstring reg_path(installer::kMediaPlayerRegPath);
  // registry paths can also be appended like file system path
  file_util::AppendToPath(&reg_path, installer_util::kChromeExe);
  VLOG(1) << "Adding Chrome to Media player list at " << reg_path;
  scoped_ptr<WorkItem> work_item(WorkItem::CreateCreateRegKeyWorkItem(
      HKEY_LOCAL_MACHINE, reg_path));

  // if the operation fails we log the error but still continue
  if (!work_item.get()->Do())
    LOG(ERROR) << "Could not add Chrome to media player inclusion list.";
}

void AddInstallerCopyTasks(const FilePath& setup_path,
                           const FilePath& archive_path,
                           const FilePath& temp_path,
                           const Version& new_version,
                           WorkItemList* install_list,
                           const Package& package) {
  DCHECK(install_list);
  FilePath installer_dir(package.GetInstallerDirectory(new_version));
  install_list->AddCreateDirWorkItem(installer_dir);

  FilePath exe_dst(installer_dir.Append(setup_path.BaseName()));
  FilePath archive_dst(installer_dir.Append(archive_path.BaseName()));

  install_list->AddCopyTreeWorkItem(setup_path.value(), exe_dst.value(),
                                    temp_path.value(), WorkItem::ALWAYS);
  if (package.system_level()) {
    install_list->AddCopyTreeWorkItem(archive_path.value(), archive_dst.value(),
                                      temp_path.value(), WorkItem::ALWAYS);
  } else {
    install_list->AddMoveTreeWorkItem(archive_path.value(), archive_dst.value(),
                                      temp_path.value());
  }
}

void AppendUninstallCommandLineFlags(CommandLine* uninstall_cmd,
                                     const Product& product) {
  DCHECK(uninstall_cmd);

  uninstall_cmd->AppendSwitch(installer_util::switches::kUninstall);

  const installer_util::MasterPreferences& prefs =
      InstallUtil::GetMasterPreferencesForCurrentProcess();

  bool cf_switch_added = false;

  if (prefs.is_multi_install()) {
    uninstall_cmd->AppendSwitch(installer_util::switches::kMultiInstall);
    switch (product.distribution()->GetType()) {
      case BrowserDistribution::CHROME_BROWSER:
        uninstall_cmd->AppendSwitch(installer_util::switches::kChrome);
        break;
      case BrowserDistribution::CHROME_FRAME:
        uninstall_cmd->AppendSwitch(installer_util::switches::kChromeFrame);
        cf_switch_added = true;
        break;
      case BrowserDistribution::CEEE:
        uninstall_cmd->AppendSwitch(installer_util::switches::kCeee);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  if (product.distribution()->GetType() == BrowserDistribution::CHROME_FRAME) {
    DCHECK(prefs.install_chrome_frame());
    uninstall_cmd->AppendSwitch(installer_util::switches::kDeleteProfile);
    if (!cf_switch_added) {
      uninstall_cmd->AppendSwitch(installer_util::switches::kChromeFrame);
    }
  }

  if (InstallUtil::IsChromeSxSProcess())
    uninstall_cmd->AppendSwitch(installer_util::switches::kChromeSxS);

  if (product.IsMsi())
    uninstall_cmd->AppendSwitch(installer_util::switches::kMsi);

  // Propagate the verbose logging switch to uninstalls too.
  bool value = false;
  if (prefs.GetBool(installer_util::master_preferences::kVerboseLogging,
                    &value) && value)
    uninstall_cmd->AppendSwitch(installer_util::switches::kVerboseLogging);

  if (product.system_level())
    uninstall_cmd->AppendSwitch(installer_util::switches::kSystemLevel);
}

// This method adds work items to create (or update) Chrome uninstall entry in
// either the Control Panel->Add/Remove Programs list or in the Omaha client
// state key if running under an MSI installer.
void AddUninstallShortcutWorkItems(const FilePath& setup_path,
                                   const Version& new_version,
                                   WorkItemList* install_list,
                                   const Product& product) {
  HKEY reg_root = product.system_level() ? HKEY_LOCAL_MACHINE :
                                           HKEY_CURRENT_USER;
  BrowserDistribution* browser_dist = product.distribution();
  DCHECK(browser_dist);

  // When we are installed via an MSI, we need to store our uninstall strings
  // in the Google Update client state key. We do this even for non-MSI
  // managed installs to avoid breaking the edge case whereby an MSI-managed
  // install is updated by a non-msi installer (which would confuse the MSI
  // machinery if these strings were not also updated).
  // Do not quote the command line for the MSI invocation.
  FilePath install_path(product.package().path());
  FilePath installer_path(
      product.package().GetInstallerDirectory(new_version));
  installer_path = installer_path.Append(setup_path.BaseName());

  CommandLine uninstall_arguments(CommandLine::NO_PROGRAM);
  AppendUninstallCommandLineFlags(&uninstall_arguments, product);

  std::wstring update_state_key(browser_dist->GetStateKey());
  install_list->AddCreateRegKeyWorkItem(reg_root, update_state_key);
  install_list->AddSetRegValueWorkItem(reg_root, update_state_key,
      installer_util::kUninstallStringField, installer_path.value(), true);
  install_list->AddSetRegValueWorkItem(reg_root, update_state_key,
      installer_util::kUninstallArgumentsField,
      uninstall_arguments.command_line_string(), true);

  // MSI installations will manage their own uninstall shortcuts.
  if (!product.IsMsi()) {
    // We need to quote the command line for the Add/Remove Programs dialog.
    CommandLine quoted_uninstall_cmd(installer_path);
    DCHECK_EQ(quoted_uninstall_cmd.command_line_string()[0], '"');
    quoted_uninstall_cmd.AppendArguments(uninstall_arguments, false);

    std::wstring uninstall_reg = browser_dist->GetUninstallRegPath();
    install_list->AddCreateRegKeyWorkItem(reg_root, uninstall_reg);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
        installer_util::kUninstallDisplayNameField,
        browser_dist->GetAppShortCutName(), true);
    install_list->AddSetRegValueWorkItem(reg_root,
        uninstall_reg, installer_util::kUninstallStringField,
        quoted_uninstall_cmd.command_line_string(), true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         L"InstallLocation",
                                         install_path.value(),
                                         true);

    // DisplayIcon, NoModify and NoRepair
    FilePath chrome_icon(install_path.Append(installer_util::kChromeExe));
    ShellUtil::GetChromeIcon(product.distribution(), chrome_icon.value());
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"DisplayIcon", chrome_icon.value(),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"NoModify", 1, true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"NoRepair", 1, true);

    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"Publisher",
                                         browser_dist->GetPublisherName(),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"Version", new_version.GetString(),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"DisplayVersion",
                                         new_version.GetString(), true);
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
}

// This is called when an MSI installation is run. It may be that a user is
// attempting to install the MSI on top of a non-MSI managed installation.
// If so, try and remove any existing uninstallation shortcuts, as we want the
// uninstall to be managed entirely by the MSI machinery (accessible via the
// Add/Remove programs dialog).
void DeleteUninstallShortcutsForMSI(const Product& product) {
  DCHECK(product.IsMsi()) << "This must only be called for MSI installations!";

  // First attempt to delete the old installation's ARP dialog entry.
  HKEY reg_root = product.system_level() ? HKEY_LOCAL_MACHINE :
                                           HKEY_CURRENT_USER;
  base::win::RegKey root_key(reg_root, L"", KEY_ALL_ACCESS);
  std::wstring uninstall_reg(product.distribution()->GetUninstallRegPath());
  InstallUtil::DeleteRegistryKey(root_key, uninstall_reg);

  // Then attempt to delete the old installation's start menu shortcut.
  FilePath uninstall_link;
  if (product.system_level()) {
    PathService::Get(base::DIR_COMMON_START_MENU, &uninstall_link);
  } else {
    PathService::Get(base::DIR_START_MENU, &uninstall_link);
  }

  if (uninstall_link.empty()) {
    LOG(ERROR) << "Failed to get location for shortcut.";
  } else {
    uninstall_link = uninstall_link.Append(
        product.distribution()->GetAppShortCutName());
    uninstall_link = uninstall_link.Append(
        product.distribution()->GetUninstallLinkName() + L".lnk");
    VLOG(1) << "Deleting old uninstall shortcut (if present): "
            << uninstall_link.value();
    if (!file_util::Delete(uninstall_link, true))
      VLOG(1) << "Failed to delete old uninstall shortcut.";
  }
}

// Copy master preferences file provided to installer, in the same folder
// as chrome.exe so Chrome first run can find it. This function will be called
// only on the first install of Chrome.
void CopyPreferenceFileForFirstRun(const Package& package,
                                   const FilePath& prefs_source_path) {
  FilePath prefs_dest_path(package.path().AppendASCII(
      installer_util::kDefaultMasterPrefs));
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
bool CreateOrUpdateChromeShortcuts(const FilePath& setup_path,
                                   const Version& new_version,
                                   installer_util::InstallStatus install_status,
                                   const Product& product,
                                   bool create_all_shortcut,
                                   bool alt_shortcut) {
  // TODO(tommi): Change this function to use WorkItemList.
#ifndef NDEBUG
  const installer_util::MasterPreferences& prefs =
      InstallUtil::GetMasterPreferencesForCurrentProcess();
  DCHECK(prefs.install_chrome());
#endif

  FilePath shortcut_path;
  int dir_enum = product.system_level() ? base::DIR_COMMON_START_MENU :
                                          base::DIR_START_MENU;
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
      product.package().path().Append(installer_util::kChromeExe));

  if ((install_status == installer_util::FIRST_INSTALL_SUCCESS) ||
      (install_status == installer_util::INSTALL_REPAIRED)) {
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
  if (ret && !product.IsMsi()) {
    FilePath uninstall_link(shortcut_path);  // Uninstall Chrome link
    uninstall_link = uninstall_link.Append(
        browser_dist->GetUninstallLinkName() + L".lnk");
    if ((install_status == installer_util::FIRST_INSTALL_SUCCESS) ||
        (install_status == installer_util::INSTALL_REPAIRED) ||
        (file_util::PathExists(uninstall_link))) {
      if (!file_util::PathExists(shortcut_path))
        file_util::CreateDirectory(shortcut_path);

      FilePath setup_exe(
          product.package().GetInstallerDirectory(new_version)
              .Append(setup_path.BaseName()));

      CommandLine arguments(CommandLine::NO_PROGRAM);
      AppendUninstallCommandLineFlags(&arguments, product);
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
    if (product.system_level()) {
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

bool RegisterComDlls(const Package& install,
                     const Version* current_version,
                     const Version& new_version) {
  // TODO(tommi): setup.exe should always have at least one DLL to register.
  // Currently we rely on scan_server_dlls.py to populate the array for us,
  // but we might as well use an explicit static array of required components.
  if (kNumDllsToRegister <= 0) {
    NOTREACHED() << "no dlls to register";
    return false;
  }

  // Unregister DLLs that were left from the old version that is being upgraded.
  if (current_version) {
    FilePath old_dll_path(install.path().Append(current_version->GetString()));
    // Ignore failures to unregister old DLLs.
    installer::RegisterComDllList(old_dll_path, install.system_level(), false,
                                  false);
  }

  FilePath dll_path(install.path().Append(new_version.GetString()));
  return installer::RegisterComDllList(dll_path, install.system_level(), true,
                                       true);
}

// After a successful copying of all the files, this function is called to
// do a few post install tasks:
// - Handle the case of in-use-update by updating "opv" (old version) key or
//   deleting it if not required.
// - Register any new dlls and unregister old dlls.
// - If this is an MSI install, ensures that the MSI marker is set, and sets
//   it if not.
// If these operations are successful, the function returns true, otherwise
// false.
bool DoPostInstallTasks(const FilePath& setup_path,
                        const FilePath& new_chrome_exe,
                        const Version* current_version,
                        const Version& new_version,
                        const Package& package) {
  HKEY root = package.system_level() ? HKEY_LOCAL_MACHINE :
                                       HKEY_CURRENT_USER;
  const Products& products = package.products();

  if (file_util::PathExists(new_chrome_exe)) {
    // Looks like this was in use update. So make sure we update the 'opv' key
    // with the current version that is active and 'cmd' key with the rename
    // command to run.
    if (!current_version) {
      LOG(ERROR) << "New chrome.exe exists but current version is NULL!";
      return false;
    }

    scoped_ptr<WorkItemList> inuse_list(WorkItem::CreateWorkItemList());
    FilePath installer_path(package.GetInstallerDirectory(new_version)
        .Append(setup_path.BaseName()));

    CommandLine rename(installer_path);
    rename.AppendSwitch(installer_util::switches::kRenameChromeExe);
    if (package.system_level())
      rename.AppendSwitch(installer_util::switches::kSystemLevel);

    if (InstallUtil::IsChromeSxSProcess())
      rename.AppendSwitch(installer_util::switches::kChromeSxS);

    for (size_t i = 0; i < products.size(); ++i) {
      BrowserDistribution* dist = products[i]->distribution();
      std::wstring version_key(dist->GetVersionKey());
      inuse_list->AddSetRegValueWorkItem(root, version_key,
                                         google_update::kRegOldVersionField,
                                         current_version->GetString(), true);

      // Adding this registry entry for all products is overkill.
      // However, as it stands, we don't have a way to know which distribution
      // will check the key and run the command, so we add it for all.
      // After the first run, the subsequent runs should just be noops.
      // (see Upgrade::SwapNewChromeExeIfPresent).
      inuse_list->AddSetRegValueWorkItem(root, version_key,
                                         google_update::kRegRenameCmdField,
                                         rename.command_line_string(), true);
    }

    if (!inuse_list->Do()) {
      LOG(ERROR) << "Couldn't write opv/cmd values to registry.";
      inuse_list->Rollback();
      return false;
    }
  } else {
    // Since this was not an in-use-update, delete 'opv' and 'cmd' keys.
    scoped_ptr<WorkItemList> inuse_list(WorkItem::CreateWorkItemList());
    for (size_t i = 0; i < products.size(); ++i) {
      BrowserDistribution* dist = products[i]->distribution();
      std::wstring version_key(dist->GetVersionKey());
      inuse_list->AddDeleteRegValueWorkItem(root, version_key,
                                            google_update::kRegOldVersionField,
                                            true);
      inuse_list->AddDeleteRegValueWorkItem(root, version_key,
                                            google_update::kRegRenameCmdField,
                                            true);
    }

    if (!inuse_list->Do()) {
      LOG(ERROR) << "Couldn't delete opv/cmd values from registry.";
      inuse_list->Rollback();
      return false;
    }
  }

  if (FindProduct(products, BrowserDistribution::CHROME_FRAME) ||
      FindProduct(products, BrowserDistribution::CEEE)) {
    // TODO(robershield): move the "which DLLs should be registered" policy
    // into the installer.
    if (!RegisterComDlls(package, current_version, new_version)) {
      LOG(ERROR) << "RegisterComDlls failed.  Aborting.";
      return false;
    }
  }

  // If we're told that we're an MSI install, make sure to set the marker
  // in the client state key so that future updates do the right thing.
  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];
    if (product->IsMsi()) {
      if (!product->SetMsiMarker(true))
        return false;

      // We want MSI installs to take over the Add/Remove Programs shortcut.
      // Make a best-effort attempt to delete any shortcuts left over from
      // previous non-MSI installations for the same type of install (system or
      // per user).
      DeleteUninstallShortcutsForMSI(*product);
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

void RegisterChromeOnMachine(const Product& product,
                             bool make_chrome_default) {
  DCHECK_EQ(product.distribution()->GetType(),
            BrowserDistribution::CHROME_BROWSER);

  // Try to add Chrome to Media Player shim inclusion list. We don't do any
  // error checking here because this operation will fail if user doesn't
  // have admin rights and we want to ignore the error.
  AddChromeToMediaPlayerList();

  // Is --make-chrome-default option is given we make Chrome default browser
  // otherwise we only register it on the machine as a valid browser.
  FilePath chrome_exe(
      product.package().path().Append(installer_util::kChromeExe));
  VLOG(1) << "Registering Chrome as browser: " << chrome_exe.value();
  if (make_chrome_default) {
    int level = ShellUtil::CURRENT_USER;
    if (product.system_level())
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
// oldest_installed_version: returns the oldest active version (if any)
//
// This function makes best effort to do installation in a transactional
// manner. If failed it tries to rollback all changes on the file system
// and registry. For example, if package exists before calling the
// function, it rolls back all new file and directory changes under
// package. If package does not exist before calling the function
// (typical new install), the function creates package during install
// and removes the whole directory during rollback.
installer_util::InstallStatus InstallNewVersion(
    const FilePath& setup_path,
    const FilePath& archive_path,
    const FilePath& src_path,
    const FilePath& temp_dir,
    const Version& new_version,
    scoped_ptr<Version>* current_version,
    const Package& package) {
  DCHECK(current_version);

  const Products& products = package.products();
  DCHECK(products.size());

  if (FindProduct(products, BrowserDistribution::CHROME_FRAME)) {
    // Make sure that we don't end up deleting installed files on next reboot.
    if (!RemoveFromMovesPendingReboot(package.path().value().c_str())) {
      LOG(ERROR) << "Error accessing pending moves value.";
    }
  }

  current_version->reset(package.GetCurrentVersion());

  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  // A temp directory that work items need and the actual install directory.
  install_list->AddCreateDirWorkItem(temp_dir);
  install_list->AddCreateDirWorkItem(package.path());

  // Delete any new_chrome.exe if present (we will end up creating a new one
  // if required) and then copy chrome.exe
  FilePath new_chrome_exe(
      package.path().Append(installer_util::kChromeNewExe));

  install_list->AddDeleteTreeWorkItem(new_chrome_exe);
  install_list->AddCopyTreeWorkItem(
      src_path.Append(installer_util::kChromeExe).value(),
      package.path().Append(installer_util::kChromeExe).value(),
      temp_dir.value(), WorkItem::NEW_NAME_IF_IN_USE, new_chrome_exe.value());

  // Extra executable for 64 bit systems.
  if (Is64bit()) {
    install_list->AddCopyTreeWorkItem(
        src_path.Append(installer::kWowHelperExe).value(),
        package.path().Append(installer::kWowHelperExe).value(),
        temp_dir.value(), WorkItem::ALWAYS);
  }

  // If it is system level install copy the version folder (since we want to
  // take the permissions of %ProgramFiles% folder) otherwise just move it.
  if (package.system_level()) {
    install_list->AddCopyTreeWorkItem(
        src_path.Append(new_version.GetString()).value(),
        package.path().Append(new_version.GetString()).value(),
        temp_dir.value(), WorkItem::ALWAYS);
  } else {
    install_list->AddMoveTreeWorkItem(
        src_path.Append(new_version.GetString()).value(),
        package.path().Append(new_version.GetString()).value(),
        temp_dir.value());
  }

  // Copy the default Dictionaries only if the folder doesn't exist already.
  install_list->AddCopyTreeWorkItem(
      src_path.Append(installer::kDictionaries).value(),
      package.path().Append(installer::kDictionaries).value(),
      temp_dir.value(), WorkItem::IF_NOT_PRESENT);

  // Delete any old_chrome.exe if present.
  install_list->AddDeleteTreeWorkItem(
      package.path().Append(installer_util::kChromeOldExe));

  // Copy installer in install directory and
  // add shortcut in Control Panel->Add/Remove Programs.
  AddInstallerCopyTasks(setup_path, archive_path, temp_dir, new_version,
                        install_list.get(), package);

  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];

    AddUninstallShortcutWorkItems(setup_path, new_version, install_list.get(),
                                  *product);

    // Create Version key for each distribution (if not already present) and set
    // the new product version as the last step.
    HKEY root = product->system_level() ? HKEY_LOCAL_MACHINE :
                                          HKEY_CURRENT_USER;
    std::wstring version_key(product->distribution()->GetVersionKey());
    install_list->AddCreateRegKeyWorkItem(root, version_key);

    std::wstring product_name(product->distribution()->GetAppShortCutName());
    install_list->AddSetRegValueWorkItem(root, version_key,
                                         google_update::kRegNameField,
                                         product_name,
                                         true);    // overwrite name also
    install_list->AddSetRegValueWorkItem(root, version_key,
                                         google_update::kRegOopcrashesField,
                                         1,
                                         false);   // set during first install
    install_list->AddSetRegValueWorkItem(root, version_key,
                                         google_update::kRegVersionField,
                                         new_version.GetString(),
                                         true);    // overwrite version
  }

  if (!install_list->Do() ||
      !DoPostInstallTasks(setup_path, new_chrome_exe, current_version->get(),
                          new_version, package)) {
    installer_util::InstallStatus result =
        file_util::PathExists(new_chrome_exe) && current_version->get() &&
        new_version.IsEqual(*current_version->get()) ?
        installer_util::SAME_VERSION_REPAIR_FAILED :
        installer_util::INSTALL_FAILED;
    LOG(ERROR) << "Install failed, rolling back... result: " << result;
    install_list->Rollback();
    LOG(ERROR) << "Rollback complete. ";
    return result;
  }

  if (!current_version->get()) {
    VLOG(1) << "First install of version " << new_version.GetString();
    return installer_util::FIRST_INSTALL_SUCCESS;
  }

  if (new_version.IsEqual(*current_version->get())) {
    VLOG(1) << "Install repaired of version " << new_version.GetString();
    return installer_util::INSTALL_REPAIRED;
  }

  if (new_version.IsHigherThan(current_version->get())) {
    if (file_util::PathExists(new_chrome_exe)) {
      VLOG(1) << "Version updated to " << new_version.GetString()
              << " while running " << (*current_version)->GetString();
      return installer_util::IN_USE_UPDATED;
    }
    VLOG(1) << "Version updated to " << new_version.GetString();
    return installer_util::NEW_VERSION_UPDATED;
  }

  LOG(ERROR) << "Not sure how we got here while updating"
             << ", new version: " << new_version.GetString()
             << ", old version: " << (*current_version)->GetString();

  return installer_util::INSTALL_FAILED;
}

}  // end namespace

namespace installer {

installer_util::InstallStatus InstallOrUpdateChrome(
    const FilePath& setup_path, const FilePath& archive_path,
    const FilePath& install_temp_path, const FilePath& prefs_path,
    const installer_util::MasterPreferences& prefs, const Version& new_version,
    const Package& install) {
  bool system_install = install.system_level();

  FilePath src_path(install_temp_path);
  src_path = src_path.Append(kInstallSourceDir).Append(kInstallSourceChromeDir);

  scoped_ptr<Version> existing_version;
  installer_util::InstallStatus result = InstallNewVersion(setup_path,
      archive_path, src_path, install_temp_path, new_version,
      &existing_version, install);

  if (!BrowserDistribution::GetInstallReturnCode(result)) {
    if (result == installer_util::FIRST_INSTALL_SUCCESS)
      CopyPreferenceFileForFirstRun(install, prefs_path);

    bool do_not_create_shortcuts = false;
    prefs.GetBool(installer_util::master_preferences::kDoNotCreateShortcuts,
                  &do_not_create_shortcuts);

    // Currently this only creates shortcuts for Chrome, but for other products
    // we might want to create shortcuts.
    const Product* chrome_install =
        FindProduct(install.products(), BrowserDistribution::CHROME_BROWSER);
    if (chrome_install && !do_not_create_shortcuts) {
      bool create_all_shortcut = false;
      prefs.GetBool(installer_util::master_preferences::kCreateAllShortcuts,
                    &create_all_shortcut);
      bool alt_shortcut = false;
      prefs.GetBool(installer_util::master_preferences::kAltShortcutText,
                    &alt_shortcut);
      if (!CreateOrUpdateChromeShortcuts(setup_path, new_version, result,
                                         *chrome_install, create_all_shortcut,
                                         alt_shortcut)) {
        PLOG(WARNING) << "Failed to create/update start menu shortcut.";
      }

      bool make_chrome_default = false;
      prefs.GetBool(installer_util::master_preferences::kMakeChromeDefault,
                    &make_chrome_default);

      // If this is not the user's first Chrome install, but they have chosen
      // Chrome to become their default browser on the download page, we must
      // force it here because the master_preferences file will not get copied
      // into the build.
      bool force_chrome_default_for_user = false;
      if (result == installer_util::NEW_VERSION_UPDATED ||
          result == installer_util::INSTALL_REPAIRED) {
        prefs.GetBool(
            installer_util::master_preferences::kMakeChromeDefaultForUser,
            &force_chrome_default_for_user);
      }

      RegisterChromeOnMachine(*chrome_install,
          make_chrome_default || force_chrome_default_for_user);
    }

    install.RemoveOldVersionDirectories(existing_version.get() ?
        *existing_version.get() : new_version);
  }

  return result;
}

bool RegisterComDllList(const FilePath& dll_folder, bool system_level,
                        bool do_register, bool rollback_on_failure) {
  bool success = false;
  scoped_ptr<WorkItemList> work_item_list;
  if (rollback_on_failure) {
    work_item_list.reset(WorkItem::CreateWorkItemList());
  } else {
    work_item_list.reset(WorkItem::CreateNoRollbackWorkItemList());
  }

  // TODO(robertshield): What if the list of old dlls and new ones isn't
  // the same?  I (elmo) think we should start storing the list of DLLs
  // somewhere.
  if (InstallUtil::BuildDLLRegistrationList(dll_folder.value(), kDllsToRegister,
                                            kNumDllsToRegister, do_register,
                                            !system_level,
                                            work_item_list.get())) {
    // Ignore failures to unregister old DLLs.
    success = work_item_list->Do();
    if (!success && rollback_on_failure) {
      work_item_list->Rollback();
    }
  }

  return success;
}

}  // namespace installer

