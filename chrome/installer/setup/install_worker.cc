// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the definitions of the installer functions that build
// the WorkItemList used to install the application.

#include "chrome/installer/setup/install_worker.h"

#include <oaidl.h>
#include <shlobj.h>
#include <time.h>

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
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
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome_frame/chrome_tab.h"

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

  install_list->AddCopyTreeWorkItem(setup_path.value(), exe_dst.value(),
                                    temp_path.value(), WorkItem::ALWAYS);

  // In the past, we copied rather than moved for system level installs so that
  // the permissions of %ProgramFiles% would be picked up.  Now that |temp_path|
  // is in %ProgramFiles% for system level installs (and in %LOCALAPPDATA%
  // otherwise), there is no need to do this for the archive.  Setup.exe, on
  // the other hand, is created elsewhere so it must always be copied.
  install_list->AddMoveTreeWorkItem(archive_path.value(), archive_dst.value(),
                                    temp_path.value(), WorkItem::ALWAYS_MOVE);
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
  // machinery if these strings were not also updated). The UninstallString
  // value placed in the client state key is also used by the mini_installer to
  // locate the setup.exe instance used for binary patching.
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
      uninstall_arguments.GetCommandLineString(), true);

  // MSI installations will manage their own uninstall shortcuts.
  if (!installer_state.is_msi() && product.ShouldCreateUninstallEntry()) {
    // We need to quote the command line for the Add/Remove Programs dialog.
    CommandLine quoted_uninstall_cmd(installer_path);
    DCHECK_EQ(quoted_uninstall_cmd.GetCommandLineString()[0], '"');
    quoted_uninstall_cmd.AppendArguments(uninstall_arguments, false);

    std::wstring uninstall_reg = browser_dist->GetUninstallRegPath();
    install_list->AddCreateRegKeyWorkItem(reg_root, uninstall_reg);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
        installer::kUninstallDisplayNameField,
        browser_dist->GetAppShortCutName(), true);
    install_list->AddSetRegValueWorkItem(reg_root,
        uninstall_reg, installer::kUninstallStringField,
        quoted_uninstall_cmd.GetCommandLineString(), true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         L"InstallLocation",
                                         install_path.value(),
                                         true);

    // DisplayIcon, NoModify and NoRepair
    std::wstring chrome_icon = ShellUtil::GetChromeIcon(
        product.distribution(),
        install_path.Append(installer::kChromeExe).value());
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"DisplayIcon", chrome_icon, true);
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
                                         ASCIIToWide(new_version.GetString()),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"DisplayVersion",
                                         ASCIIToWide(new_version.GetString()),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         L"InstallDate",
                                         InstallUtil::GetCurrentDate(),
                                         false);

    const std::vector<uint16>& version_components = new_version.components();
    if (version_components.size() == 4) {
      // Our version should be in major.minor.build.rev.
      install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
          L"VersionMajor", static_cast<DWORD>(version_components[2]), true);
      install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
          L"VersionMinor", static_cast<DWORD>(version_components[3]), true);
    }
  }
}

// Add uninstall-related work items for multi-install scenarios.
void AddMultiUninstallWorkItems(const InstallerState& installer_state,
                                const FilePath& setup_path,
                                const Version& new_version,
                                WorkItemList* install_list) {
  DCHECK(installer_state.is_multi_install());

  // The mini_installer needs a reliable way to locate setup.exe for diff
  // updates. For single-installs, the product's ClientState key is consulted
  // (Chrome's or Chrome Frame's). For multi-installs, the binaries' key is
  // used.
  const HKEY reg_root = installer_state.root_key();
  std::wstring binaries_state_key(
      installer_state.multi_package_binaries_distribution()->GetStateKey());
  FilePath installer_path(
      installer_state.GetInstallerDirectory(new_version)
          .Append(setup_path.BaseName()));
  install_list->AddCreateRegKeyWorkItem(reg_root, binaries_state_key);
  install_list->AddSetRegValueWorkItem(reg_root, binaries_state_key,
      installer::kUninstallStringField, installer_path.value(), true);
}

