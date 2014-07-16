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

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
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

using base::ASCIIToWide;
using base::win::RegKey;

namespace installer {

namespace {

// The version identifying the work done by setup.exe --configure-user-settings
// on user login by way of Active Setup.  Increase this value if the work done
// in setup_main.cc's handling of kConfigureUserSettings changes and should be
// executed again for all users.
const wchar_t kActiveSetupVersion[] = L"24,0,0,0";

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

  // If only the App Host (not even the Chrome Binaries) is being installed,
  // this must be a user-level App Host piggybacking on system-level Chrome
  // Binaries. Only setup.exe is required, and only for uninstall.
  if (installer_state.products().size() != 1 ||
      !installer_state.FindProduct(BrowserDistribution::CHROME_APP_HOST)) {
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
        // directory. For example, when quick-enabling user-level App Launcher
        // from system-level Binaries. We can't (and don't want to) remove the
        // system-level archive.
        install_list->AddCopyTreeWorkItem(archive_path.value(),
                                          archive_dst.value(),
                                          temp_path.value(),
                                          WorkItem::ALWAYS);
      }
    }
  }
}

base::string16 GetRegCommandKey(BrowserDistribution* dist,
                                const wchar_t* name) {
  base::string16 cmd_key(dist->GetVersionKey());
  cmd_key.append(1, base::FilePath::kSeparators[0])
      .append(google_update::kRegCommandsKey)
      .append(1, base::FilePath::kSeparators[0])
      .append(name);
  return cmd_key;
}

// Adds work items to create (or delete if uninstalling) app commands to launch
// the app with a switch. The following criteria should be true:
//  1. The switch takes one parameter.
//  2. The command send pings.
//  3. The command is web accessible.
//  4. The command is run as the user.
void AddCommandWithParameterWorkItems(const InstallerState& installer_state,
                                      const InstallationState& machine_state,
                                      const Version& new_version,
                                      const Product& product,
                                      const wchar_t* command_key,
                                      const wchar_t* app,
                                      const char* command_with_parameter,
                                      WorkItemList* work_item_list) {
  DCHECK(command_key);
  DCHECK(app);
  DCHECK(command_with_parameter);
  DCHECK(work_item_list);

  base::string16 full_cmd_key(
      GetRegCommandKey(product.distribution(), command_key));

  if (installer_state.operation() == InstallerState::UNINSTALL) {
    work_item_list->AddDeleteRegKeyWorkItem(installer_state.root_key(),
                                            full_cmd_key,
                                            KEY_WOW64_32KEY)
        ->set_log_message("removing " + base::UTF16ToASCII(command_key) +
                          " command");
  } else {
    CommandLine cmd_line(installer_state.target_path().Append(app));
    cmd_line.AppendSwitchASCII(command_with_parameter, "%1");

    AppCommand cmd(cmd_line.GetCommandLineString());
    cmd.set_sends_pings(true);
    cmd.set_is_web_accessible(true);
    cmd.set_is_run_as_user(true);
    cmd.AddWorkItems(installer_state.root_key(), full_cmd_key, work_item_list);
  }
}

void AddInstallAppCommandWorkItems(const InstallerState& installer_state,
                                   const InstallationState& machine_state,
                                   const Version& new_version,
                                   const Product& product,
                                   WorkItemList* work_item_list) {
  DCHECK(product.is_chrome_app_host());
  AddCommandWithParameterWorkItems(installer_state, machine_state, new_version,
                                   product, kCmdInstallApp,
                                   installer::kChromeAppHostExe,
                                   ::switches::kInstallFromWebstore,
                                   work_item_list);
}

