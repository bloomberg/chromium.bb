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
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

#include "chrome_tab.h"  // NOLINT

using base::win::RegKey;

namespace installer {

// Local helper to call AddRegisterComDllWorkItems for all DLLs in a set of
// products managed by a given package.
void AddRegisterComDllWorkItemsForPackage(const InstallerState& installer_state,
                                          const Version* old_version,
                                          const Version& new_version,
                                          WorkItemList* work_item_list) {
  // First collect the list of DLLs to be registered from each product.
  std::vector<FilePath> com_dll_list;
  installer_state.AddComDllList(&com_dll_list);

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
      FilePath old_dll_path(installer_state.target_path().AppendASCII(
          old_version->GetString()));

      installer::AddRegisterComDllWorkItems(old_dll_path,
                                            com_dll_list,
                                            installer_state.system_install(),
                                            false,  // Unregister
                                            true,   // May fail
                                            work_item_list);
    }

    FilePath dll_path(installer_state.target_path().AppendASCII(
        new_version.GetString()));
    installer::AddRegisterComDllWorkItems(dll_path,
                                          com_dll_list,
                                          installer_state.system_install(),
                                          true,   // Register
                                          false,  // Must succeed.
                                          work_item_list);
  }
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

  // In the past, we copied rather than moved for system level installs so that
  // the permissions of %ProgramFiles% would be picked up.  Now that |temp_path|
  // is in %ProgramFiles% for system level installs (and in %LOCALAPPDATA%
  // otherwise), there is no need to do this.
  install_list->AddMoveTreeWorkItem(setup_path.value(), exe_dst.value(),
                                    temp_path.value());
  install_list->AddMoveTreeWorkItem(archive_path.value(), archive_dst.value(),
                                    temp_path.value());
}

// This method adds work items to create (or update) Chrome uninstall entry in
// either the Control Panel->Add/Remove Programs list or in the Omaha client
// state key if running under an MSI installer.
void AddUninstallShortcutWorkItems(const InstallerState& installer_state,
                                   const FilePath& setup_path,
                                   const Version& new_version,
                                   WorkItemList* install_list,
                                   const Product& product) {
  HKEY reg_root = installer_state.root_key();
  BrowserDistribution* browser_dist = product.distribution();
  DCHECK(browser_dist);

  // When we are installed via an MSI, we need to store our uninstall strings
  // in the Google Update client state key. We do this even for non-MSI
  // managed installs to avoid breaking the edge case whereby an MSI-managed
  // install is updated by a non-msi installer (which would confuse the MSI
  // machinery if these strings were not also updated).
  // Do not quote the command line for the MSI invocation.
  FilePath install_path(installer_state.target_path());
  FilePath installer_path(installer_state.GetInstallerDirectory(new_version));
  installer_path = installer_path.Append(setup_path.BaseName());

  CommandLine uninstall_arguments(CommandLine::NO_PROGRAM);
  AppendUninstallCommandLineFlags(installer_state, product,
                                  &uninstall_arguments);

  // The Chrome uninstallation command serves as the master uninstall command
  // for Chrome + all other products (i.e. Chrome Frame) that do not have an
  // uninstall entry in the Add/Remove Programs dialog.  We skip this processing
  // in case of uninstall since this means that Chrome Frame is being
  // uninstalled, so there's no need to do any looping.
  if (product.is_chrome() &&
      installer_state.operation() != InstallerState::UNINSTALL) {
    const Products& products = installer_state.products();
    for (size_t i = 0; i < products.size(); ++i) {
      const Product& p = *products[i];
      if (!p.is_chrome() && !p.ShouldCreateUninstallEntry())
        p.AppendProductFlags(&uninstall_arguments);
    }
  }

  std::wstring update_state_key(browser_dist->GetStateKey());
  install_list->AddCreateRegKeyWorkItem(reg_root, update_state_key);
  install_list->AddSetRegValueWorkItem(reg_root, update_state_key,
      installer::kUninstallStringField, installer_path.value(), true);
  install_list->AddSetRegValueWorkItem(reg_root, update_state_key,
      installer::kUninstallArgumentsField,
      uninstall_arguments.command_line_string(), true);

  // MSI installations will manage their own uninstall shortcuts.
  if (!installer_state.is_msi() && product.ShouldCreateUninstallEntry()) {
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
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"InstallDate",
                                         InstallUtil::GetCurrentDate(),
                                         false);
  }
}

