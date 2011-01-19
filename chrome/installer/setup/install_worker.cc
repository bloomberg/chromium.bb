// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the definitions of the installer functions that build
// the WorkItemList used to install the application.

#include "chrome/installer/setup/install_worker.h"

#include <shlobj.h>
#include <time.h>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/package.h"
#include "chrome/installer/util/package_properties.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

using base::win::RegKey;

namespace {

// This method tells if we are running on 64 bit platform so that we can copy
// one extra exe. If the API call to determine 64 bit fails, we play it safe
// and return true anyway so that the executable can be copied.
bool Is64bit() {
  typedef BOOL (WINAPI* WOW_FUNC)(HANDLE, BOOL*);
  BOOL is_64 = FALSE;

  HMODULE module = GetModuleHandle(L"kernel32.dll");
  WOW_FUNC is_wow64 = reinterpret_cast<WOW_FUNC>(
      GetProcAddress(module, "IsWow64Process"));
  return (is_wow64 != NULL) &&
           (!(is_wow64)(GetCurrentProcess(), &is_64) || (is_64 != FALSE));
}

}  // namespace

namespace installer {

// Local helper to call AddRegisterComDllWorkItems for all DLLs in a set of
// products managed by a given package.
void AddRegisterComDllWorkItemsForPackage(const Package& package,
                                          const Version* old_version,
                                          const Version& new_version,
                                          WorkItemList* work_item_list) {
  // First collect the list of DLLs to be registered from each product.
  const Products& products = package.products();
  Products::const_iterator product_iter(products.begin());
  std::vector<FilePath> com_dll_list;
  for (; product_iter != products.end(); ++product_iter) {
    BrowserDistribution* dist = product_iter->get()->distribution();
    std::vector<FilePath> dist_dll_list(dist->GetComDllList());
    com_dll_list.insert(com_dll_list.end(), dist_dll_list.begin(),
                        dist_dll_list.end());
  }

  // Then, if we got some, attempt to unregister the DLLs from the old
  // version directory and then re-register them in the new one.
  // Note that if we are migrating the install directory then we will not
  // successfully unregister the old DLLs.
  // TODO(robertshield): See whether we need to fix the migration case.
  // TODO(robertshield): If we ever remove a DLL from a product, this will
  // not unregister it on update. We should build the unregistration list from
  // saved state instead of assuming it is the same as the registration list.
  if (!com_dll_list.empty()) {
    if (old_version) {
      FilePath old_dll_path(
          package.path().Append(UTF8ToWide(old_version->GetString())));

      installer::AddRegisterComDllWorkItems(old_dll_path,
                                            com_dll_list,
                                            package.system_level(),
                                            false,  // Unregister
                                            true,   // May fail
                                            work_item_list);
    }

    FilePath dll_path(
        package.path().Append(UTF8ToWide(new_version.GetString())));
    installer::AddRegisterComDllWorkItems(dll_path,
                                          com_dll_list,
                                          package.system_level(),
                                          true,   // Register
                                          false,  // Must succeed.
                                          work_item_list);
  }
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

  if (product.is_chrome()) {
    // The Chrome uninstallation command serves as the master uninstall
    // command for Chrome + all other products (i.e. Chrome Frame) that do
    // not have an uninstall entry in the Add/Remove Programs dialog.
    const Products& products = product.package().products();
    for (size_t i = 0; i < products.size(); ++i) {
      const Product& p = *products[i];
      if (!p.is_chrome() && !p.ShouldCreateUninstallEntry()) {
        p.distribution()->AppendUninstallCommandLineFlags(&uninstall_arguments);
      }
    }
  }

  std::wstring update_state_key(browser_dist->GetStateKey());
  install_list->AddCreateRegKeyWorkItem(reg_root, update_state_key);
  install_list->AddSetRegValueWorkItem(reg_root, update_state_key,
      installer::kUninstallStringField, installer_path.value(), true);
  install_list->AddSetRegValueWorkItem(reg_root, update_state_key,
      installer::kUninstallArgumentsField,
      uninstall_arguments.command_line_string(), true);

  if (product.ShouldCreateUninstallEntry()) {
    // We need to quote the command line for the Add/Remove Programs dialog.
    CommandLine quoted_uninstall_cmd(installer_path);
    DCHECK_EQ(quoted_uninstall_cmd.command_line_string()[0], '"');
    quoted_uninstall_cmd.AppendArguments(uninstall_arguments, false);

    std::wstring uninstall_reg = browser_dist->GetUninstallRegPath();
    install_list->AddCreateRegKeyWorkItem(reg_root, uninstall_reg);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
        installer::kUninstallDisplayNameField,
        browser_dist->GetAppShortCutName(), true);
    install_list->AddSetRegValueWorkItem(reg_root,
        uninstall_reg, installer::kUninstallStringField,
        quoted_uninstall_cmd.command_line_string(), true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         L"InstallLocation",
                                         install_path.value(),
                                         true);

    // DisplayIcon, NoModify and NoRepair
    FilePath chrome_icon(install_path.Append(installer::kChromeExe));
    ShellUtil::GetChromeIcon(product.distribution(), chrome_icon.value());
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"DisplayIcon", chrome_icon.value(),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"NoModify", static_cast<DWORD>(1),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"NoRepair", static_cast<DWORD>(1),
                                         true);

    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"Publisher",
                                         browser_dist->GetPublisherName(),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"Version",
                                         UTF8ToWide(new_version.GetString()),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"DisplayVersion",
                                         UTF8ToWide(new_version.GetString()),
                                         true);
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