// Create Version key for a product (if not already present) and sets the new
// product version as the last step.
void AddVersionKeyWorkItems(HKEY root,
                            BrowserDistribution* dist,
                            const Version& new_version,
                            bool add_language_identifier,
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
  if (add_language_identifier) {
    // Write the language identifier of the current translation.  Omaha's set of
    // languages is a superset of Chrome's set of translations with this one
    // exception: what Chrome calls "en-us", Omaha calls "en".  sigh.
    std::wstring language(GetCurrentTranslation());
    if (LowerCaseEqualsASCII(language, "en-us"))
      language.resize(2);
    list->AddSetRegValueWorkItem(root, version_key,
                                 google_update::kRegLangField, language,
                                 false);  // do not overwrite language
  }
  list->AddSetRegValueWorkItem(root, version_key,
                               google_update::kRegVersionField,
                               ASCIIToWide(new_version.GetString()),
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

// Mirror oeminstall the first time anything is installed multi.  There is no
// need to update the value on future install/update runs since this value never
// changes.  Note that the value is removed by Google Update after EULA
// acceptance is processed.
void AddOemInstallWorkItems(const InstallationState& original_state,
                            const InstallerState& installer_state,
                            WorkItemList* install_list) {
  DCHECK(installer_state.is_multi_install());
  const bool system_install = installer_state.system_install();
  if (!original_state.GetProductState(system_install,
                                      BrowserDistribution::CHROME_BINARIES)) {
    const HKEY root_key = installer_state.root_key();
    std::wstring multi_key(
        installer_state.multi_package_binaries_distribution()->GetStateKey());

    // Copy the value from Chrome unless Chrome isn't installed or being
    // installed.
    BrowserDistribution::Type source_type;
    if (installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER)) {
      source_type = BrowserDistribution::CHROME_BROWSER;
    } else if (!installer_state.products().empty()) {
      // Pick a product, any product.
      source_type = installer_state.products()[0]->distribution()->GetType();
    } else {
      // Nothing is being installed?  Entirely unexpected, so do no harm.
      LOG(ERROR) << "No products found in AddOemInstallWorkItems";
      return;
    }
    const ProductState* source_product =
        original_state.GetNonVersionedProductState(system_install, source_type);

    std::wstring oem_install;
    if (source_product->GetOemInstall(&oem_install)) {
      VLOG(1) << "Mirroring oeminstall=\"" << oem_install << "\" from "
              << BrowserDistribution::GetSpecificDistribution(source_type)
                     ->GetAppShortCutName();
      install_list->AddCreateRegKeyWorkItem(root_key, multi_key);
      // Always overwrite an old value.
      install_list->AddSetRegValueWorkItem(root_key, multi_key,
                                           google_update::kRegOemInstallField,
                                           oem_install, true);
    } else {
      // Clear any old value.
      install_list->AddDeleteRegValueWorkItem(
          root_key, multi_key, google_update::kRegOemInstallField);
    }
  }
}

// Mirror eulaaccepted the first time anything is installed multi.  There is no
// need to update the value on future install/update runs since
// GoogleUpdateSettings::SetEULAConsent will modify the value for both the
// relevant product and for the binaries.
void AddEulaAcceptedWorkItems(const InstallationState& original_state,
                              const InstallerState& installer_state,
                              WorkItemList* install_list) {
  DCHECK(installer_state.is_multi_install());
  const bool system_install = installer_state.system_install();
  if (!original_state.GetProductState(system_install,
                                      BrowserDistribution::CHROME_BINARIES)) {
    const HKEY root_key = installer_state.root_key();
    std::wstring multi_key(
        installer_state.multi_package_binaries_distribution()->GetStateKey());

    // Copy the value from the product with the greatest value.
    bool have_eula_accepted = false;
    BrowserDistribution::Type product_type;
    DWORD eula_accepted;
    const Products& products = installer_state.products();
    for (size_t i = 0, count = products.size(); i != count; ++i) {
      DWORD dword_value = 0;
      BrowserDistribution::Type this_type =
          products[i]->distribution()->GetType();
      const ProductState* product_state =
          original_state.GetNonVersionedProductState(
              system_install, this_type);
      if (product_state->GetEulaAccepted(&dword_value) &&
          (!have_eula_accepted || dword_value > eula_accepted)) {
        have_eula_accepted = true;
        eula_accepted = dword_value;
        product_type = this_type;
      }
    }

    if (have_eula_accepted) {
      VLOG(1) << "Mirroring eulaaccepted=" << eula_accepted << " from "
              << BrowserDistribution::GetSpecificDistribution(product_type)
                     ->GetAppShortCutName();
      install_list->AddCreateRegKeyWorkItem(root_key, multi_key);
      install_list->AddSetRegValueWorkItem(
          root_key, multi_key, google_update::kRegEULAAceptedField,
          eula_accepted, true);
    } else {
      // Clear any old value.
      install_list->AddDeleteRegValueWorkItem(
          root_key, multi_key, google_update::kRegEULAAceptedField);
    }
  }
}

// Adds work items that make registry adjustments for Google Update.
void AddGoogleUpdateWorkItems(const InstallationState& original_state,
                              const InstallerState& installer_state,
                              WorkItemList* install_list) {
  // Is a multi-install product being installed or over-installed?
  if (installer_state.operation() != InstallerState::MULTI_INSTALL &&
      installer_state.operation() != InstallerState::MULTI_UPDATE) {
    VLOG(1) << "AddGoogleUpdateWorkItems noop: " << installer_state.operation();
    return;
  }

  const bool system_install = installer_state.system_install();
  const HKEY root_key = installer_state.root_key();
  std::wstring multi_key(
      installer_state.multi_package_binaries_distribution()->GetStateKey());

  // For system-level installs, make sure the ClientStateMedium key for the
  // binaries exists.
  if (system_install) {
    install_list->AddCreateRegKeyWorkItem(
        root_key,
        installer_state.multi_package_binaries_distribution()->
            GetStateMediumKey().c_str());
  }

  // Creating the ClientState key for binaries, if we're migrating to multi then
  // copy over Chrome's brand code if it has one. Chrome Frame currently never
  // has a brand code.
  if (installer_state.state_type() != BrowserDistribution::CHROME_BINARIES) {
    const ProductState* chrome_product_state =
        original_state.GetNonVersionedProductState(
            system_install, BrowserDistribution::CHROME_BROWSER);

    const std::wstring& brand(chrome_product_state->brand());
    if (!brand.empty()) {
      install_list->AddCreateRegKeyWorkItem(root_key, multi_key);
      // Write Chrome's brand code to the multi key. Never overwrite the value
      // if one is already present (although this shouldn't happen).
      install_list->AddSetRegValueWorkItem(root_key,
                                           multi_key,
                                           google_update::kRegBrandField,
                                           brand,
                                           false);
    }
  }

  AddOemInstallWorkItems(original_state, installer_state, install_list);

  AddEulaAcceptedWorkItems(original_state, installer_state, install_list);

  AddUsageStatsWorkItems(original_state, installer_state, install_list);

  // TODO(grt): check for other keys/values we should put in the package's
  // ClientState and/or Clients key.
}

void AddUsageStatsWorkItems(const InstallationState& original_state,
                            const InstallerState& installer_state,
                            WorkItemList* install_list) {
  DCHECK(installer_state.operation() == InstallerState::MULTI_INSTALL ||
         installer_state.operation() == InstallerState::MULTI_UPDATE);

  HKEY root_key = installer_state.root_key();
  bool value_found = false;
  DWORD usagestats = 0;
  const Products& products = installer_state.products();

  // Search for an existing usagestats value for any product.
  for (Products::const_iterator scan = products.begin(), end = products.end();
       !value_found && scan != end; ++scan) {
    BrowserDistribution* dist = (*scan)->distribution();
    const ProductState* product_state =
        original_state.GetNonVersionedProductState(
            installer_state.system_install(), dist->GetType());
    value_found = product_state->GetUsageStats(&usagestats);
  }

  // If a value was found, write it in the appropriate location for the
  // binaries and remove all values from the products.
  if (value_found) {
    std::wstring state_key(
        installer_state.multi_package_binaries_distribution()->GetStateKey());
    install_list->AddCreateRegKeyWorkItem(root_key, state_key);
    // Overwrite any existing value so that overinstalls (where Omaha writes a
    // new value into a product's state key) pick up the correct value.
    install_list->AddSetRegValueWorkItem(root_key, state_key,
                                         google_update::kRegUsageStatsField,
                                         usagestats, true);

    for (Products::const_iterator scan = products.begin(), end = products.end();
         scan != end; ++scan) {
      BrowserDistribution* dist = (*scan)->distribution();
      if (installer_state.system_install()) {
        install_list->AddDeleteRegValueWorkItem(
            root_key, dist->GetStateMediumKey(),
            google_update::kRegUsageStatsField);
        // Previous versions of Chrome also wrote a value in HKCU even for
        // system-level installs, so clean that up.
        install_list->AddDeleteRegValueWorkItem(
            HKEY_CURRENT_USER, dist->GetStateKey(),
            google_update::kRegUsageStatsField);
      }
      install_list->AddDeleteRegValueWorkItem(root_key, dist->GetStateKey(),
          google_update::kRegUsageStatsField);
    }
  }
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
  // We update the 'opv' value with the current version that is active,
  // the 'cpv' value with the critical update version (if present), and the
  // 'cmd' value with the rename command to run.
  {
    scoped_ptr<WorkItemList> in_use_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new ConditionRunIfFileExists(new_chrome_exe)));
    in_use_update_work_items->set_log_message("InUseUpdateWorkItemList");

    // |critical_version| will be valid only if this in-use update includes a
    // version considered critical relative to the version being updated.
    Version critical_version(installer_state.DetermineCriticalVersion(
        current_version, new_version));
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
            ASCIIToWide(current_version->GetString()), true);
      }
      if (critical_version.IsValid()) {
        in_use_update_work_items->AddSetRegValueWorkItem(root, version_key,
            google_update::kRegCriticalVersionField,
            ASCIIToWide(critical_version.GetString()), true);
      } else {
        in_use_update_work_items->AddDeleteRegValueWorkItem(root, version_key,
            google_update::kRegCriticalVersionField);
      }

      // Adding this registry entry for all products is overkill.
      // However, as it stands, we don't have a way to know which distribution
      // will check the key and run the command, so we add it for all.  The
      // first to run it will perform the operation and clean up the other
      // values.
      CommandLine product_rename_cmd(rename);
      products[i]->AppendRenameFlags(&product_rename_cmd);
      in_use_update_work_items->AddSetRegValueWorkItem(
          root, version_key, google_update::kRegRenameCmdField,
          product_rename_cmd.GetCommandLineString(), true);
    }

    if (current_version != NULL && installer_state.is_multi_install()) {
      BrowserDistribution* dist =
          installer_state.multi_package_binaries_distribution();
      version_key = dist->GetVersionKey();
      in_use_update_work_items->AddSetRegValueWorkItem(
          root, version_key, google_update::kRegOldVersionField,
          ASCIIToWide(current_version->GetString()), true);
      if (critical_version.IsValid()) {
        in_use_update_work_items->AddSetRegValueWorkItem(
            root, version_key, google_update::kRegCriticalVersionField,
            ASCIIToWide(critical_version.GetString()), true);
      } else {
        in_use_update_work_items->AddDeleteRegValueWorkItem(
            root, version_key, google_update::kRegCriticalVersionField);
      }
      // TODO(tommi): We should move the rename command here. We also need to
      // update upgrade_utils::SwapNewChromeExeIfPresent.
    }

    if (installer_state.FindProduct(BrowserDistribution::CHROME_FRAME)) {
      AddCopyIELowRightsPolicyWorkItems(installer_state,
                                        in_use_update_work_items.get());
    }

    post_install_task_list->AddWorkItem(in_use_update_work_items.release());
  }

  // Append work items that will be executed if this was NOT an in-use update.
  {
    scoped_ptr<WorkItemList> regular_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new Not(new ConditionRunIfFileExists(new_chrome_exe))));
    regular_update_work_items->set_log_message("RegularUpdateWorkItemList");

    // Since this was not an in-use-update, delete 'opv', 'cpv', and 'cmd' keys.
    for (size_t i = 0; i < products.size(); ++i) {
      BrowserDistribution* dist = products[i]->distribution();
      std::wstring version_key(dist->GetVersionKey());
      regular_update_work_items->AddDeleteRegValueWorkItem(root, version_key,
          google_update::kRegOldVersionField);
      regular_update_work_items->AddDeleteRegValueWorkItem(root, version_key,
          google_update::kRegCriticalVersionField);
      regular_update_work_items->AddDeleteRegValueWorkItem(root, version_key,
          google_update::kRegRenameCmdField);
    }

    if (installer_state.FindProduct(BrowserDistribution::CHROME_FRAME)) {
      AddDeleteOldIELowRightsPolicyWorkItems(installer_state,
                                             regular_update_work_items.get());
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

  if (current_version != NULL && current_version->get() != NULL) {
    // Delete the archive from an existing install to save some disk space.  We
    // make this an unconditional work item since there's no need to roll this
    // back; if installation fails we'll be moved to the "-full" channel anyway.
    FilePath old_installer_dir(
        installer_state.GetInstallerDirectory(**current_version));
    FilePath old_archive(old_installer_dir.Append(archive_path.BaseName()));
    install_list->AddDeleteTreeWorkItem(old_archive, temp_path)
        ->set_ignore_failure(true);
  }

  // Delete any new_chrome.exe if present (we will end up creating a new one
  // if required) and then copy chrome.exe
  FilePath new_chrome_exe(target_path.Append(installer::kChromeNewExe));

  install_list->AddDeleteTreeWorkItem(new_chrome_exe, temp_path);

  if (installer_state.IsChromeFrameRunning(original_state)) {
    VLOG(1) << "Chrome Frame in use. Copying to new_chrome.exe";
    install_list->AddCopyTreeWorkItem(
        src_path.Append(installer::kChromeExe).value(),
        new_chrome_exe.value(),
        temp_path.value(),
        WorkItem::ALWAYS);
  } else {
    install_list->AddCopyTreeWorkItem(
        src_path.Append(installer::kChromeExe).value(),
        target_path.Append(installer::kChromeExe).value(),
        temp_path.value(),
        WorkItem::NEW_NAME_IF_IN_USE,
        new_chrome_exe.value());
  }

  // Extra executable for 64 bit systems.
  // NOTE: We check for "not disabled" so that if the API call fails, we play it
  // safe and copy the executable anyway.
  // NOTE: the file wow_helper.exe is only needed for Vista and below.
  if (base::win::OSInfo::GetInstance()->wow64_status() !=
      base::win::OSInfo::WOW64_DISABLED &&
      base::win::GetVersion() <= base::win::VERSION_VISTA) {
    install_list->AddMoveTreeWorkItem(
        src_path.Append(installer::kWowHelperExe).value(),
        target_path.Append(installer::kWowHelperExe).value(),
        temp_path.value(),
        WorkItem::ALWAYS_MOVE);
  }

  // Install kVisualElementsManifest if it is present in |src_path|. No need to
  // make this a conditional work item as if the file is not there now, it will
  // never be.
  if (file_util::PathExists(
          src_path.Append(installer::kVisualElementsManifest))) {
    install_list->AddMoveTreeWorkItem(
        src_path.Append(installer::kVisualElementsManifest).value(),
        target_path.Append(installer::kVisualElementsManifest).value(),
        temp_path.value(),
        WorkItem::ALWAYS_MOVE);
  } else {
    // We do not want to have an old VisualElementsManifest pointing to an old
    // version directory. Delete it as there wasn't a new one to replace it.
    install_list->AddDeleteTreeWorkItem(
        target_path.Append(installer::kVisualElementsManifest),
        temp_path);
  }

  // In the past, we copied rather than moved for system level installs so that
  // the permissions of %ProgramFiles% would be picked up.  Now that |temp_path|
  // is in %ProgramFiles% for system level installs (and in %LOCALAPPDATA%
  // otherwise), there is no need to do this.
  // Note that we pass true for check_duplicates to avoid failing on in-use
  // repair runs if the current_version is the same as the new_version.
  bool check_for_duplicates =
      (current_version != NULL && current_version->get() != NULL &&
       current_version->get()->Equals(new_version));
  install_list->AddMoveTreeWorkItem(
      src_path.AppendASCII(new_version.GetString()).value(),
      target_path.AppendASCII(new_version.GetString()).value(),
      temp_path.value(),
      check_for_duplicates ? WorkItem::CHECK_DUPLICATES :
                             WorkItem::ALWAYS_MOVE);

  // Delete any old_chrome.exe if present (ignore failure if it's in use).
  install_list->AddDeleteTreeWorkItem(
      target_path.Append(installer::kChromeOldExe), temp_path)
      ->set_ignore_failure(true);

  // Copy installer in install directory and
  // add shortcut in Control Panel->Add/Remove Programs.
  AddInstallerCopyTasks(installer_state, setup_path, archive_path, temp_path,
                        new_version, install_list);

  const HKEY root = installer_state.root_key();
  // Only set "lang" for user-level installs since for system-level, the install
  // language may not be related to a given user's runtime language.
  const bool add_language_identifier = !installer_state.system_install();

  const Products& products = installer_state.products();
  for (size_t i = 0; i < products.size(); ++i) {
    const Product* product = products[i];

    AddUninstallShortcutWorkItems(installer_state, setup_path, new_version,
                                  install_list, *product);

    AddVersionKeyWorkItems(root, product->distribution(), new_version,
                           add_language_identifier, install_list);

    AddDelegateExecuteWorkItems(installer_state, src_path, new_version,
                                *product, install_list);
  }

  if (installer_state.is_multi_install()) {
    AddMultiUninstallWorkItems(installer_state, setup_path, new_version,
                               install_list);

    AddVersionKeyWorkItems(root,
        installer_state.multi_package_binaries_distribution(), new_version,
        add_language_identifier, install_list);
  }

  // Add any remaining work items that involve special settings for
  // each product.
  AddProductSpecificWorkItems(original_state, installer_state, setup_path,
                              new_version, install_list);

  // Copy over brand, usagestats, and other values.
  AddGoogleUpdateWorkItems(original_state, installer_state, install_list);

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
                                   temp_opt_out.GetCommandLineString(), true);
      list->AddSetRegValueWorkItem(root, version_key,
                                   google_update::kRegCFEndTempOptOutCmdField,
                                   end_temp_opt_out.GetCommandLineString(),
                                   true);
      list->AddSetRegValueWorkItem(root, version_key,
                                   google_update::kRegCFOptOutCmdField,
                                   opt_out.GetCommandLineString(), true);
      list->AddSetRegValueWorkItem(root, version_key,
                                   google_update::kRegCFOptInCmdField,
                                   opt_in.GetCommandLineString(), true);
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