// Create Version key for a product (if not already present) and sets the new
// product version as the last step.
void AddVersionKeyWorkItems(HKEY root,
                            BrowserDistribution* dist,
                            const Version& new_version,
                            WorkItemList* list) {
  // Create Version key for each distribution (if not already present) and set
  // the new product version as the last step.
  std::wstring version_key(dist->GetVersionKey());
  list->AddCreateRegKeyWorkItem(root, version_key);

  std::wstring product_name(dist->GetAppShortCutName());
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

void AddProductSpecificWorkItems(const InstallationState& original_state,
                                 const InstallerState& installer_state,
                                 const FilePath& setup_path,
                                 const Version& new_version,
                                 WorkItemList* list) {
  const Products& products = installer_state.products();
  for (size_t i = 0; i < products.size(); ++i) {
    const Product& p = *products[i];
    if (p.is_chrome_frame()) {
      AddChromeFrameWorkItems(original_state, installer_state, setup_path,
                              new_version, p, list);
    }
  }
}

// Adds work items that make registry adjustments for Google Update.  When a
// product is installed (including overinstall), Google Update will write the
// channel ("ap") value into either Chrome or Chrome Frame's ClientState key.
// In the multi-install case, this value is used as the basis upon which the
// package's channel value is built (by adding the ordered list of installed
// products and their options).
void AddGoogleUpdateWorkItems(const InstallerState& installer_state,
                              WorkItemList* install_list) {
  // Is a multi-install product being installed or over-installed?
  if (installer_state.operation() != InstallerState::MULTI_INSTALL &&
      installer_state.operation() != InstallerState::MULTI_UPDATE) {
    VLOG(1) << "AddGoogleUpdateWorkItems noop: " << installer_state.operation();
    return;
  }

  const HKEY reg_root = installer_state.root_key();
  const std::wstring key_path = installer_state.state_key();
  ChannelInfo channel_info;

  // Update the "ap" value for the product being installed/updated.
  // It is completely acceptable for there to be no "ap" value or even no
  // ClientState key.  Note that we check the registry rather than an
  // InstallationState instance since on a fresh install the "ap" value will be
  // present sans "pv" value.
  channel_info.Initialize(RegKey(reg_root, key_path.c_str(), KEY_QUERY_VALUE));

  // This is a multi-install product.
  bool modified = channel_info.SetMultiInstall(true);

  // Add the appropriate modifiers for all products and their options.
  modified |= installer_state.SetChannelFlags(true, &channel_info);

  VLOG(1) << "ap: " << channel_info.value();

  // Write the results if needed.
  if (modified) {
    install_list->AddSetRegValueWorkItem(reg_root, key_path,
                                         google_update::kRegApField,
                                         channel_info.value(), true);
  } else {
    VLOG(1) << "Channel flags not modified";
  }

  // Synchronize the other products and the package with this one.
  std::wstring other_key;
  std::vector<std::wstring> keys;

  keys.reserve(installer_state.products().size());
  other_key =
      installer_state.multi_package_binaries_distribution()->GetStateKey();
  if (other_key != key_path)
    keys.push_back(other_key);
  Products::const_iterator scan = installer_state.products().begin();
  Products::const_iterator end = installer_state.products().end();
  for (; scan != end; ++scan) {
    other_key = (*scan)->distribution()->GetStateKey();
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
void AddDeleteUninstallShortcutsForMSIWorkItems(
    const InstallerState& installer_state,
    const Product& product,
    const FilePath& temp_path,
    WorkItemList* work_item_list) {
  DCHECK(installer_state.is_msi())
      << "This must only be called for MSI installations!";

  // First attempt to delete the old installation's ARP dialog entry.
  HKEY reg_root = installer_state.root_key();
  std::wstring uninstall_reg(product.distribution()->GetUninstallRegPath());

  WorkItem* delete_reg_key = work_item_list->AddDeleteRegKeyWorkItem(
      reg_root, uninstall_reg);
  delete_reg_key->set_ignore_failure(true);

  // Then attempt to delete the old installation's start menu shortcut.
  FilePath uninstall_link;
  if (installer_state.system_install()) {
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
        uninstall_link, temp_path);
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
bool AppendPostInstallTasks(const InstallerState& installer_state,
                            const FilePath& setup_path,
                            const FilePath& new_chrome_exe,
                            const Version* current_version,
                            const Version& new_version,
                            const FilePath& temp_path,
                            WorkItemList* post_install_task_list) {
  DCHECK(post_install_task_list);

  HKEY root = installer_state.root_key();
  const Products& products = installer_state.products();

  // Append work items that will only be executed if this was an update.
  // We update the 'opv' key with the current version that is active and 'cmd'
  // key with the rename command to run.
  {
    scoped_ptr<WorkItemList> in_use_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new ConditionRunIfFileExists(new_chrome_exe)));
    in_use_update_work_items->set_log_message("InUseUpdateWorkItemList");

    FilePath installer_path(installer_state.GetInstallerDirectory(new_version)
        .Append(setup_path.BaseName()));

    CommandLine rename(installer_path);
    rename.AppendSwitch(switches::kRenameChromeExe);
    if (installer_state.system_install())
      rename.AppendSwitch(switches::kSystemLevel);

    if (installer_state.verbose_logging())
      rename.AppendSwitch(switches::kVerboseLogging);

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
      CommandLine product_rename_cmd(rename);
      products[i]->AppendProductFlags(&product_rename_cmd);
      in_use_update_work_items->AddSetRegValueWorkItem(
          root,
          version_key,
          google_update::kRegRenameCmdField,
          product_rename_cmd.command_line_string(),
          true);
    }

    if (current_version != NULL && installer_state.is_multi_install()) {
      BrowserDistribution* dist =
          installer_state.multi_package_binaries_distribution();
      in_use_update_work_items->AddSetRegValueWorkItem(
          root,
          dist->GetVersionKey(),
          google_update::kRegOldVersionField,
          UTF8ToWide(current_version->GetString()),
          true);
      // TODO(tommi): We should move the rename command here. We also need to
      // update Upgrade::SwapNewChromeExeIfPresent.
    }

    post_install_task_list->AddWorkItem(in_use_update_work_items.release());
  }

  // Append work items that will be executed if this was NOT an in-use update.
  {
    scoped_ptr<WorkItemList> regular_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new Not(new ConditionRunIfFileExists(new_chrome_exe))));
    regular_update_work_items->set_log_message("RegularUpdateWorkItemList");

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

  AddRegisterComDllWorkItemsForPackage(installer_state, current_version,
                                       new_version, post_install_task_list);

  // If we're told that we're an MSI install, make sure to set the marker
  // in the client state key so that future updates do the right thing.
  if (installer_state.is_msi()) {
    for (size_t i = 0; i < products.size(); ++i) {
      const Product* product = products[i];
      AddSetMsiMarkerWorkItem(installer_state, product->distribution(), true,
                              post_install_task_list);

      // We want MSI installs to take over the Add/Remove Programs shortcut.
      // Make a best-effort attempt to delete any shortcuts left over from
      // previous non-MSI installations for the same type of install (system or
      // per user).
      AddDeleteUninstallShortcutsForMSIWorkItems(installer_state, *product,
                                                 temp_path,
                                                 post_install_task_list);
    }
    if (installer_state.is_multi_install()) {
      AddSetMsiMarkerWorkItem(installer_state,
          installer_state.multi_package_binaries_distribution(), true,
          post_install_task_list);
    }
  }

  return true;
}