// Create Version key for a product (if not already present) and sets the new
// product version as the last step.
void AddVersionKeyWorkItems(HKEY root,
                            const Product& product,
                            const Version& new_version,
                            WorkItemList* list) {
  // Create Version key for each distribution (if not already present) and set
  // the new product version as the last step.
  std::wstring version_key(product.distribution()->GetVersionKey());
  list->AddCreateRegKeyWorkItem(root, version_key);

  std::wstring product_name(product.distribution()->GetAppShortCutName());
  list->AddSetRegValueWorkItem(root, version_key, google_update::kRegNameField,
                               product_name, true);  // overwrite name also
  list->AddSetRegValueWorkItem(root, version_key,
                               google_update::kRegOopcrashesField,
                               static_cast<DWORD>(1),
                               false);  // set during first install
  list->AddSetRegValueWorkItem(root, version_key,
                               google_update::kRegVersionField,
                               UTF8ToWide(new_version.GetString()),
                               true);  // overwrite version
}

void AddProductSpecificWorkItems(bool install,
                                 const FilePath& setup_path,
                                 const Version& new_version,
                                 const Package& package,
                                 WorkItemList* list) {
  const Products& products = package.products();
  for (size_t i = 0; i < products.size(); ++i) {
    const Product& p = *products[i];
    if (p.is_chrome_frame()) {
      AddChromeFrameWorkItems(install, setup_path, new_version, p, list);
    }
  }
}