void AddDelegateExecuteWorkItems(const InstallerState& installer_state,
                                 const FilePath& src_path,
                                 const Version& new_version,
                                 const Product& product,
                                 WorkItemList* list) {
  string16 handler_class_uuid;
  string16 type_lib_uuid;
  string16 type_lib_version;
  string16 interface_uuid;
  BrowserDistribution* distribution = product.distribution();
  if (!distribution->GetDelegateExecuteHandlerData(
          &handler_class_uuid, &type_lib_uuid, &type_lib_version,
          &interface_uuid)) {
    VLOG(1) << "No DelegateExecute verb handler processing to do for "
            << distribution->GetAppShortCutName();
    return;
  }

  HKEY root = installer_state.root_key();
  const bool is_install =
      (installer_state.operation() != InstallerState::UNINSTALL);
  string16 delegate_execute_path(L"Software\\Classes\\CLSID\\");
  delegate_execute_path.append(handler_class_uuid);
  string16 typelib_path(L"Software\\Classes\\TypeLib\\");
  typelib_path.append(type_lib_uuid);
  string16 interface_path(L"Software\\Classes\\Interface\\");
  interface_path.append(interface_uuid);

  // Add work items to register the handler iff it is present.  Remove its
  // registration otherwise since builds after r132190 included it when it
  // wasn't strictly necessary.
  // TODO(grt): remove the extra check for the .exe when it's ever-present;
  // see also shell_util.cc's GetProgIdEntries.
  if (is_install &&
      file_util::PathExists(src_path.AppendASCII(new_version.GetString())
          .Append(kDelegateExecuteExe))) {
    VLOG(1) << "Adding registration items for DelegateExecute verb handler.";

    // The path to the exe (in the version directory).
    FilePath delegate_execute(
        installer_state.target_path().AppendASCII(new_version.GetString()));
    delegate_execute = delegate_execute.Append(kDelegateExecuteExe);

    // Command-line featuring the quoted path to the exe.
    string16 command(1, L'"');
    command.append(delegate_execute.value()).append(1, L'"');

    // Register the CommandExecuteImpl class at
    // Software\Classes\CLSID\{5C65F4B0-3651-4514-B207-D10CB699B14B}
    list->AddCreateRegKeyWorkItem(root, delegate_execute_path);
    list->AddSetRegValueWorkItem(root, delegate_execute_path, L"",
                                 L"CommandExecuteImpl Class", true);
    string16 subkey(delegate_execute_path);
    subkey.append(L"\\LocalServer32");
    list->AddCreateRegKeyWorkItem(root, subkey);
    list->AddSetRegValueWorkItem(root, subkey, L"", command, true);
    list->AddSetRegValueWorkItem(root, subkey, L"ServerExecutable",
                                 delegate_execute.value(), true);

    subkey.assign(delegate_execute_path).append(L"\\Programmable");
    list->AddCreateRegKeyWorkItem(root, subkey);

    subkey.assign(delegate_execute_path).append(L"\\TypeLib");
    list->AddCreateRegKeyWorkItem(root, subkey);
    list->AddSetRegValueWorkItem(root, subkey, L"", type_lib_uuid, true);

    subkey.assign(delegate_execute_path).append(L"\\Version");
    list->AddCreateRegKeyWorkItem(root, subkey);
    list->AddSetRegValueWorkItem(root, subkey, L"", type_lib_version, true);

    // Register the DelegateExecuteLib type library at
    // Software\Classes\TypeLib\{4E805ED8-EBA0-4601-9681-12815A56EBFD}
    list->AddCreateRegKeyWorkItem(root, typelib_path);

    string16 version_key(typelib_path);
    version_key.append(1, L'\\').append(type_lib_version);
    list->AddCreateRegKeyWorkItem(root, version_key);
    list->AddSetRegValueWorkItem(root, version_key, L"", L"DelegateExecuteLib",
                                 true);

    subkey.assign(version_key).append(L"\\FLAGS");
    const DWORD flags = LIBFLAG_FRESTRICTED | LIBFLAG_FCONTROL;
    list->AddCreateRegKeyWorkItem(root, subkey);
    list->AddSetRegValueWorkItem(root, subkey, L"", flags, true);

    subkey.assign(version_key).append(L"\\0");
    list->AddCreateRegKeyWorkItem(root, subkey);

    subkey.append(L"\\win32");
    list->AddCreateRegKeyWorkItem(root, subkey);
    list->AddSetRegValueWorkItem(root, subkey, L"", delegate_execute.value(),
                                 true);

    subkey.assign(version_key).append(L"\\HELPDIR");
    list->AddCreateRegKeyWorkItem(root, subkey);
    list->AddSetRegValueWorkItem(root, subkey, L"",
                                 delegate_execute.DirName().value(), true);

    // Register to ICommandExecuteImpl interface at
    // Software\Classes\Interface\{0BA0D4E9-2259-4963-B9AE-A839F7CB7544}
    list->AddCreateRegKeyWorkItem(root, interface_path);
    list->AddSetRegValueWorkItem(root, interface_path, L"",
                                 L"ICommandExecuteImpl", true);

    subkey.assign(interface_path).append(L"\\ProxyStubClsid32");
    list->AddCreateRegKeyWorkItem(root, subkey);
    list->AddSetRegValueWorkItem(root, subkey, L"", kPSOAInterfaceUuid, true);

    subkey.assign(interface_path).append(L"\\TypeLib");
    list->AddCreateRegKeyWorkItem(root, subkey);
    list->AddSetRegValueWorkItem(root, subkey, L"", type_lib_uuid, true);
    list->AddSetRegValueWorkItem(root, subkey, L"Version", type_lib_version,
                                 true);

  } else {
    VLOG(1) << "Adding unregistration items for DelegateExecute verb handler.";

    list->AddDeleteRegKeyWorkItem(root, delegate_execute_path);
    list->AddDeleteRegKeyWorkItem(root, typelib_path);
    list->AddDeleteRegKeyWorkItem(root, interface_path);
  }
}