void AddInstallWorkItems(const InstallationState& original_state,
                         const InstallerState& installer_state,
                         const FilePath& setup_path,
                         const FilePath& archive_path,
                         const FilePath& src_path,
                         const FilePath& temp_path,
                         const Version& new_version,
                         scoped_ptr<Version>* current_version,
                         WorkItemList* install_list) {
  DCHECK(install_list);

  const FilePath& target_path = installer_state.target_path();

  // A temp directory that work items need and the actual install directory.
  install_list->AddCreateDirWorkItem(temp_path);
  install_list->AddCreateDirWorkItem(target_path);

  // Delete any new_chrome.exe if present (we will end up creating a new one
  // if required) and then copy chrome.exe
  FilePath new_chrome_exe(
      target_path.Append(installer::kChromeNewExe));

  install_list->AddDeleteTreeWorkItem(new_chrome_exe, temp_path);
  install_list->AddCopyTreeWorkItem(
      src_path.Append(installer::kChromeExe).value(),
      target_path.Append(installer::kChromeExe).value(),
      temp_path.value(), WorkItem::NEW_NAME_IF_IN_USE, new_chrome_exe.value());

  // Extra executable for 64 bit systems.
  // NOTE: We check for "not disabled" so that if the API call fails, we play it
  // safe and copy the executable anyway.
  if (base::win::GetWOW64Status() != base::win::WOW64_DISABLED) {
    install_list->AddMoveTreeWorkItem(
        src_path.Append(installer::kWowHelperExe).value(),
        target_path.Append(installer::kWowHelperExe).value(),
        temp_path.value());
  }

  // In the past, we copied rather than moved for system level installs so that
  // the permissions of %ProgramFiles% would be picked up.  Now that |temp_path|
  // is in %ProgramFiles% for system level installs (and in %LOCALAPPDATA%
  // otherwise), there is no need to do this.
  install_list->AddMoveTreeWorkItem(
      src_path.AppendASCII(new_version.GetString()).value(),
      target_path.AppendASCII(new_version.GetString()).value(),
      temp_path.value());

  // Copy the default Dictionaries only if the folder doesn't exist already.
  // TODO(grt): Use AddMoveTreeWorkItem in a conditional WorkItemList, which
  // will be more efficient in space and time.
  install_list->AddCopyTreeWorkItem(
      src_path.Append(installer::kDictionaries).value(),
      target_path.Append(installer::kDictionaries).value(),
      temp_path.value(), WorkItem::IF_NOT_PRESENT);

  // Delete any old_chrome.exe if present.
  install_list->AddDeleteTreeWorkItem(
      target_path.Append(installer::kChromeOldExe), temp_path);

  // Copy installer in install directory and
  // add shortcut in Control Panel->Add/Remove Programs.
  AddInstallerCopyTasks(installer_state, setup_path, archive_path, temp_path,
                        new_version, install_list);

  const HKEY root = installer_state.root_key();

  const Products& products = installer_state.products();
  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];

    AddUninstallShortcutWorkItems(installer_state, setup_path, new_version,
                                  install_list, *product);

    AddVersionKeyWorkItems(root, product->distribution(), new_version,
                           install_list);
  }

  if (installer_state.is_multi_install()) {
    AddVersionKeyWorkItems(root,
        installer_state.multi_package_binaries_distribution(), new_version,
        install_list);
  }

  // Add any remaining work items that involve special settings for
  // each product.
  AddProductSpecificWorkItems(original_state, installer_state, setup_path,
                              new_version, install_list);

  AddGoogleUpdateWorkItems(installer_state, install_list);

  AddQuickEnableWorkItems(installer_state, original_state, &setup_path,
                          &new_version, install_list);

  // Append the tasks that run after the installation.
  AppendPostInstallTasks(installer_state,
                         setup_path,
                         new_chrome_exe,
                         current_version->get(),
                         new_version,
                         temp_path,
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

void AddSetMsiMarkerWorkItem(const InstallerState& installer_state,
                             BrowserDistribution* dist,
                             bool set,
                             WorkItemList* work_item_list) {
  DCHECK(work_item_list);
  DWORD msi_value = set ? 1 : 0;
  WorkItem* set_msi_work_item = work_item_list->AddSetRegValueWorkItem(
      installer_state.root_key(), dist->GetStateKey(),
      google_update::kRegMSIField, msi_value, true);
  DCHECK(set_msi_work_item);
  set_msi_work_item->set_ignore_failure(true);
  set_msi_work_item->set_log_message("Could not write MSI marker!");
}

void AddChromeFrameWorkItems(const InstallationState& original_state,
                             const InstallerState& installer_state,
                             const FilePath& setup_path,
                             const Version& new_version,
                             const Product& product,
                             WorkItemList* list) {
  DCHECK(product.is_chrome_frame());
  if (!installer_state.is_multi_install()) {
    VLOG(1) << "Not adding GCF specific work items for single install.";
    return;
  }

  std::wstring version_key(product.distribution()->GetVersionKey());
  bool ready_mode = product.HasOption(kOptionReadyMode);
  HKEY root = installer_state.root_key();
  const bool is_install =
      (installer_state.operation() != InstallerState::UNINSTALL);
  bool update_chrome_uninstall_command = false;
  BrowserDistribution* dist =
      installer_state.multi_package_binaries_distribution();
  if (ready_mode) {
    // If GCF is being installed in ready mode, we write an entry to the
    // multi-install state key.  If the value already exists, we will not
    // overwrite it since the user might have opted out.
    list->AddCreateRegKeyWorkItem(root, dist->GetStateKey());
    list->AddSetRegValueWorkItem(root, dist->GetStateKey(),
        kChromeFrameReadyModeField,
        static_cast<int64>(is_install ? 1 : 0),  // The value we want to set.
        is_install ? false : true);  // Overwrite existing value.
    if (is_install) {
      FilePath installer_path(installer_state
          .GetInstallerDirectory(new_version).Append(setup_path.BaseName()));

      CommandLine basic_cl(installer_path);
      basic_cl.AppendSwitch(switches::kChromeFrame);
      basic_cl.AppendSwitch(switches::kMultiInstall);

      if (installer_state.system_install())
        basic_cl.AppendSwitch(switches::kSystemLevel);

      CommandLine temp_opt_out(basic_cl);
      temp_opt_out.AppendSwitch(switches::kChromeFrameReadyModeTempOptOut);

      CommandLine end_temp_opt_out(basic_cl);
      end_temp_opt_out.AppendSwitch(
          switches::kChromeFrameReadyModeEndTempOptOut);

      CommandLine opt_out(installer_path);
      AppendUninstallCommandLineFlags(installer_state, product, &opt_out);
      // Force Uninstall silences the prompt to reboot to complete uninstall.
      opt_out.AppendSwitch(switches::kForceUninstall);

      CommandLine opt_in(basic_cl);
      opt_in.AppendSwitch(switches::kChromeFrameReadyModeOptIn);

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
          (installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER) ==
           NULL);
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

    list->AddDeleteRegValueWorkItem(root, dist->GetStateKey(),
        kChromeFrameReadyModeField);

    const Product* chrome =
        installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER);
    if (chrome) {
      // Chrome is already a part of this installation run, so we can assume
      // that the uninstallation arguments will be updated correctly.
    } else {
      // Chrome is not a part of this installation run, so we have to explicitly
      // check if Chrome is installed, and if so, update its uninstallation
      // command lines.
      const ProductState* chrome_state = original_state.GetProductState(
          installer_state.system_install(),
          BrowserDistribution::CHROME_BROWSER);
      update_chrome_uninstall_command =
          (chrome_state != NULL) && chrome_state->is_multi_install();
    }
  }

  if (!ready_mode || !is_install) {
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
    const ProductState* chrome_state = original_state.GetProductState(
        installer_state.system_install(),
        BrowserDistribution::CHROME_BROWSER);
    if (chrome_state != NULL) {
      DCHECK(chrome_state->is_multi_install());
      Product chrome(BrowserDistribution::GetSpecificDistribution(
                         BrowserDistribution::CHROME_BROWSER));
      chrome.InitializeFromUninstallCommand(chrome_state->uninstall_command());
      AddUninstallShortcutWorkItems(installer_state, setup_path,
                                    chrome_state->version(), list, chrome);
    } else {
      NOTREACHED() << "What happened to Chrome?";
    }
  }
}