// Adds work items that make registry adjustments for Google Update.  When a
// product is installed (including overinstall), Google Update will write the
// channel ("ap") value into either Chrome or Chrome Frame's ClientState key.
// In the multi-install case, this value is used as the basis upon which the
// package's channel value is built (by adding the ordered list of installed
// products and their options).
void AddGoogleUpdateWorkItems(const InstallationState& original_state,
                              const InstallerState& installer_state,
                              const Package& package,
                              WorkItemList* install_list) {
  // Is a multi-install product being installed or over-installed?
  if (installer_state.operation() != InstallerState::MULTI_INSTALL)
    return;

  const HKEY reg_root = package.system_level() ? HKEY_LOCAL_MACHINE :
                                                 HKEY_CURRENT_USER;
  const std::wstring key_path = installer_state.state_key();
  ChannelInfo channel_info;

  // Update the "ap" value for the product being installed/updated.
  // It is completely acceptable for there to be no "ap" value or even no
  // ClientState key.  Note that we check the registry rather than
  // original_state since on a fresh install the "ap" value will be present
  // sans "pv" value.
  channel_info.Initialize(RegKey(reg_root, key_path.c_str(), KEY_QUERY_VALUE));

  // This is a multi-install product.
  bool modified = channel_info.SetMultiInstall(true);

  // Add the appropriate modifiers for all products and their options.
  Products::const_iterator scan = package.products().begin();
  const Products::const_iterator end = package.products().end();
  for (; scan != end; ++scan) {
    modified |= scan->get()->distribution()->SetChannelFlags(true,
                                                             &channel_info);
  }

  // Write the results if needed.
  if (modified) {
    install_list->AddSetRegValueWorkItem(reg_root, key_path,
                                         google_update::kRegApField,
                                         channel_info.value(), true);
  }

  // Synchronize the other products and the package with this one.
  std::wstring other_key;
  std::vector<std::wstring> keys;

  keys.reserve(package.products().size());
  other_key = package.properties()->GetStateKey();
  if (other_key != key_path)
    keys.push_back(other_key);
  scan = package.products().begin();
  for (; scan != end; ++scan) {
    other_key = scan->get()->distribution()->GetStateKey();
    if (other_key != key_path)
      keys.push_back(other_key);
  }

  RegKey key;
  ChannelInfo other_info;
  std::vector<std::wstring>::const_iterator kscan = keys.begin();
  std::vector<std::wstring>::const_iterator kend = keys.end();
  for (; kscan != kend; ++kscan) {
    // Handle the case where the ClientState key doesn't exist by creating it.
    // This takes care of the multi-installer's package key, which is not
    // created by Google Update for us.
    if ((key.Open(reg_root, kscan->c_str(), KEY_QUERY_VALUE) != ERROR_SUCCESS)
        || (!other_info.Initialize(key))) {
      other_info.set_value(std::wstring());
    }
    if (!other_info.Equals(channel_info)) {
      if (!key.Valid())
        install_list->AddCreateRegKeyWorkItem(reg_root, *kscan);
      install_list->AddSetRegValueWorkItem(reg_root, *kscan,
                                           google_update::kRegApField,
                                           channel_info.value(), true);
    }
  }
  // TODO(grt): check for other keys/values we should put in the package's
  // ClientState and/or Clients key.
}