void AddInstallExtensionCommandWorkItem(const InstallerState& installer_state,
                                        const InstallationState& machine_state,
                                        const base::FilePath& setup_path,
                                        const Version& new_version,
                                        const Product& product,
                                        WorkItemList* work_item_list) {
  DCHECK(product.is_chrome());
  AddCommandWithParameterWorkItems(installer_state, machine_state, new_version,
                                   product, kCmdInstallExtension,
                                   installer::kChromeExe,
                                   ::switches::kLimitedInstallFromWebstore,
                                   work_item_list);
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

// Returns the basic CommandLine to setup.exe for a quick-enable operation on
// the binaries. This will unconditionally include --multi-install as well as
// --verbose-logging if the current installation was launched with
// --verbose-logging.  |setup_path| and |new_version| are optional only when
// the operation is an uninstall.
CommandLine GetGenericQuickEnableCommand(
    const InstallerState& installer_state,
    const InstallationState& machine_state,
    const base::FilePath& setup_path,
    const Version& new_version) {
  // Only valid for multi-install operations.
  DCHECK(installer_state.is_multi_install());
  // Only valid when Chrome Binaries aren't being uninstalled.
  DCHECK(installer_state.operation() != InstallerState::UNINSTALL ||
         !installer_state.FindProduct(BrowserDistribution::CHROME_BINARIES));
  // setup_path and new_version are required when not uninstalling.
  DCHECK(installer_state.operation() == InstallerState::UNINSTALL ||
         (!setup_path.empty() && new_version.IsValid()));

  // The path to setup.exe contains the version of the Chrome Binaries, so it
  // takes a little work to get it right.
  base::FilePath binaries_setup_path;
  if (installer_state.operation() == InstallerState::UNINSTALL) {
    // One or more products are being uninstalled, but not Chrome Binaries.
    // Use the path to the currently installed Chrome Binaries' setup.exe.
    const ProductState* product_state = machine_state.GetProductState(
        installer_state.system_install(),
        BrowserDistribution::CHROME_BINARIES);
    DCHECK(product_state);
    binaries_setup_path = product_state->uninstall_command().GetProgram();
  } else {
    // Chrome Binaries are being installed, updated, or otherwise operated on.
    // Use the path to the given |setup_path| in the normal location of
    // multi-install Chrome Binaries of the given |version|.
    binaries_setup_path = installer_state.GetInstallerDirectory(new_version)
                              .Append(setup_path.BaseName());
  }
  DCHECK(!binaries_setup_path.empty());

  CommandLine cmd_line(binaries_setup_path);
  cmd_line.AppendSwitch(switches::kMultiInstall);
  if (installer_state.verbose_logging())
    cmd_line.AppendSwitch(switches::kVerboseLogging);
  return cmd_line;
}

// Adds work items to add the "quick-enable-application-host" command to the
// multi-installer binaries' version key on the basis of the current operation
// (represented in |installer_state|) and the pre-existing machine configuration
// (represented in |machine_state|).
void AddQuickEnableApplicationLauncherWorkItems(
    const InstallerState& installer_state,
    const InstallationState& machine_state,
    const base::FilePath& setup_path,
    const Version& new_version,
    WorkItemList* work_item_list) {
  DCHECK(work_item_list);

  bool will_have_chrome_binaries =
      WillProductBePresentAfterSetup(installer_state, machine_state,
                                     BrowserDistribution::CHROME_BINARIES);

  // For system-level binaries there is no way to keep the command state in sync
  // with the installation/uninstallation of the Application Launcher (which is
  // always at user-level).  So we do not try to remove the command, i.e., it
  // will always be installed if the Chrome Binaries are installed.
  if (will_have_chrome_binaries) {
    base::string16 cmd_key(
        GetRegCommandKey(BrowserDistribution::GetSpecificDistribution(
                             BrowserDistribution::CHROME_BINARIES),
                         kCmdQuickEnableApplicationHost));
    CommandLine cmd_line(GetGenericQuickEnableCommand(installer_state,
                                                      machine_state,
                                                      setup_path,
                                                      new_version));
    // kMultiInstall and kVerboseLogging were processed above.
    cmd_line.AppendSwitch(switches::kChromeAppLauncher);
    cmd_line.AppendSwitch(switches::kEnsureGoogleUpdatePresent);
    AppCommand cmd(cmd_line.GetCommandLineString());
    cmd.set_sends_pings(true);
    cmd.set_is_web_accessible(true);
    cmd.set_is_run_as_user(true);
    cmd.AddWorkItems(installer_state.root_key(), cmd_key, work_item_list);
  }
}

void AddProductSpecificWorkItems(const InstallationState& original_state,
                                 const InstallerState& installer_state,
                                 const base::FilePath& setup_path,
                                 const Version& new_version,
                                 bool is_new_install,
                                 WorkItemList* list) {
  const Products& products = installer_state.products();
  for (Products::const_iterator it = products.begin(); it < products.end();
       ++it) {
    const Product& p = **it;
    if (p.is_chrome_app_host()) {
      AddInstallAppCommandWorkItems(installer_state, original_state,
                                    new_version, p, list);
    }
    if (p.is_chrome()) {
      AddOsUpgradeWorkItems(installer_state, setup_path, new_version, p,
                            list);
      AddInstallExtensionCommandWorkItem(installer_state, original_state,
                                         setup_path, new_version, p, list);
      AddFirewallRulesWorkItems(
          installer_state, p.distribution(), is_new_install, list);
    }
    if (p.is_chrome_binaries()) {
      AddQueryEULAAcceptanceWorkItems(
          installer_state, setup_path, new_version, p, list);
      AddQuickEnableChromeFrameWorkItems(installer_state, list);
      AddQuickEnableApplicationLauncherWorkItems(
          installer_state, original_state, setup_path, new_version, list);
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
    const base::FilePath& temp_path,
    WorkItemList* work_item_list) {
  DCHECK(installer_state.is_msi())
      << "This must only be called for MSI installations!";

  // First attempt to delete the old installation's ARP dialog entry.
  HKEY reg_root = installer_state.root_key();
  base::string16 uninstall_reg(product.distribution()->GetUninstallRegPath());

  WorkItem* delete_reg_key = work_item_list->AddDeleteRegKeyWorkItem(
      reg_root, uninstall_reg, KEY_WOW64_32KEY);
  delete_reg_key->set_ignore_failure(true);

  // Then attempt to delete the old installation's start menu shortcut.
  base::FilePath uninstall_link;
  if (installer_state.system_install()) {
    PathService::Get(base::DIR_COMMON_START_MENU, &uninstall_link);
  } else {
    PathService::Get(base::DIR_START_MENU, &uninstall_link);
  }

  if (uninstall_link.empty()) {
    LOG(ERROR) << "Failed to get location for shortcut.";
  } else {
    uninstall_link = uninstall_link.Append(
        product.distribution()->GetStartMenuShortcutSubfolder(
            BrowserDistribution::SUBFOLDER_CHROME));
    uninstall_link = uninstall_link.Append(
        product.distribution()->GetUninstallLinkName() + installer::kLnkExt);
    VLOG(1) << "Deleting old uninstall shortcut (if present): "
            << uninstall_link.value();
    WorkItem* delete_link = work_item_list->AddDeleteTreeWorkItem(
        uninstall_link, temp_path);
    delete_link->set_ignore_failure(true);
    delete_link->set_log_message(
        "Failed to delete old uninstall shortcut.");
  }
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

// Probes COM machinery to get an instance of delegate_execute.exe's
// CommandExecuteImpl class.  This is required so that COM purges its cache of
// the path to the binary, which changes on updates.  This callback
// unconditionally returns true since an install should not be aborted if the
// probe fails.
bool ProbeCommandExecuteCallback(const base::string16& command_execute_id,
                                 const CallbackWorkItem& work_item) {
  // Noop on rollback.
  if (work_item.IsRollback())
    return true;

  CLSID class_id = {};

  HRESULT hr = CLSIDFromString(command_execute_id.c_str(), &class_id);
  if (FAILED(hr)) {
    LOG(DFATAL) << "Failed converting \"" << command_execute_id << "\" to "
                   "CLSID; hr=0x" << std::hex << hr;
  } else {
    base::win::ScopedComPtr<IUnknown> command_execute_impl;
    hr = command_execute_impl.CreateInstance(class_id, NULL,
                                             CLSCTX_LOCAL_SERVER);
    if (hr != REGDB_E_CLASSNOTREG) {
      LOG(ERROR) << "Unexpected result creating CommandExecuteImpl; hr=0x"
                 << std::hex << hr;
    }
  }

  return true;
}

void AddUninstallDelegateExecuteWorkItems(
    HKEY root,
    const base::string16& delegate_execute_path,
    WorkItemList* list) {
  VLOG(1) << "Adding unregistration items for DelegateExecute verb handler in "
          << root;
  // Delete both 64 and 32 keys to handle 32->64 or 64->32 migration.
  list->AddDeleteRegKeyWorkItem(root, delegate_execute_path, KEY_WOW64_32KEY);

  list->AddDeleteRegKeyWorkItem(root, delegate_execute_path, KEY_WOW64_64KEY);

  // In the past, the ICommandExecuteImpl interface and a TypeLib were both
  // registered.  Remove these since this operation may be updating a machine
  // that had the old registrations.
  list->AddDeleteRegKeyWorkItem(root,
                                L"Software\\Classes\\Interface\\"
                                L"{0BA0D4E9-2259-4963-B9AE-A839F7CB7544}",
                                KEY_WOW64_32KEY);
  list->AddDeleteRegKeyWorkItem(root,
                                L"Software\\Classes\\TypeLib\\"
#if defined(GOOGLE_CHROME_BUILD)
                                L"{4E805ED8-EBA0-4601-9681-12815A56EBFD}",
#else
                                L"{7779FB70-B399-454A-AA1A-BAA850032B10}",
#endif
                                KEY_WOW64_32KEY);
}

// Google Chrome Canary, between 20.0.1101.0 (crrev.com/132190) and 20.0.1106.0
// (exclusively -- crrev.com/132596), registered a DelegateExecute class by
// mistake (with the same GUID as Chrome). The fix stopped registering the bad
// value, but didn't delete it. This is a problem for users who had installed
// Canary before 20.0.1106.0 and now have a system-level Chrome, as the
// left-behind Canary registrations in HKCU mask the HKLM registrations for the
// same GUID. Cleanup those registrations if they still exist and belong to this
// Canary (i.e., the registered delegate_execute's path is under |target_path|).
void CleanupBadCanaryDelegateExecuteRegistration(
    const base::FilePath& target_path,
    WorkItemList* list) {
  base::string16 google_chrome_delegate_execute_path(
      L"Software\\Classes\\CLSID\\{5C65F4B0-3651-4514-B207-D10CB699B14B}");
  base::string16 google_chrome_local_server_32(
      google_chrome_delegate_execute_path + L"\\LocalServer32");

  RegKey local_server_32_key;
  base::string16 registered_server;
  if (local_server_32_key.Open(HKEY_CURRENT_USER,
                               google_chrome_local_server_32.c_str(),
                               KEY_QUERY_VALUE) == ERROR_SUCCESS &&
      local_server_32_key.ReadValue(L"ServerExecutable",
                                    &registered_server) == ERROR_SUCCESS &&
      target_path.IsParent(base::FilePath(registered_server))) {
    scoped_ptr<WorkItemList> no_rollback_list(
        WorkItem::CreateNoRollbackWorkItemList());
    AddUninstallDelegateExecuteWorkItems(
        HKEY_CURRENT_USER, google_chrome_delegate_execute_path,
        no_rollback_list.get());
    list->AddWorkItem(no_rollback_list.release());
    VLOG(1) << "Added deletion items for bad Canary registrations.";
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

  CommandLine uninstall_arguments(CommandLine::NO_PROGRAM);
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
    CommandLine quoted_uninstall_cmd(installer_path);
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
        install_path.Append(dist->GetIconFilename()).value(),
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
                                         ASCIIToWide(new_version.GetString()),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"DisplayVersion",
                                         ASCIIToWide(new_version.GetString()),
                                         true);
    // TODO(wfh): Ensure that this value is preserved in the 64-bit hive when
    // 64-bit installs place the uninstall information into the 64-bit registry.
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"InstallDate",
                                         InstallUtil::GetCurrentDate(),
                                         false);

    const std::vector<uint16>& version_components = new_version.components();
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
                            BrowserDistribution* dist,
                            const Version& new_version,
                            bool add_language_identifier,
                            WorkItemList* list) {
  // Create Version key for each distribution (if not already present) and set
  // the new product version as the last step.
  base::string16 version_key(dist->GetVersionKey());
  list->AddCreateRegKeyWorkItem(root, version_key, KEY_WOW64_32KEY);

  base::string16 product_name(dist->GetDisplayName());
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
    if (LowerCaseEqualsASCII(language, "en-us"))
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
                               ASCIIToWide(new_version.GetString()),
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

bool AppendPostInstallTasks(const InstallerState& installer_state,
                            const base::FilePath& setup_path,
                            const Version* current_version,
                            const Version& new_version,
                            const base::FilePath& temp_path,
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

    CommandLine rename(installer_path);
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
            ASCIIToWide(current_version->GetString()),
            true);
      }
      if (critical_version.IsValid()) {
        in_use_update_work_items->AddSetRegValueWorkItem(
            root,
            version_key,
            KEY_WOW64_32KEY,
            google_update::kRegCriticalVersionField,
            ASCIIToWide(critical_version.GetString()),
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
        CommandLine product_rename_cmd(rename);
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

      // We want MSI installs to take over the Add/Remove Programs shortcut.
      // Make a best-effort attempt to delete any shortcuts left over from
      // previous non-MSI installations for the same type of install (system or
      // per user).
      if (product->ShouldCreateUninstallEntry()) {
        AddDeleteUninstallShortcutsForMSIWorkItems(installer_state, *product,
                                                   temp_path,
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

  if (installer_state.FindProduct(BrowserDistribution::CHROME_APP_HOST)) {
    install_list->AddCopyTreeWorkItem(
        src_path.Append(installer::kChromeAppHostExe).value(),
        target_path.Append(installer::kChromeAppHostExe).value(),
        temp_path.value(),
        WorkItem::ALWAYS,
        L"");
  }

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

    AddVersionKeyWorkItems(root, product.distribution(), new_version,
                           add_language_identifier, install_list);

    AddDelegateExecuteWorkItems(installer_state, target_path, new_version,
                                product, install_list);

    AddActiveSetupWorkItems(installer_state, setup_path, new_version, product,
                            install_list);
  }

  // TODO(huangs): Implement actual migration code and remove the hack below.
  // If installing Chrome without the legacy stand-alone App Launcher (to be
  // handled later), add "shadow" App Launcher registry keys so Google Update
  // would recognize the "dr" value in the App Launcher ClientState key.
  // Checking .is_multi_install() excludes Chrome Canary and stand-alone Chrome.
  if (installer_state.is_multi_install() &&
      installer_state.FindProduct(BrowserDistribution::CHROME_BROWSER) &&
      !installer_state.FindProduct(BrowserDistribution::CHROME_APP_HOST)) {
    BrowserDistribution* shadow_app_launcher_dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_APP_HOST);
    AddVersionKeyWorkItems(root, shadow_app_launcher_dist, new_version,
                           add_language_identifier, install_list);
  }

  // Add any remaining work items that involve special settings for
  // each product.
  AddProductSpecificWorkItems(original_state,
                              installer_state,
                              setup_path,
                              new_version,
                              current_version == NULL,
                              install_list);

  // Copy over brand, usagestats, and other values.
  AddGoogleUpdateWorkItems(original_state, installer_state, install_list);

  // Append the tasks that run after the installation.
  AppendPostInstallTasks(installer_state,
                         setup_path,
                         current_version,
                         new_version,
                         temp_path,
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

void AddDelegateExecuteWorkItems(const InstallerState& installer_state,
                                 const base::FilePath& target_path,
                                 const Version& new_version,
                                 const Product& product,
                                 WorkItemList* list) {
  base::string16 handler_class_uuid;
  BrowserDistribution* dist = product.distribution();
  if (!dist->GetCommandExecuteImplClsid(&handler_class_uuid)) {
    if (InstallUtil::IsChromeSxSProcess()) {
      CleanupBadCanaryDelegateExecuteRegistration(target_path, list);
    } else {
      VLOG(1) << "No DelegateExecute verb handler processing to do for "
              << dist->GetDisplayName();
    }
    return;
  }

  HKEY root = installer_state.root_key();
  base::string16 delegate_execute_path(L"Software\\Classes\\CLSID\\");
  delegate_execute_path.append(handler_class_uuid);

  // Unconditionally remove registration regardless of whether or not it is
  // needed since builds after r132190 included it when it wasn't strictly
  // necessary.  Do this removal before adding in the new key to ensure that
  // the COM probe/flush below does its job.
  AddUninstallDelegateExecuteWorkItems(root, delegate_execute_path, list);

  // Add work items to register the handler iff it is present.
  // See also shell_util.cc's GetProgIdEntries.
  if (installer_state.operation() != InstallerState::UNINSTALL) {
    VLOG(1) << "Adding registration items for DelegateExecute verb handler.";

    // Force COM to flush its cache containing the path to the old handler.
    list->AddCallbackWorkItem(base::Bind(&ProbeCommandExecuteCallback,
                                         handler_class_uuid));

    // The path to the exe (in the version directory).
    base::FilePath delegate_execute(target_path);
    if (new_version.IsValid())
      delegate_execute = delegate_execute.AppendASCII(new_version.GetString());
    delegate_execute = delegate_execute.Append(kDelegateExecuteExe);

    // Command-line featuring the quoted path to the exe.
    base::string16 command(1, L'"');
    command.append(delegate_execute.value()).append(1, L'"');

    // Register the CommandExecuteImpl class in Software\Classes\CLSID\...
    list->AddCreateRegKeyWorkItem(
        root, delegate_execute_path, WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root,
                                 delegate_execute_path,
                                 WorkItem::kWow64Default,
                                 L"",
                                 L"CommandExecuteImpl Class",
                                 true);
    base::string16 subkey(delegate_execute_path);
    subkey.append(L"\\LocalServer32");
    list->AddCreateRegKeyWorkItem(root, subkey, WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(
        root, subkey, WorkItem::kWow64Default, L"", command, true);
    list->AddSetRegValueWorkItem(root,
                                 subkey,
                                 WorkItem::kWow64Default,
                                 L"ServerExecutable",
                                 delegate_execute.value(),
                                 true);

    subkey.assign(delegate_execute_path).append(L"\\Programmable");
    list->AddCreateRegKeyWorkItem(root, subkey, WorkItem::kWow64Default);
  }
}

void AddActiveSetupWorkItems(const InstallerState& installer_state,
                             const base::FilePath& setup_path,
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
  CommandLine cmd(active_setup_exe);
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

  list->AddSetRegValueWorkItem(root,
                               active_setup_path,
                               WorkItem::kWow64Default,
                               L"Version",
                               kActiveSetupVersion,
                               true);
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
                                     CommandLine* uninstall_cmd) {
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
    CommandLine cmd_line(installer_state
        .GetInstallerDirectory(new_version)
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

void AddQueryEULAAcceptanceWorkItems(const InstallerState& installer_state,
                                     const base::FilePath& setup_path,
                                     const Version& new_version,
                                     const Product& product,
                                     WorkItemList* work_item_list) {
  const HKEY root_key = installer_state.root_key();
  base::string16 cmd_key(
      GetRegCommandKey(product.distribution(), kCmdQueryEULAAcceptance));
  if (installer_state.operation() == InstallerState::UNINSTALL) {
    work_item_list->AddDeleteRegKeyWorkItem(root_key, cmd_key, KEY_WOW64_32KEY)
        ->set_log_message("Removing query EULA acceptance command");
  } else {
    CommandLine cmd_line(installer_state
        .GetInstallerDirectory(new_version)
        .Append(setup_path.BaseName()));
    cmd_line.AppendSwitch(switches::kQueryEULAAcceptance);
    if (installer_state.system_install())
      cmd_line.AppendSwitch(installer::switches::kSystemLevel);
    if (installer_state.verbose_logging())
      cmd_line.AppendSwitch(installer::switches::kVerboseLogging);
    AppCommand cmd(cmd_line.GetCommandLineString());
    cmd.set_is_web_accessible(true);
    cmd.set_is_run_as_user(true);
    cmd.AddWorkItems(installer_state.root_key(), cmd_key, work_item_list);
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