void AddElevationPolicyWorkItems(const InstallationState& original_state,
                                 const InstallerState& installer_state,
                                 const Version& new_version,
                                 WorkItemList* install_list) {
  if (!installer_state.is_multi_install()) {
    VLOG(1) << "Not adding elevation policy for single installs";
    return;
  } else {
    const ProductState* cf_state =
        original_state.GetProductState(installer_state.system_install(),
                                       BrowserDistribution::CHROME_FRAME);
    if (cf_state && !cf_state->is_multi_install()) {
      LOG(WARNING) << "Not adding elevation policy since a single install "
                      "of CF exists";
      return;
    }
  }

  FilePath binary_dir(
      GetChromeInstallPath(installer_state.system_install(),
          BrowserDistribution::GetSpecificDistribution(
              BrowserDistribution::CHROME_BINARIES)));

  struct {
    const wchar_t* sub_key;
    const wchar_t* executable;
    const FilePath exe_dir;
  } low_rights_entries[] = {
    { L"ElevationPolicy\\", kChromeLauncherExe,
      binary_dir.Append(ASCIIToWide(new_version.GetString())) },
    { L"DragDrop\\", chrome::kBrowserProcessExecutableName, binary_dir },
  };

  bool uninstall = (installer_state.operation() == InstallerState::UNINSTALL);
  HKEY root = installer_state.root_key();
  const wchar_t kLowRightsKeyPath[] =
      L"SOFTWARE\\Microsoft\\Internet Explorer\\Low Rights\\";
  std::wstring key_path(kLowRightsKeyPath);

  wchar_t cf_classid[64] = {0};
  StringFromGUID2(__uuidof(ChromeFrame), cf_classid, arraysize(cf_classid));

  for (size_t i = 0; i < arraysize(low_rights_entries); ++i) {
    key_path.append(low_rights_entries[i].sub_key).append(cf_classid);
    if (uninstall) {
      install_list->AddDeleteRegKeyWorkItem(root, key_path);
    } else {
      install_list->AddCreateRegKeyWorkItem(root, key_path);
      install_list->AddSetRegValueWorkItem(root, key_path, L"Policy",
          static_cast<DWORD>(3), true);
      install_list->AddSetRegValueWorkItem(root, key_path, L"AppName",
          low_rights_entries[i].executable, true);
      install_list->AddSetRegValueWorkItem(root, key_path, L"AppPath",
          low_rights_entries[i].exe_dir.value(), true);
    }
    key_path.resize(arraysize(kLowRightsKeyPath) - 1);
  }
}