// This is called when an MSI installation is run. It may be that a user is
// attempting to install the MSI on top of a non-MSI managed installation.
// If so, try and remove any existing uninstallation shortcuts, as we want the
// uninstall to be managed entirely by the MSI machinery (accessible via the
// Add/Remove programs dialog).
void AddDeleteUninstallShortcutsForMSIWorkItems(const Product& product,
                                                WorkItemList* work_item_list) {
  DCHECK(product.IsMsi()) << "This must only be called for MSI installations!";

  // First attempt to delete the old installation's ARP dialog entry.
  HKEY reg_root = product.system_level() ? HKEY_LOCAL_MACHINE :
                                           HKEY_CURRENT_USER;
  base::win::RegKey root_key(reg_root, L"", KEY_ALL_ACCESS);
  std::wstring uninstall_reg(product.distribution()->GetUninstallRegPath());

  WorkItem* delete_reg_key = work_item_list->AddDeleteRegKeyWorkItem(
      reg_root, uninstall_reg);
  delete_reg_key->set_ignore_failure(true);

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
    WorkItem* delete_link = work_item_list->AddDeleteTreeWorkItem(
        uninstall_link);
    delete_link->set_ignore_failure(true);
    delete_link->set_log_message(
        "Failed to delete old uninstall shortcut.");
  }
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
bool AppendPostInstallTasks(bool multi_install,
                            const FilePath& setup_path,
                            const FilePath& new_chrome_exe,
                            const Version* current_version,
                            const Version& new_version,
                            const Package& package,
                            WorkItemList* post_install_task_list) {
  DCHECK(post_install_task_list);
  HKEY root = package.system_level() ? HKEY_LOCAL_MACHINE :
                                       HKEY_CURRENT_USER;
  const Products& products = package.products();


  // Append work items that will only be executed if this was an update.
  // We update the 'opv' key with the current version that is active and 'cmd'
  // key with the rename command to run.
  {
    scoped_ptr<WorkItemList> in_use_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new ConditionRunIfFileExists(new_chrome_exe)));
    in_use_update_work_items->set_log_message("InUseUpdateWorkItemList");

    FilePath installer_path(package.GetInstallerDirectory(new_version)
        .Append(setup_path.BaseName()));

    CommandLine rename(installer_path);
    rename.AppendSwitch(installer::switches::kRenameChromeExe);
    if (package.system_level())
      rename.AppendSwitch(installer::switches::kSystemLevel);

    if (InstallUtil::IsChromeSxSProcess())
      rename.AppendSwitch(installer::switches::kChromeSxS);

    if (multi_install)
      rename.AppendSwitch(installer::switches::kMultiInstall);

    std::wstring version_key;
    for (size_t i = 0; i < products.size(); ++i) {
      BrowserDistribution* dist = products[i]->distribution();
      version_key = dist->GetVersionKey();

      if (current_version != NULL) {
        in_use_update_work_items->AddSetRegValueWorkItem(root, version_key,
            google_update::kRegOldVersionField,
            UTF8ToWide(current_version->GetString()), true);
      }

      // Adding this registry entry for all products is overkill.
      // However, as it stands, we don't have a way to know which distribution
      // will check the key and run the command, so we add it for all.
      // After the first run, the subsequent runs should just be noops.
      // (see Upgrade::SwapNewChromeExeIfPresent).
      in_use_update_work_items->AddSetRegValueWorkItem(
          root,
          version_key,
          google_update::kRegRenameCmdField,
          rename.command_line_string(),
          true);
    }

    if (multi_install) {
      PackageProperties* props = package.properties();
      if (props->ReceivesUpdates() && current_version != NULL) {
        in_use_update_work_items->AddSetRegValueWorkItem(
            root,
            props->GetVersionKey(),
            google_update::kRegOldVersionField,
            UTF8ToWide(current_version->GetString()),
            true);
        // TODO(tommi): We should move the rename command here. We also need to
        // update Upgrade::SwapNewChromeExeIfPresent.
      }
    }

    post_install_task_list->AddWorkItem(in_use_update_work_items.release());
  }

  // Append work items that will be executed if this was NOT an in-use update.
  {
    scoped_ptr<WorkItemList> regular_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new Not(new ConditionRunIfFileExists(new_chrome_exe))));
    regular_update_work_items->set_log_message(
        "RegularUpdateWorkItemList");

    // Since this was not an in-use-update, delete 'opv' and 'cmd' keys.
    for (size_t i = 0; i < products.size(); ++i) {
      BrowserDistribution* dist = products[i]->distribution();
      std::wstring version_key(dist->GetVersionKey());
      regular_update_work_items->AddDeleteRegValueWorkItem(root, version_key,
                                            google_update::kRegOldVersionField);
      regular_update_work_items->AddDeleteRegValueWorkItem(root, version_key,
                                            google_update::kRegRenameCmdField);
    }

    post_install_task_list->AddWorkItem(regular_update_work_items.release());
  }

  AddRegisterComDllWorkItemsForPackage(package, current_version, new_version,
                                       post_install_task_list);

  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];
    // If we're told that we're an MSI install, make sure to set the marker
    // in the client state key so that future updates do the right thing.
    if (product->IsMsi()) {
      AddSetMsiMarkerWorkItem(*product, true, post_install_task_list);

      // We want MSI installs to take over the Add/Remove Programs shortcut.
      // Make a best-effort attempt to delete any shortcuts left over from
      // previous non-MSI installations for the same type of install (system or
      // per user).
      AddDeleteUninstallShortcutsForMSIWorkItems(*product,
                                                 post_install_task_list);
    }
  }

  return true;
}

