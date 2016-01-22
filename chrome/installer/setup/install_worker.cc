// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the definitions of the installer functions that build
// the WorkItemList used to install the application.

#include "chrome/installer/setup/install_worker.h"

#include <oaidl.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/setup/app_launcher_installer.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/update_active_setup_version_work_item.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/firewall_manager_win.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

using base::ASCIIToUTF16;
using base::win::RegKey;

namespace installer {

namespace {

// Although the UUID of the ChromeFrame class is used for the "current" value,
// this is done only as a convenience; there is no need for the GUID of the Low
// Rights policies to match the ChromeFrame class's GUID.  Hence, it is safe to
// use this completely unrelated GUID for the "old" policies.
const wchar_t kIELowRightsPolicyOldGuid[] =
    L"{6C288DD7-76FB-4721-B628-56FAC252E199}";

const wchar_t kElevationPolicyKeyPath[] =
    L"SOFTWARE\\Microsoft\\Internet Explorer\\Low Rights\\ElevationPolicy\\";

void GetOldIELowRightsElevationPolicyKeyPath(base::string16* key_path) {
  key_path->assign(kElevationPolicyKeyPath,
                   arraysize(kElevationPolicyKeyPath) - 1);
  key_path->append(kIELowRightsPolicyOldGuid,
                   arraysize(kIELowRightsPolicyOldGuid)- 1);
}

// Local helper to call AddRegisterComDllWorkItems for all DLLs in a set of
// products managed by a given package.
// |old_version| can be NULL to indicate no Chrome is currently installed.
void AddRegisterComDllWorkItemsForPackage(const InstallerState& installer_state,
                                          const Version* old_version,
                                          const Version& new_version,
                                          WorkItemList* work_item_list) {
  // First collect the list of DLLs to be registered from each product.
  std::vector<base::FilePath> com_dll_list;
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
      base::FilePath old_dll_path(installer_state.target_path().AppendASCII(
          old_version->GetString()));

      installer::AddRegisterComDllWorkItems(old_dll_path,
                                            com_dll_list,
                                            installer_state.system_install(),
                                            false,  // Unregister
                                            true,   // May fail
                                            work_item_list);
    }