namespace {

enum ElevationPolicyId {
  CURRENT_ELEVATION_POLICY,
  OLD_ELEVATION_POLICY,
};

// Although the UUID of the ChromeFrame class is used for the "current" value,
// this is done only as a convenience; there is no need for the GUID of the Low
// Rights policies to match the ChromeFrame class's GUID.  Hence, it is safe to
// use this completely unrelated GUID for the "old" policies.
const wchar_t kIELowRightsPolicyOldGuid[] =
    L"{6C288DD7-76FB-4721-B628-56FAC252E199}";

const wchar_t kElevationPolicyKeyPath[] =
    L"SOFTWARE\\Microsoft\\Internet Explorer\\Low Rights\\ElevationPolicy\\";

void GetIELowRightsElevationPolicyKeyPath(ElevationPolicyId policy,
                                          std::wstring* key_path) {
  DCHECK(policy == CURRENT_ELEVATION_POLICY || policy == OLD_ELEVATION_POLICY);

  key_path->assign(kElevationPolicyKeyPath,
                   arraysize(kElevationPolicyKeyPath) - 1);
  if (policy == CURRENT_ELEVATION_POLICY) {
    wchar_t cf_clsid[64];
    int len = StringFromGUID2(__uuidof(ChromeFrame), &cf_clsid[0],
                              arraysize(cf_clsid));
    key_path->append(&cf_clsid[0], len - 1);
  } else {
    key_path->append(kIELowRightsPolicyOldGuid,
                     arraysize(kIELowRightsPolicyOldGuid)- 1);
  }
}

}  // namespace