void AddInstallWorkItems(const InstallationState& original_state,
                         const InstallerState& installer_state,
                         bool multi_install,
                         const FilePath& setup_path,
                         const FilePath& archive_path,
                         const FilePath& src_path,
                         const FilePath& temp_dir,
                         const Version& new_version,
                         scoped_ptr<Version>* current_version,
                         const Package& package,
                         WorkItemList* install_list) {
  DCHECK(install_list);

  // A temp directory that work items need and the actual install directory.
  install_list->AddCreateDirWorkItem(temp_dir);
  install_list->AddCreateDirWorkItem(package.path());

  // Delete any new_chrome.exe if present (we will end up creating a new one
  // if required) and then copy chrome.exe
  FilePath new_chrome_exe(
      package.path().Append(installer::kChromeNewExe));

  install_list->AddDeleteTreeWorkItem(new_chrome_exe);
  install_list->AddCopyTreeWorkItem(
      src_path.Append(installer::kChromeExe).value(),
      package.path().Append(installer::kChromeExe).value(),
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
        src_path.Append(UTF8ToWide(new_version.GetString())).value(),
        package.path().Append(UTF8ToWide(new_version.GetString())).value(),
        temp_dir.value(), WorkItem::ALWAYS);
  } else {
    install_list->AddMoveTreeWorkItem(
        src_path.Append(UTF8ToWide(new_version.GetString())).value(),
        package.path().Append(UTF8ToWide(new_version.GetString())).value(),
        temp_dir.value());
  }

  // Copy the default Dictionaries only if the folder doesn't exist already.
  install_list->AddCopyTreeWorkItem(
      src_path.Append(installer::kDictionaries).value(),
      package.path().Append(installer::kDictionaries).value(),
      temp_dir.value(), WorkItem::IF_NOT_PRESENT);

  // Delete any old_chrome.exe if present.
  install_list->AddDeleteTreeWorkItem(
      package.path().Append(installer::kChromeOldExe));

  // Copy installer in install directory and
  // add shortcut in Control Panel->Add/Remove Programs.
  AddInstallerCopyTasks(setup_path, archive_path, temp_dir, new_version,
                        install_list, package);

  HKEY root = package.system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  const Products& products = package.products();
  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];

    AddUninstallShortcutWorkItems(setup_path, new_version, install_list,
                                  *product);

    AddVersionKeyWorkItems(root, *product, new_version, install_list);
  }

  if (multi_install) {
    PackageProperties* props = package.properties();
    if (props->ReceivesUpdates()) {
      std::wstring version_key(props->GetVersionKey());
      install_list->AddCreateRegKeyWorkItem(root, version_key);
      install_list->AddSetRegValueWorkItem(root, version_key,
                                           google_update::kRegVersionField,
                                           UTF8ToWide(new_version.GetString()),
                                           true);    // overwrite version
      install_list->AddSetRegValueWorkItem(root, version_key,
          google_update::kRegNameField,
          ASCIIToWide(installer::PackageProperties::kPackageProductName),
          true);    // overwrite name also
    }
  }

  // Add any remaining work items that involve special settings for
  // each product.
  AddProductSpecificWorkItems(true, setup_path, new_version, package,
                              install_list);

  AddGoogleUpdateWorkItems(original_state, installer_state, package,
                           install_list);

  // Append the tasks that run after the installation.
  AppendPostInstallTasks(multi_install,
                         setup_path,
                         new_chrome_exe,
                         current_version->get(),
                         new_version,
                         package,
                         install_list);
}

void AddRegisterComDllWorkItems(const FilePath& dll_folder,
                                const std::vector<FilePath>& dll_list,
                                bool system_level,
                                bool do_register,
                                bool ignore_failures,
                                WorkItemList* work_item_list) {
  DCHECK(work_item_list);
  if (dll_list.empty()) {
    VLOG(1) << "No COM DLLs to register";
  } else {
    std::vector<FilePath>::const_iterator dll_iter(dll_list.begin());
    for (; dll_iter != dll_list.end(); ++dll_iter) {
      FilePath dll_path = dll_folder.Append(*dll_iter);
      WorkItem* work_item = work_item_list->AddSelfRegWorkItem(
          dll_path.value(), do_register, !system_level);
      DCHECK(work_item);
      work_item->set_ignore_failure(ignore_failures);
    }
  }
}

void AddSetMsiMarkerWorkItem(const Product& product,
                             bool set,
                             WorkItemList* work_item_list) {
  DCHECK(work_item_list);
  BrowserDistribution* dist = product.distribution();
  HKEY reg_root = product.system_level() ? HKEY_LOCAL_MACHINE :
                                           HKEY_CURRENT_USER;
  DWORD msi_value = set ? 1 : 0;
  WorkItem* set_msi_work_item = work_item_list->AddSetRegValueWorkItem(
      reg_root, dist->GetStateKey(), google_update::kRegMSIField,
      msi_value, true);
  DCHECK(set_msi_work_item);
  set_msi_work_item->set_ignore_failure(true);
  set_msi_work_item->set_log_message("Could not write MSI marker!");
}