void AppendUninstallCommandLineFlags(const InstallerState& installer_state,
                                     const Product& product,
                                     CommandLine* uninstall_cmd) {
  DCHECK(uninstall_cmd);

  uninstall_cmd->AppendSwitch(installer::switches::kUninstall);

  // Append the product-specific uninstall flags.
  product.AppendProductFlags(uninstall_cmd);
  if (installer_state.is_msi()) {
    uninstall_cmd->AppendSwitch(installer::switches::kMsi);
    // See comment in uninstall.cc where we check for the kDeleteProfile switch.
    if (product.is_chrome_frame()) {
      uninstall_cmd->AppendSwitch(installer::switches::kDeleteProfile);
    }
  }
  if (installer_state.system_install())
    uninstall_cmd->AppendSwitch(installer::switches::kSystemLevel);
  if (installer_state.verbose_logging())
    uninstall_cmd->AppendSwitch(installer::switches::kVerboseLogging);
}

void RefreshElevationPolicy() {
  const wchar_t kIEFrameDll[] = L"ieframe.dll";
  const char kIERefreshPolicy[] = "IERefreshElevationPolicy";

  HMODULE ieframe = LoadLibrary(kIEFrameDll);
  if (ieframe) {
    typedef HRESULT (__stdcall *IERefreshPolicy)();
    IERefreshPolicy ie_refresh_policy = reinterpret_cast<IERefreshPolicy>(
        GetProcAddress(ieframe, kIERefreshPolicy));

    if (ie_refresh_policy) {
      ie_refresh_policy();
    } else {
      VLOG(1) << kIERefreshPolicy << " not supported.";
    }

    FreeLibrary(ieframe);
  } else {
    VLOG(1) << "Cannot load " << kIEFrameDll;
  }
}