    base::FilePath dll_path(installer_state.target_path().AppendASCII(
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
                           const base::FilePath& setup_path,
                           const base::FilePath& archive_path,
                           const base::FilePath& temp_path,
                           const Version& new_version,
                           WorkItemList* install_list) {
  DCHECK(install_list);
  base::FilePath installer_dir(
      installer_state.GetInstallerDirectory(new_version));
  install_list->AddCreateDirWorkItem(installer_dir);

  base::FilePath exe_dst(installer_dir.Append(setup_path.BaseName()));

  if (exe_dst != setup_path) {
    install_list->AddCopyTreeWorkItem(setup_path.value(), exe_dst.value(),
                                      temp_path.value(), WorkItem::ALWAYS);
  }

  if (installer_state.RequiresActiveSetup()) {
    // Make a copy of setup.exe with a different name so that Active Setup
    // doesn't require an admin on XP thanks to Application Compatibility.
    base::FilePath active_setup_exe(installer_dir.Append(kActiveSetupExe));
    install_list->AddCopyTreeWorkItem(
        setup_path.value(), active_setup_exe.value(), temp_path.value(),
        WorkItem::ALWAYS);
  }

  base::FilePath archive_dst(installer_dir.Append(archive_path.BaseName()));
  if (archive_path != archive_dst) {
    // In the past, we copied rather than moved for system level installs so
    // that the permissions of %ProgramFiles% would be picked up.  Now that
    // |temp_path| is in %ProgramFiles% for system level installs (and in
    // %LOCALAPPDATA% otherwise), there is no need to do this for the archive.
    // Setup.exe, on the other hand, is created elsewhere so it must always be
    // copied.
    if (temp_path.IsParent(archive_path)) {
      install_list->AddMoveTreeWorkItem(archive_path.value(),
                                        archive_dst.value(),
                                        temp_path.value(),
                                        WorkItem::ALWAYS_MOVE);
    } else {
      // This may occur when setup is run out of an existing installation
      // directory. We cannot remove the system-level archive.
      install_list->AddCopyTreeWorkItem(archive_path.value(),
                                        archive_dst.value(),
                                        temp_path.value(),
                                        WorkItem::ALWAYS);
    }
  }
}

base::string16 GetRegCommandKey(BrowserDistribution* dist,
                                const wchar_t* name) {
  return GetRegistrationDataCommandKey(dist->GetAppRegistrationData(), name);
}

// A callback invoked by |work_item| that adds firewall rules for Chrome. Rules
// are left in-place on rollback unless |remove_on_rollback| is true. This is
// the case for new installs only. Updates and overinstalls leave the rule
// in-place on rollback since a previous install of Chrome will be used in that
// case.
bool AddFirewallRulesCallback(bool system_level,
                              BrowserDistribution* dist,
                              const base::FilePath& chrome_path,
                              bool remove_on_rollback,
                              const CallbackWorkItem& work_item) {
  // There is no work to do on rollback if this is not a new install.
  if (work_item.IsRollback() && !remove_on_rollback)
    return true;

  scoped_ptr<FirewallManager> manager =
      FirewallManager::Create(dist, chrome_path);
  if (!manager) {
    LOG(ERROR) << "Failed creating a FirewallManager. Continuing with install.";
    return true;
  }

  if (work_item.IsRollback()) {
    manager->RemoveFirewallRules();
    return true;
  }

  // Adding the firewall rule is expected to fail for user-level installs on
  // Vista+. Try anyway in case the installer is running elevated.
  if (!manager->AddFirewallRules())
    LOG(ERROR) << "Failed creating a firewall rules. Continuing with install.";

  // Don't abort installation if the firewall rule couldn't be added.
  return true;
}

// Adds work items to |list| to create firewall rules.
void AddFirewallRulesWorkItems(const InstallerState& installer_state,
                               BrowserDistribution* dist,
                               bool is_new_install,
                               WorkItemList* list) {
  list->AddCallbackWorkItem(
      base::Bind(&AddFirewallRulesCallback,
                 installer_state.system_install(),
                 dist,
                 installer_state.target_path().Append(kChromeExe),
                 is_new_install));
}

void AddProductSpecificWorkItems(const InstallationState& original_state,
                                 const InstallerState& installer_state,
                                 const base::FilePath& setup_path,
                                 const Version& new_version,
                                 bool is_new_install,
                                 bool add_language_identifier,
                                 WorkItemList* list) {
  const Products& products = installer_state.products();
  for (Products::const_iterator it = products.begin(); it < products.end();
       ++it) {
    const Product& p = **it;
    if (p.is_chrome()) {
      AddOsUpgradeWorkItems(installer_state, setup_path, new_version, p,
                            list);
      AddFirewallRulesWorkItems(
          installer_state, p.distribution(), is_new_install, list);

#if defined(GOOGLE_CHROME_BUILD)
      if (!InstallUtil::IsChromeSxSProcess()) {
        // Add items to set up the App Launcher's version key if Google Chrome
        // is being installed or updated.
        AddAppLauncherVersionKeyWorkItems(installer_state.root_key(),
            new_version, add_language_identifier, list);
      }
#endif  // GOOGLE_CHROME_BUILD
    }
    if (p.is_chrome_binaries())
      AddQuickEnableChromeFrameWorkItems(installer_state, list);
  }
}

// This is called when an MSI installation is run. It may be that a user is
// attempting to install the MSI on top of a non-MSI managed installation. If
// so, try and remove any existing "Add/Remove Programs" entry, as we want the
// uninstall to be managed entirely by the MSI machinery (accessible via the
// Add/Remove programs dialog).
void AddDeleteUninstallEntryForMSIWorkItems(
    const InstallerState& installer_state,
    const Product& product,
    WorkItemList* work_item_list) {
  DCHECK(installer_state.is_msi())
      << "This must only be called for MSI installations!";

  HKEY reg_root = installer_state.root_key();
  base::string16 uninstall_reg(product.distribution()->GetUninstallRegPath());

  WorkItem* delete_reg_key = work_item_list->AddDeleteRegKeyWorkItem(
      reg_root, uninstall_reg, KEY_WOW64_32KEY);
  delete_reg_key->set_ignore_failure(true);
}

// Adds Chrome specific install work items to |install_list|.
// |current_version| can be NULL to indicate no Chrome is currently installed.
void AddChromeWorkItems(const InstallationState& original_state,
                        const InstallerState& installer_state,
                        const base::FilePath& setup_path,
                        const base::FilePath& archive_path,
                        const base::FilePath& src_path,
                        const base::FilePath& temp_path,
                        const Version* current_version,
                        const Version& new_version,
                        WorkItemList* install_list) {
  const base::FilePath& target_path = installer_state.target_path();

  if (current_version) {
    // Delete the archive from an existing install to save some disk space.  We
    // make this an unconditional work item since there's no need to roll this
    // back; if installation fails we'll be moved to the "-full" channel anyway.
    base::FilePath old_installer_dir(
        installer_state.GetInstallerDirectory(*current_version));
    base::FilePath old_archive(
        old_installer_dir.Append(installer::kChromeArchive));
    // Don't delete the archive that we are actually installing from.
    if (archive_path != old_archive) {
      install_list->AddDeleteTreeWorkItem(old_archive, temp_path)->
          set_ignore_failure(true);
    }
  }

  // Delete any new_chrome.exe if present (we will end up creating a new one
  // if required) and then copy chrome.exe
  base::FilePath new_chrome_exe(target_path.Append(installer::kChromeNewExe));

  install_list->AddDeleteTreeWorkItem(new_chrome_exe, temp_path);

  // TODO(grt): Remove this check in M35.
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
  if (base::PathExists(
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
  bool check_for_duplicates = (current_version &&
                               current_version->Equals(new_version));
  install_list->AddMoveTreeWorkItem(
      src_path.AppendASCII(new_version.GetString()).value(),
      target_path.AppendASCII(new_version.GetString()).value(),
      temp_path.value(),
      check_for_duplicates ? WorkItem::CHECK_DUPLICATES :
                             WorkItem::ALWAYS_MOVE);

  // Delete any old_chrome.exe if present (ignore failure if it's in use).
  install_list->AddDeleteTreeWorkItem(
      target_path.Append(installer::kChromeOldExe), temp_path)->
          set_ignore_failure(true);
}

// Adds work items to remove COM registration for |product|'s deprecated
// DelegateExecute verb handler.
void AddCleanupDelegateExecuteWorkItems(const InstallerState& installer_state,
                                        const Product& product,
                                        WorkItemList* list) {
  if (product.is_chrome()) {
    VLOG(1) << "Adding unregistration items for DelegateExecute verb handler.";
    const base::string16 handler_class_uuid =
        product.distribution()->GetCommandExecuteImplClsid();
    DCHECK(!handler_class_uuid.empty());

    const HKEY root = installer_state.root_key();
    base::string16 delegate_execute_path(L"Software\\Classes\\CLSID\\");
    delegate_execute_path.append(handler_class_uuid);
    // Delete both 64 and 32 keys to handle 32->64 or 64->32 migration.
    list->AddDeleteRegKeyWorkItem(root, delegate_execute_path, KEY_WOW64_32KEY);
    list->AddDeleteRegKeyWorkItem(root, delegate_execute_path, KEY_WOW64_64KEY);
  }
}

}  // namespace

// This method adds work items to create (or update) Chrome uninstall entry in
// either the Control Panel->Add/Remove Programs list or in the Omaha client
// state key if running under an MSI installer.
void AddUninstallShortcutWorkItems(const InstallerState& installer_state,
                                   const base::FilePath& setup_path,
                                   const Version& new_version,
                                   const Product& product,
                                   WorkItemList* install_list) {
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
  base::FilePath install_path(installer_state.target_path());
  base::FilePath installer_path(
      installer_state.GetInstallerDirectory(new_version));
  installer_path = installer_path.Append(setup_path.BaseName());

  base::CommandLine uninstall_arguments(base::CommandLine::NO_PROGRAM);
  AppendUninstallCommandLineFlags(installer_state, product,
                                  &uninstall_arguments);

  base::string16 update_state_key(browser_dist->GetStateKey());
  install_list->AddCreateRegKeyWorkItem(
      reg_root, update_state_key, KEY_WOW64_32KEY);
  install_list->AddSetRegValueWorkItem(reg_root,
                                       update_state_key,
                                       KEY_WOW64_32KEY,
                                       installer::kUninstallStringField,
                                       installer_path.value(),
                                       true);
  install_list->AddSetRegValueWorkItem(
      reg_root,
      update_state_key,
      KEY_WOW64_32KEY,
      installer::kUninstallArgumentsField,
      uninstall_arguments.GetCommandLineString(),
      true);

  // MSI installations will manage their own uninstall shortcuts.
  if (!installer_state.is_msi() && product.ShouldCreateUninstallEntry()) {
    // We need to quote the command line for the Add/Remove Programs dialog.
    base::CommandLine quoted_uninstall_cmd(installer_path);
    DCHECK_EQ(quoted_uninstall_cmd.GetCommandLineString()[0], '"');
    quoted_uninstall_cmd.AppendArguments(uninstall_arguments, false);

    base::string16 uninstall_reg = browser_dist->GetUninstallRegPath();
    install_list->AddCreateRegKeyWorkItem(
        reg_root, uninstall_reg, KEY_WOW64_32KEY);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         installer::kUninstallDisplayNameField,
                                         browser_dist->GetDisplayName(),
                                         true);
    install_list->AddSetRegValueWorkItem(
        reg_root,
        uninstall_reg,
        KEY_WOW64_32KEY,
        installer::kUninstallStringField,
        quoted_uninstall_cmd.GetCommandLineString(),
        true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"InstallLocation",
                                         install_path.value(),
                                         true);

    BrowserDistribution* dist = product.distribution();
    base::string16 chrome_icon = ShellUtil::FormatIconLocation(
        install_path.Append(dist->GetIconFilename()),
        dist->GetIconIndex(BrowserDistribution::SHORTCUT_CHROME));
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"DisplayIcon",
                                         chrome_icon,
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"NoModify",
                                         static_cast<DWORD>(1),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"NoRepair",
                                         static_cast<DWORD>(1),
                                         true);

    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"Publisher",
                                         browser_dist->GetPublisherName(),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"Version",
                                         ASCIIToUTF16(new_version.GetString()),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"DisplayVersion",
                                         ASCIIToUTF16(new_version.GetString()),
                                         true);
    // TODO(wfh): Ensure that this value is preserved in the 64-bit hive when
    // 64-bit installs place the uninstall information into the 64-bit registry.
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"InstallDate",
                                         InstallUtil::GetCurrentDate(),
                                         false);