void AddChromeFrameWorkItems(bool install,
                             const FilePath& setup_path,
                             const Version& new_version,
                             const Product& product,
                             WorkItemList* list) {
  DCHECK(product.is_chrome_frame());
  if (!product.package().multi_install()) {
    VLOG(1) << "Not adding GCF specific work items for single install.";
    return;
  }

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();

  BrowserDistribution* cf = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME, prefs);
  std::wstring version_key(cf->GetVersionKey());

  // TODO(tommi): This assumes we know exactly how ShouldCreateUninstallEntry
  // is implemented.  Since there is logic in ChromeFrameDistribution for how
  // to determine when this is enabled, this is how we have to figure out if
  // this feature is enabled right now, but it's a hack and we need a cleaner
  // way to figure this out.
  // Note that we cannot just check the master preferences for
  // kChromeFrameReadyMode, since there are other things that need to be correct
  // in the environment in order to enable this feature.
  bool ready_mode = !product.distribution()->ShouldCreateUninstallEntry();

  HKEY root = product.package().system_level() ? HKEY_LOCAL_MACHINE :
                                                 HKEY_CURRENT_USER;
  bool update_chrome_uninstall_command = false;
  if (ready_mode) {
    // If GCF is being installed in ready mode, we write an entry to the
    // multi-install state key.  If the value already exists, we will not
    // overwrite it since the user might have opted out.
    list->AddCreateRegKeyWorkItem(root,
        product.package().properties()->GetStateKey());
    list->AddSetRegValueWorkItem(root,
        product.package().properties()->GetStateKey(),
        installer::kChromeFrameReadyModeField,
        static_cast<int64>(install ? 1 : 0),  // The value we want to set.
        install ? false : true);  // Overwrite existing value.
    if (install) {
      FilePath installer_path(product.package()
          .GetInstallerDirectory(new_version).Append(setup_path.BaseName()));

      CommandLine basic_cl(installer_path);
      basic_cl.AppendSwitch(installer::switches::kChromeFrame);
      basic_cl.AppendSwitch(installer::switches::kMultiInstall);

      if (product.package().system_level())
        basic_cl.AppendSwitch(installer::switches::kSystemLevel);

      if (InstallUtil::IsChromeSxSProcess())
        basic_cl.AppendSwitch(installer::switches::kChromeSxS);

      CommandLine temp_opt_out(basic_cl);
      temp_opt_out.AppendSwitch(
          installer::switches::kChromeFrameReadyModeTempOptOut);

      CommandLine end_temp_opt_out(basic_cl);
      end_temp_opt_out.AppendSwitch(
          installer::switches::kChromeFrameReadyModeEndTempOptOut);

      CommandLine opt_out(installer_path);
      AppendUninstallCommandLineFlags(&opt_out, product);
      // Force Uninstall silences the prompt to reboot to complete uninstall.
      opt_out.AppendSwitch(installer::switches::kForceUninstall);

      CommandLine opt_in(basic_cl);
      opt_in.AppendSwitch(
          installer::switches::kChromeFrameReadyModeOptIn);

      list->AddSetRegValueWorkItem(root, version_key,
                                   google_update::kRegCFTempOptOutCmdField,
                                   temp_opt_out.command_line_string(), true);
      list->AddSetRegValueWorkItem(root, version_key,
                                   google_update::kRegCFEndTempOptOutCmdField,
                                   end_temp_opt_out.command_line_string(),
                                   true);
      list->AddSetRegValueWorkItem(root, version_key,
                                   google_update::kRegCFOptOutCmdField,
                                   opt_out.command_line_string(), true);
      list->AddSetRegValueWorkItem(root, version_key,
                                   google_update::kRegCFOptInCmdField,
                                   opt_in.command_line_string(), true);
    } else {
      // If Chrome is not also being uninstalled, we need to update its command
      // line so that it doesn't include uninstalling Chrome Frame now.
      update_chrome_uninstall_command =
          (installer::FindProduct(product.package().products(),
              BrowserDistribution::CHROME_BROWSER) == NULL);
    }
  } else {
    // It doesn't matter here if we're installing or uninstalling Chrome Frame.
    // If ready mode isn't specified on the command line for installs, we need
    // to delete the ready mode flag from the registry if it exists - this
    // constitutes an opt-in for the user.  If we're uninstalling CF and ready
    // mode isn't specified on the command line, that means that CF wasn't
    // installed with ready mode enabled (the --ready-mode switch should be set
    // in the registry) so deleting the value should have no effect.
    // In both cases (install/uninstall), we need to make sure that Chrome's
    // uninstallation command line does not include the --chrome-frame switch
    // so that uninstalling Chrome will no longer uninstall Chrome Frame.

    if (RegKey(root, product.package().properties()->GetStateKey().c_str(),
               KEY_QUERY_VALUE).Valid()) {
      list->AddDeleteRegValueWorkItem(root,
          product.package().properties()->GetStateKey(),
          installer::kChromeFrameReadyModeField);
    }

    const Product* chrome = installer::FindProduct(product.package().products(),
        BrowserDistribution::CHROME_BROWSER);
    if (chrome) {
      // Chrome is already a part of this installation run, so we can assume
      // that the uninstallation arguments will be updated correctly.
    } else {
      // Chrome is not a part of this installation run, so we have to explicitly
      // check if Chrome is installed, and if so, update its uninstallation
      // command lines.
      BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER,
          MasterPreferences::ForCurrentProcess());
      update_chrome_uninstall_command =
          IsInstalledAsMulti(product.system_level(), dist);
    }
  }

  if (!ready_mode || !install) {
    list->AddDeleteRegValueWorkItem(root, version_key,
                                    google_update::kRegCFTempOptOutCmdField);
    list->AddDeleteRegValueWorkItem(root, version_key,
                                    google_update::kRegCFEndTempOptOutCmdField);
    list->AddDeleteRegValueWorkItem(root, version_key,
                                    google_update::kRegCFOptOutCmdField);
    list->AddDeleteRegValueWorkItem(root, version_key,
                                    google_update::kRegCFOptInCmdField);
  }

  if (update_chrome_uninstall_command) {
    // Chrome is not a part of this installation run, so we have to explicitly
    // check if Chrome is installed, and if so, update its uninstallation
    // command lines.
    BrowserDistribution* chrome_dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER, prefs);
    const Package& pack = product.package();
    scoped_refptr<Package> package(new Package(pack.multi_install(),
        pack.system_level(), pack.path(), pack.properties()));
    scoped_refptr<Product> chrome_product(new Product(chrome_dist, package));
    AddUninstallShortcutWorkItems(setup_path, new_version, list,
                                  *chrome_product.get());
  }
}

void AppendUninstallCommandLineFlags(CommandLine* uninstall_cmd,
                                     const Product& product) {
  DCHECK(uninstall_cmd);

  uninstall_cmd->AppendSwitch(installer::switches::kUninstall);

  // Append the product-specific uninstall flags.
  product.distribution()->AppendUninstallCommandLineFlags(uninstall_cmd);
  if (product.IsMsi()) {
    uninstall_cmd->AppendSwitch(installer::switches::kMsi);
    // See comment in uninstall.cc where we check for the kDeleteProfile switch.
    if (product.is_chrome_frame()) {
      uninstall_cmd->AppendSwitch(installer::switches::kDeleteProfile);
    }
  }
  if (product.system_level())
    uninstall_cmd->AppendSwitch(installer::switches::kSystemLevel);

  // Propagate switches obtained from preferences as well.
  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  if (prefs.is_multi_install()) {
    uninstall_cmd->AppendSwitch(installer::switches::kMultiInstall);
  }
  bool value = false;
  if (prefs.GetBool(installer::master_preferences::kVerboseLogging,
                    &value) && value)
    uninstall_cmd->AppendSwitch(installer::switches::kVerboseLogging);
}

}  // namespace installer