void AddDeleteOldIELowRightsPolicyWorkItems(
    const InstallerState& installer_state,
    WorkItemList* install_list) {
  DCHECK(install_list);

  std::wstring key_path;
  GetIELowRightsElevationPolicyKeyPath(OLD_ELEVATION_POLICY, &key_path);
  install_list->AddDeleteRegKeyWorkItem(installer_state.root_key(), key_path);
}

// Adds work items to copy the chrome_launcher IE low rights elevation policy
// from the primary policy GUID to the "old" policy GUID.  Take care not to
// perform the copy if there is already an old policy present, as the ones under
// the main kElevationPolicyGuid would then correspond to an intermediate
// version (current_version < pv < new_version).
void AddCopyIELowRightsPolicyWorkItems(const InstallerState& installer_state,
                                       WorkItemList* install_list) {
  DCHECK(install_list);

  std::wstring current_key_path;
  std::wstring old_key_path;

  GetIELowRightsElevationPolicyKeyPath(CURRENT_ELEVATION_POLICY,
                                       &current_key_path);
  GetIELowRightsElevationPolicyKeyPath(OLD_ELEVATION_POLICY, &old_key_path);
  // Do not clobber existing old policies.
  install_list->AddCopyRegKeyWorkItem(installer_state.root_key(),
                                      current_key_path, old_key_path,
                                      WorkItem::IF_NOT_PRESENT);
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
      AppCommand cmd(cmd_line.GetCommandLineString(), true, true);
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