    const std::vector<uint32_t>& version_components = new_version.components();
    if (version_components.size() == 4) {
      // Our version should be in major.minor.build.rev.
      install_list->AddSetRegValueWorkItem(
          reg_root,
          uninstall_reg,
          KEY_WOW64_32KEY,
          L"VersionMajor",
          static_cast<DWORD>(version_components[2]),
          true);
      install_list->AddSetRegValueWorkItem(
          reg_root,
          uninstall_reg,
          KEY_WOW64_32KEY,
          L"VersionMinor",
          static_cast<DWORD>(version_components[3]),
          true);
    }
  }
}

// Create Version key for a product (if not already present) and sets the new
// product version as the last step.
void AddVersionKeyWorkItems(HKEY root,
                            const base::string16& version_key,
                            const base::string16& product_name,
                            const Version& new_version,
                            bool add_language_identifier,
                            WorkItemList* list) {
  list->AddCreateRegKeyWorkItem(root, version_key, KEY_WOW64_32KEY);

  list->AddSetRegValueWorkItem(root,
                               version_key,
                               KEY_WOW64_32KEY,
                               google_update::kRegNameField,
                               product_name,
                               true);  // overwrite name also
  list->AddSetRegValueWorkItem(root,
                               version_key,
                               KEY_WOW64_32KEY,
                               google_update::kRegOopcrashesField,
                               static_cast<DWORD>(1),
                               false);  // set during first install
  if (add_language_identifier) {
    // Write the language identifier of the current translation.  Omaha's set of
    // languages is a superset of Chrome's set of translations with this one
    // exception: what Chrome calls "en-us", Omaha calls "en".  sigh.
    base::string16 language(GetCurrentTranslation());
    if (base::LowerCaseEqualsASCII(language, "en-us"))
      language.resize(2);
    list->AddSetRegValueWorkItem(root,
                                 version_key,
                                 KEY_WOW64_32KEY,
                                 google_update::kRegLangField,
                                 language,
                                 false);  // do not overwrite language
  }
  list->AddSetRegValueWorkItem(root,
                               version_key,
                               KEY_WOW64_32KEY,
                               google_update::kRegVersionField,
                               ASCIIToUTF16(new_version.GetString()),
                               true);  // overwrite version
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
    base::string16 multi_key(
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

    base::string16 oem_install;
    if (source_product->GetOemInstall(&oem_install)) {
      VLOG(1) << "Mirroring oeminstall=\"" << oem_install << "\" from "
              << BrowserDistribution::GetSpecificDistribution(source_type)->
                     GetDisplayName();
      install_list->AddCreateRegKeyWorkItem(
          root_key, multi_key, KEY_WOW64_32KEY);
      // Always overwrite an old value.
      install_list->AddSetRegValueWorkItem(root_key,
                                           multi_key,
                                           KEY_WOW64_32KEY,
                                           google_update::kRegOemInstallField,
                                           oem_install,
                                           true);
    } else {
      // Clear any old value.
      install_list->AddDeleteRegValueWorkItem(
          root_key,
          multi_key,
          KEY_WOW64_32KEY,
          google_update::kRegOemInstallField);
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
    base::string16 multi_key(
        installer_state.multi_package_binaries_distribution()->GetStateKey());

    // Copy the value from the product with the greatest value.
    bool have_eula_accepted = false;
    BrowserDistribution::Type product_type = BrowserDistribution::NUM_TYPES;
    DWORD eula_accepted = 0;
    const Products& products = installer_state.products();
    for (Products::const_iterator it = products.begin(); it < products.end();
         ++it) {
      const Product& product = **it;
      if (product.is_chrome_binaries())
        continue;
      DWORD dword_value = 0;
      BrowserDistribution::Type this_type = product.distribution()->GetType();
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
              << BrowserDistribution::GetSpecificDistribution(product_type)->
                     GetDisplayName();
      install_list->AddCreateRegKeyWorkItem(
          root_key, multi_key, KEY_WOW64_32KEY);
      install_list->AddSetRegValueWorkItem(root_key,
                                           multi_key,
                                           KEY_WOW64_32KEY,
                                           google_update::kRegEULAAceptedField,
                                           eula_accepted,
                                           true);
    } else {
      // Clear any old value.
      install_list->AddDeleteRegValueWorkItem(
          root_key,
          multi_key,
          KEY_WOW64_32KEY,
          google_update::kRegEULAAceptedField);
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
  base::string16 multi_key(
      installer_state.multi_package_binaries_distribution()->GetStateKey());

  // For system-level installs, make sure the ClientStateMedium key for the
  // binaries exists.
  if (system_install) {
    install_list->AddCreateRegKeyWorkItem(
        root_key,
        installer_state.multi_package_binaries_distribution()
            ->GetStateMediumKey()
            .c_str(),
        KEY_WOW64_32KEY);
  }

  // Creating the ClientState key for binaries, if we're migrating to multi then
  // copy over Chrome's brand code if it has one.
  if (installer_state.state_type() != BrowserDistribution::CHROME_BINARIES) {
    const ProductState* chrome_product_state =
        original_state.GetNonVersionedProductState(
            system_install, BrowserDistribution::CHROME_BROWSER);

    const base::string16& brand(chrome_product_state->brand());
    if (!brand.empty()) {
      install_list->AddCreateRegKeyWorkItem(
          root_key, multi_key, KEY_WOW64_32KEY);
      // Write Chrome's brand code to the multi key. Never overwrite the value
      // if one is already present (although this shouldn't happen).
      install_list->AddSetRegValueWorkItem(root_key,
                                           multi_key,
                                           KEY_WOW64_32KEY,
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
    if ((*scan)->is_chrome_binaries())
      continue;
    BrowserDistribution* dist = (*scan)->distribution();
    const ProductState* product_state =
        original_state.GetNonVersionedProductState(
            installer_state.system_install(), dist->GetType());
    value_found = product_state->GetUsageStats(&usagestats);
  }

  // If a value was found, write it in the appropriate location for the
  // binaries and remove all values from the products.
  if (value_found) {
    base::string16 state_key(
        installer_state.multi_package_binaries_distribution()->GetStateKey());
    install_list->AddCreateRegKeyWorkItem(root_key, state_key, KEY_WOW64_32KEY);
    // Overwrite any existing value so that overinstalls (where Omaha writes a
    // new value into a product's state key) pick up the correct value.
    install_list->AddSetRegValueWorkItem(root_key,
                                         state_key,
                                         KEY_WOW64_32KEY,
                                         google_update::kRegUsageStatsField,
                                         usagestats,
                                         true);

    for (Products::const_iterator scan = products.begin(), end = products.end();
         scan != end; ++scan) {
      if ((*scan)->is_chrome_binaries())
        continue;
      BrowserDistribution* dist = (*scan)->distribution();
      if (installer_state.system_install()) {
        install_list->AddDeleteRegValueWorkItem(
            root_key,
            dist->GetStateMediumKey(),
            KEY_WOW64_32KEY,
            google_update::kRegUsageStatsField);
        // Previous versions of Chrome also wrote a value in HKCU even for
        // system-level installs, so clean that up.
        install_list->AddDeleteRegValueWorkItem(
            HKEY_CURRENT_USER,
            dist->GetStateKey(),
            KEY_WOW64_32KEY,
            google_update::kRegUsageStatsField);
      }
      install_list->AddDeleteRegValueWorkItem(
          root_key,
          dist->GetStateKey(),
          KEY_WOW64_32KEY,
          google_update::kRegUsageStatsField);
    }
  }
}

// Migrates the usagestats value from the binaries to Chrome when migrating
// multi-install Chrome to single-install.
void AddMigrateUsageStatesWorkItems(const InstallationState& original_state,
                                    const InstallerState& installer_state,
                                    WorkItemList* install_list) {
  // Ensure that a non-multi install or update is being processed (i.e.,
  // no "--multi-install" on the command line).
  if (installer_state.is_multi_install())
    return;

  // This operation doesn't apply to SxS Chrome.
  if (InstallUtil::IsChromeSxSProcess())
    return;

  // Ensure that Chrome is the product being installed or updated (there are no
  // other products, so it is especially unexpected for this to fail).
  const Product* chrome_product =
      installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER);
  if (!chrome_product) {
    LOG(DFATAL) << "Not operating on Chrome while migrating to single-install.";
    return;
  }

  const ProductState* chrome_state = original_state.GetProductState(
      installer_state.system_install(),
      BrowserDistribution::CHROME_BROWSER);
  // Bail out if there is not an existing multi-install Chrome that is being
  // updated.
  if (!chrome_state || !chrome_state->is_multi_install()) {
    VLOG(1) << "No multi-install Chrome found to migrate to single-install.";
    return;
  }

  const ProductState* binaries_state = original_state.GetProductState(
      installer_state.system_install(),
      BrowserDistribution::CHROME_BINARIES);

  // There is nothing to be done if the binaries do not have stats.
  DWORD usagestats = 0;
  if (!binaries_state || !binaries_state->GetUsageStats(&usagestats)) {
    VLOG(1) << "No usagestats value found to migrate to single-install.";
    return;
  }

  VLOG(1) << "Migrating usagestats value from multi-install to single-install.";

  // Write the value that was read to Chrome's ClientState key.
  install_list->AddSetRegValueWorkItem(
      installer_state.root_key(),
      chrome_product->distribution()->GetStateKey(),
      KEY_WOW64_32KEY,
      google_update::kRegUsageStatsField,
      usagestats,
      true);
}

bool AppendPostInstallTasks(const InstallerState& installer_state,
                            const base::FilePath& setup_path,
                            const Version* current_version,
                            const Version& new_version,
                            WorkItemList* post_install_task_list) {
  DCHECK(post_install_task_list);

  HKEY root = installer_state.root_key();
  const Products& products = installer_state.products();
  base::FilePath new_chrome_exe(
      installer_state.target_path().Append(installer::kChromeNewExe));

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
    base::FilePath installer_path(
        installer_state.GetInstallerDirectory(new_version).Append(
            setup_path.BaseName()));

    base::CommandLine rename(installer_path);
    rename.AppendSwitch(switches::kRenameChromeExe);
    if (installer_state.system_install())
      rename.AppendSwitch(switches::kSystemLevel);

    if (installer_state.verbose_logging())
      rename.AppendSwitch(switches::kVerboseLogging);

    base::string16 version_key;
    for (size_t i = 0; i < products.size(); ++i) {
      BrowserDistribution* dist = products[i]->distribution();
      version_key = dist->GetVersionKey();

      if (current_version) {
        in_use_update_work_items->AddSetRegValueWorkItem(
            root,
            version_key,
            KEY_WOW64_32KEY,
            google_update::kRegOldVersionField,
            ASCIIToUTF16(current_version->GetString()),
            true);
      }
      if (critical_version.IsValid()) {
        in_use_update_work_items->AddSetRegValueWorkItem(
            root,
            version_key,
            KEY_WOW64_32KEY,
            google_update::kRegCriticalVersionField,
            ASCIIToUTF16(critical_version.GetString()),
            true);
      } else {
        in_use_update_work_items->AddDeleteRegValueWorkItem(
            root,
            version_key,
            KEY_WOW64_32KEY,
            google_update::kRegCriticalVersionField);
      }

      // Adding this registry entry for all products (but the binaries) is
      // overkill. However, as it stands, we don't have a way to know which
      // product will check the key and run the command, so we add it for all.
      // The first to run it will perform the operation and clean up the other
      // values.
      if (dist->GetType() != BrowserDistribution::CHROME_BINARIES) {
        base::CommandLine product_rename_cmd(rename);
        products[i]->AppendRenameFlags(&product_rename_cmd);
        in_use_update_work_items->AddSetRegValueWorkItem(
            root,
            version_key,
            KEY_WOW64_32KEY,
            google_update::kRegRenameCmdField,
            product_rename_cmd.GetCommandLineString(),
            true);
      }
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
      base::string16 version_key(dist->GetVersionKey());
      regular_update_work_items->AddDeleteRegValueWorkItem(
          root,
          version_key,
          KEY_WOW64_32KEY,
          google_update::kRegOldVersionField);
      regular_update_work_items->AddDeleteRegValueWorkItem(
          root,
          version_key,
          KEY_WOW64_32KEY,
          google_update::kRegCriticalVersionField);
      regular_update_work_items->AddDeleteRegValueWorkItem(
          root,
          version_key,
          KEY_WOW64_32KEY,
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

      // We want MSI installs to take over the Add/Remove Programs entry. Make a
      // best-effort attempt to delete any entry left over from previous non-MSI
      // installations for the same type of install (system or per user).
      if (product->ShouldCreateUninstallEntry()) {
        AddDeleteUninstallEntryForMSIWorkItems(installer_state, *product,
                                               post_install_task_list);
      }
    }
  }

  return true;
}

void AddInstallWorkItems(const InstallationState& original_state,
                         const InstallerState& installer_state,
                         const base::FilePath& setup_path,
                         const base::FilePath& archive_path,
                         const base::FilePath& src_path,
                         const base::FilePath& temp_path,
                         const Version* current_version,
                         const Version& new_version,
                         WorkItemList* install_list) {
  DCHECK(install_list);

  const base::FilePath& target_path = installer_state.target_path();

  // A temp directory that work items need and the actual install directory.
  install_list->AddCreateDirWorkItem(temp_path);
  install_list->AddCreateDirWorkItem(target_path);

  if (installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER) ||
      installer_state.FindProduct(BrowserDistribution::CHROME_BINARIES)) {
    AddChromeWorkItems(original_state,
                       installer_state,
                       setup_path,
                       archive_path,
                       src_path,
                       temp_path,
                       current_version,
                       new_version,
                       install_list);
  }

#if defined(GOOGLE_CHROME_BUILD)
  // For Chrome, unconditionally remove the legacy app_host.exe.
  if (!InstallUtil::IsChromeSxSProcess())
    AddRemoveLegacyAppHostExeWorkItems(target_path, temp_path, install_list);
#endif  // GOOGLE_CHROME_BUILD

  // Copy installer in install directory
  AddInstallerCopyTasks(installer_state, setup_path, archive_path, temp_path,
                        new_version, install_list);

  const HKEY root = installer_state.root_key();
  // Only set "lang" for user-level installs since for system-level, the install
  // language may not be related to a given user's runtime language.
  const bool add_language_identifier = !installer_state.system_install();

  const Products& products = installer_state.products();
  for (Products::const_iterator it = products.begin(); it < products.end();
       ++it) {
    const Product& product = **it;

    AddUninstallShortcutWorkItems(installer_state, setup_path, new_version,
                                  product, install_list);

    BrowserDistribution* dist = product.distribution();
    AddVersionKeyWorkItems(root,
                           dist->GetVersionKey(),
                           dist->GetDisplayName(),
                           new_version,
                           add_language_identifier,
                           install_list);

    AddCleanupDelegateExecuteWorkItems(installer_state, product, install_list);
    AddCleanupDeprecatedPerUserRegistrationsWorkItems(product, install_list);

    AddActiveSetupWorkItems(installer_state, new_version, product,
                            install_list);
  }

  // Ensure that the Clients key for the binaries is gone for single installs
  // (but not for SxS Chrome).
  if (!installer_state.is_multi_install() &&
      !InstallUtil::IsChromeSxSProcess()) {
    BrowserDistribution* binaries_dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES);
    install_list->AddDeleteRegKeyWorkItem(root,
                                          binaries_dist->GetVersionKey(),
                                          KEY_WOW64_32KEY);
  }

#if defined(GOOGLE_CHROME_BUILD)
  if (!InstallUtil::IsChromeSxSProcess())
    AddRemoveLegacyAppCommandsWorkItems(installer_state, install_list);
#endif  // GOOGLE_CHROME_BUILD

  // Add any remaining work items that involve special settings for
  // each product.
  AddProductSpecificWorkItems(original_state,
                              installer_state,
                              setup_path,
                              new_version,
                              current_version == NULL,
                              add_language_identifier,
                              install_list);

  // Copy over brand, usagestats, and other values.
  AddGoogleUpdateWorkItems(original_state, installer_state, install_list);

  // Migrate usagestats back to Chrome.
  AddMigrateUsageStatesWorkItems(original_state, installer_state, install_list);

  // Append the tasks that run after the installation.
  AppendPostInstallTasks(installer_state,
                         setup_path,
                         current_version,
                         new_version,
                         install_list);
}

void AddRegisterComDllWorkItems(const base::FilePath& dll_folder,
                                const std::vector<base::FilePath>& dll_list,
                                bool system_level,
                                bool do_register,
                                bool ignore_failures,
                                WorkItemList* work_item_list) {
  DCHECK(work_item_list);
  if (dll_list.empty()) {
    VLOG(1) << "No COM DLLs to register";
  } else {
    std::vector<base::FilePath>::const_iterator dll_iter(dll_list.begin());
    for (; dll_iter != dll_list.end(); ++dll_iter) {
      base::FilePath dll_path = dll_folder.Append(*dll_iter);
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
  WorkItem* set_msi_work_item =
      work_item_list->AddSetRegValueWorkItem(installer_state.root_key(),
                                             dist->GetStateKey(),
                                             KEY_WOW64_32KEY,
                                             google_update::kRegMSIField,
                                             msi_value,
                                             true);
  DCHECK(set_msi_work_item);
  set_msi_work_item->set_ignore_failure(true);
  set_msi_work_item->set_log_message("Could not write MSI marker!");
}

void AddCleanupDeprecatedPerUserRegistrationsWorkItems(const Product& product,
                                                       WorkItemList* list) {
  if (product.is_chrome()) {
    BrowserDistribution* dist = product.distribution();

    // TODO(gab): Remove cleanup code for Metro after M53.
    VLOG(1) << "Adding unregistration items for per-user Metro keys.";
    list->AddDeleteRegKeyWorkItem(HKEY_CURRENT_USER,
                                  dist->GetRegistryPath() + L"\\Metro",
                                  KEY_WOW64_32KEY);
    list->AddDeleteRegKeyWorkItem(HKEY_CURRENT_USER,
                                  dist->GetRegistryPath() + L"\\Metro",
                                  KEY_WOW64_64KEY);
  }
}

void AddActiveSetupWorkItems(const InstallerState& installer_state,
                             const Version& new_version,
                             const Product& product,
                             WorkItemList* list) {
  DCHECK(installer_state.operation() != InstallerState::UNINSTALL);
  BrowserDistribution* dist = product.distribution();

  if (!product.is_chrome() || !installer_state.system_install()) {
    const char* install_level =
        installer_state.system_install() ? "system" : "user";
    VLOG(1) << "No Active Setup processing to do for " << install_level
            << "-level " << dist->GetDisplayName();
    return;
  }
  DCHECK(installer_state.RequiresActiveSetup());

  const HKEY root = HKEY_LOCAL_MACHINE;
  const base::string16 active_setup_path(InstallUtil::GetActiveSetupPath(dist));

  VLOG(1) << "Adding registration items for Active Setup.";
  list->AddCreateRegKeyWorkItem(
      root, active_setup_path, WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root,
                               active_setup_path,
                               WorkItem::kWow64Default,
                               L"",
                               dist->GetDisplayName(),
                               true);

  base::FilePath active_setup_exe(installer_state.GetInstallerDirectory(
      new_version).Append(kActiveSetupExe));
  base::CommandLine cmd(active_setup_exe);
  cmd.AppendSwitch(installer::switches::kConfigureUserSettings);
  cmd.AppendSwitch(installer::switches::kVerboseLogging);
  cmd.AppendSwitch(installer::switches::kSystemLevel);
  product.AppendProductFlags(&cmd);
  list->AddSetRegValueWorkItem(root,
                               active_setup_path,
                               WorkItem::kWow64Default,
                               L"StubPath",
                               cmd.GetCommandLineString(),
                               true);

  // TODO(grt): http://crbug.com/75152 Write a reference to a localized
  // resource.
  list->AddSetRegValueWorkItem(root,
                               active_setup_path,
                               WorkItem::kWow64Default,
                               L"Localized Name",
                               dist->GetDisplayName(),
                               true);

  list->AddSetRegValueWorkItem(root,
                               active_setup_path,
                               WorkItem::kWow64Default,
                               L"IsInstalled",
                               static_cast<DWORD>(1U),
                               true);

  list->AddWorkItem(new UpdateActiveSetupVersionWorkItem(
      active_setup_path, UpdateActiveSetupVersionWorkItem::UPDATE));
}

void AddDeleteOldIELowRightsPolicyWorkItems(
    const InstallerState& installer_state,
    WorkItemList* install_list) {
  DCHECK(install_list);

  base::string16 key_path;
  GetOldIELowRightsElevationPolicyKeyPath(&key_path);
  install_list->AddDeleteRegKeyWorkItem(
      installer_state.root_key(), key_path, WorkItem::kWow64Default);
}

void AppendUninstallCommandLineFlags(const InstallerState& installer_state,
                                     const Product& product,
                                     base::CommandLine* uninstall_cmd) {
  DCHECK(uninstall_cmd);

  uninstall_cmd->AppendSwitch(installer::switches::kUninstall);

  // Append the product-specific uninstall flags.
  product.AppendProductFlags(uninstall_cmd);
  if (installer_state.is_msi())
    uninstall_cmd->AppendSwitch(installer::switches::kMsi);
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

void AddOsUpgradeWorkItems(const InstallerState& installer_state,
                           const base::FilePath& setup_path,
                           const Version& new_version,
                           const Product& product,
                           WorkItemList* install_list) {
  const HKEY root_key = installer_state.root_key();
  base::string16 cmd_key(
      GetRegCommandKey(product.distribution(), kCmdOnOsUpgrade));

  if (installer_state.operation() == InstallerState::UNINSTALL) {
    install_list->AddDeleteRegKeyWorkItem(root_key, cmd_key, KEY_WOW64_32KEY)
        ->set_log_message("Removing OS upgrade command");
  } else {
    // Register with Google Update to have setup.exe --on-os-upgrade called on
    // OS upgrade.
    base::CommandLine cmd_line(
        installer_state.GetInstallerDirectory(new_version)
            .Append(setup_path.BaseName()));
    // Add the main option to indicate OS upgrade flow.
    cmd_line.AppendSwitch(installer::switches::kOnOsUpgrade);
    // Add product-specific options.
    product.AppendProductFlags(&cmd_line);
    if (installer_state.system_install())
      cmd_line.AppendSwitch(installer::switches::kSystemLevel);
    // Log everything for now.
    cmd_line.AppendSwitch(installer::switches::kVerboseLogging);

    AppCommand cmd(cmd_line.GetCommandLineString());
    cmd.set_is_auto_run_on_os_upgrade(true);
    cmd.AddWorkItems(installer_state.root_key(), cmd_key, install_list);
  }
}

void AddQuickEnableChromeFrameWorkItems(const InstallerState& installer_state,
                                        WorkItemList* work_item_list) {
  DCHECK(work_item_list);

  base::string16 cmd_key(
      GetRegCommandKey(BrowserDistribution::GetSpecificDistribution(
                           BrowserDistribution::CHROME_BINARIES),
                       kCmdQuickEnableCf));

  // Unconditionally remove the legacy Quick Enable command from the binaries.
  // Do this even if multi-install Chrome isn't installed to ensure that it is
  // not left behind in any case.
  work_item_list->AddDeleteRegKeyWorkItem(
                      installer_state.root_key(), cmd_key, KEY_WOW64_32KEY)
      ->set_log_message("removing " + base::UTF16ToASCII(kCmdQuickEnableCf) +
                        " command");
}

}  // namespace installer