void AddQuickEnableWorkItems(const InstallerState& installer_state,
                             const InstallationState& machine_state,
                             const FilePath* setup_path,
                             const Version* new_version,
                             WorkItemList* work_item_list) {
  DCHECK(setup_path ||
         installer_state.operation() == InstallerState::UNINSTALL);
  DCHECK(new_version ||
         installer_state.operation() == InstallerState::UNINSTALL);
  DCHECK(work_item_list);

  const bool system_install = installer_state.system_install();
  bool have_multi_chrome = false;
  bool have_chrome_frame = false;

  // STEP 1: Figure out the state of the machine before the operation.
  const ProductState* product_state = NULL;

  // Is multi-install Chrome already on the machine?
  product_state =
      machine_state.GetProductState(system_install,
                                    BrowserDistribution::CHROME_BROWSER);
  if (product_state != NULL && product_state->is_multi_install())
    have_multi_chrome = true;

  // Is Chrome Frame !ready-mode already on the machine?
  product_state =
      machine_state.GetProductState(system_install,
                                    BrowserDistribution::CHROME_FRAME);
  if (product_state != NULL &&
      !product_state->uninstall_command().HasSwitch(
          switches::kChromeFrameReadyMode))
    have_chrome_frame = true;

  // STEP 2: Now take into account the current operation.
  const Product* product = NULL;

  if (installer_state.operation() == InstallerState::UNINSTALL) {
    // Forget about multi-install Chrome if it is being uninstalled.
    product =
        installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER);
    if (product != NULL && installer_state.is_multi_install())
      have_multi_chrome = false;

    // Forget about Chrome Frame if it is being uninstalled.  Note that we don't
    // bother to check !HasOption(kOptionReadyMode) since have_chrome_frame
    // should have been false for that case in the first place.  It's odd if it
    // wasn't, but the right thing to do in that case is to proceed with the
    // thought that CF will not be installed in any sense when we reach the
    // finish line.
    if (installer_state.FindProduct(BrowserDistribution::CHROME_FRAME) != NULL)
      have_chrome_frame = false;
  } else {
    // Check if we're installing multi-install Chrome.
    product =
        installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER);
    if (product != NULL && installer_state.is_multi_install())
      have_multi_chrome = true;

    // Check if we're installing Chrome Frame !ready-mode.
    product = installer_state.FindProduct(BrowserDistribution::CHROME_FRAME);
    if (product != NULL && !product->HasOption(kOptionReadyMode))
      have_chrome_frame = true;
  }

  // STEP 3: Decide what to do based on the final state of things.
  enum QuickEnableOperation {
    DO_NOTHING,
    ADD_COMMAND,
    REMOVE_COMMAND
  } operation = DO_NOTHING;
  FilePath binaries_setup_path;

  if (have_chrome_frame) {
    // Chrome Frame !ready-mode is or will be installed.  Unconditionally remove
    // the quick-enable-cf command from the binaries.  We do this even if
    // multi-install Chrome isn't installed since we don't want them left
    // behind in any case.
    operation = REMOVE_COMMAND;
  } else if (have_multi_chrome) {
    // Chrome Frame isn't (to be) installed or is (to be) installed only in
    // ready-mode, while multi-install Chrome is (to be) installed.  Add the
    // quick-enable-cf command to the binaries.
    operation = ADD_COMMAND;
    // The path to setup.exe contains the version of the Chrome binaries, so it
    // takes a little work to get it right.
    if (installer_state.operation() == InstallerState::UNINSTALL) {
      // Chrome Frame is being uninstalled.  Use the path to the currently
      // installed Chrome setup.exe.
      product_state =
          machine_state.GetProductState(system_install,
                                        BrowserDistribution::CHROME_BROWSER);
      DCHECK(product_state);
      binaries_setup_path = product_state->uninstall_command().GetProgram();
    } else {
      // Chrome is being installed, updated, or otherwise being operated on.
      // Use the path to the given |setup_path| in the normal location of
      // multi-install Chrome of the given |version|.
      DCHECK(installer_state.is_multi_install());
      binaries_setup_path =
          installer_state.GetInstallerDirectory(*new_version).Append(
              setup_path->BaseName());
    }
  }

  // STEP 4: Take action.
  if (operation != DO_NOTHING) {
    // Get the path to the quick-enable-cf command for the binaries.
    BrowserDistribution* binaries =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES);
    std::wstring cmd_key(binaries->GetVersionKey());
    cmd_key.append(1, L'\\').append(google_update::kRegCommandsKey)
        .append(1, L'\\').append(kCmdQuickEnableCf);

    if (operation == ADD_COMMAND) {
      DCHECK(!binaries_setup_path.empty());
      CommandLine cmd_line(binaries_setup_path);
      cmd_line.AppendSwitch(switches::kMultiInstall);
      if (installer_state.system_install())
        cmd_line.AppendSwitch(switches::kSystemLevel);
      if (installer_state.verbose_logging())
        cmd_line.AppendSwitch(switches::kVerboseLogging);
      cmd_line.AppendSwitch(switches::kChromeFrameQuickEnable);
      AppCommand cmd(cmd_line.command_line_string(), true, true);
      cmd.AddWorkItems(installer_state.root_key(), cmd_key, work_item_list);
    } else {
      DCHECK(operation == REMOVE_COMMAND);
      work_item_list->AddDeleteRegKeyWorkItem(installer_state.root_key(),
                                              cmd_key)->set_log_message(
          "removing quick-enable-cf command");
    }
  }
}

}  // namespace installer
