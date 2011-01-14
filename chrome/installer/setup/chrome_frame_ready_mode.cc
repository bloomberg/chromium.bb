// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/chrome_frame_ready_mode.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/package.h"
#include "chrome/installer/util/package_properties.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

InstallStatus ChromeFrameReadyModeOptIn(const InstallerState& installer_state,
                                        const CommandLine& cmd_line) {
  VLOG(1) << "Opting into Chrome Frame";
  InstallStatus status = INSTALL_REPAIRED;

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  bool system_install = false;
  prefs.GetBool(master_preferences::kSystemLevel, &system_install);
  BrowserDistribution* cf = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME, prefs);
  DCHECK(cf->ShouldCreateUninstallEntry())
      << "Opting into CF should create an uninstall entry";
  BrowserDistribution* chrome = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BROWSER, prefs);

  ActivePackageProperties package_properties;

  // Remove ChromeFrameReadyMode, update Chrome's uninstallation commands to
  // only uninstall Chrome, and add an entry to the Add/Remove Programs
  // dialog for GCF.

  FilePath path(GetChromeFrameInstallPath(true, system_install, cf));
  if (path.empty()) {
    LOG(ERROR) << "Conflicting installations";
    status = NON_MULTI_INSTALLATION_EXISTS;
  } else {
    InstallationState original_state;
    original_state.Initialize(prefs);

    scoped_refptr<Package> package(new Package(prefs.is_multi_install(),
        system_install, path, &package_properties));
    scoped_refptr<Product> cf_product(new Product(cf, package));
    DCHECK(cf_product->ShouldCreateUninstallEntry() || cf_product->IsMsi());
    scoped_refptr<Product> chrome_product(new Product(chrome, package));
    const ProductState* product_state =
        original_state.GetProductState(system_install, cf->GetType());
    if (product_state == NULL) {
      LOG(ERROR) << "No Chrome Frame installation found for opt-in.";
      return CHROME_NOT_INSTALLED;
    }
    scoped_ptr<WorkItemList> item_list(WorkItem::CreateWorkItemList());

    // This creates the uninstallation entry for GCF.
    AddUninstallShortcutWorkItems(cmd_line.GetProgram(),
        product_state->version(), item_list.get(), *cf_product.get());
    // This updates the Chrome uninstallation entries.
    AddUninstallShortcutWorkItems(cmd_line.GetProgram(),
        product_state->version(), item_list.get(), *chrome_product.get());

    // Add a work item to delete the ChromeFrameReadyMode registry value.
    HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    item_list->AddDeleteRegValueWorkItem(root, package_properties.GetStateKey(),
        kChromeFrameReadyModeField, REG_QWORD);

    // Delete the command elevation registry keys
    std::wstring version_key(cf->GetVersionKey());
    item_list->AddDeleteRegValueWorkItem(
        root, version_key, google_update::kRegCFTempOptOutCmdField, REG_SZ);
    item_list->AddDeleteRegValueWorkItem(
        root, version_key, google_update::kRegCFEndTempOptOutCmdField, REG_SZ);
    item_list->AddDeleteRegValueWorkItem(root, version_key,
                                         google_update::kRegCFOptOutCmdField,
                                         REG_SZ);
    item_list->AddDeleteRegValueWorkItem(root, version_key,
                                         google_update::kRegCFOptInCmdField,
                                         REG_SZ);

    if (!item_list->Do()) {
      LOG(ERROR) << "Failed to opt into GCF";
      item_list->Rollback();
      status = READY_MODE_OPT_IN_FAILED;
    }
  }

  return status;
}

const wchar_t kPostPlatformUAKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\"
    L"User Agent\\Post Platform";
const wchar_t kChromeFramePrefix[] = L"chromeframe/";

InstallStatus ChromeFrameReadyModeTempOptOut(const CommandLine& cmd_line) {
  VLOG(1) << "Temporarily opting out of Chrome Frame";
  InstallStatus status = INSTALL_REPAIRED;

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  bool system_install = false;
  prefs.GetBool(master_preferences::kSystemLevel, &system_install);
  BrowserDistribution* cf = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME, prefs);

  installer::ActivePackageProperties package_properties;

  // Remove the ChromeFrame user agent string from the registry, modify the
  // ReadyMode state flag.
  FilePath path(GetChromeFrameInstallPath(true, system_install, cf));
  if (path.empty()) {
    LOG(ERROR) << "Conflicting installations";
    status = NON_MULTI_INSTALLATION_EXISTS;
  } else {
    scoped_ptr<WorkItemList> item_list(WorkItem::CreateWorkItemList());

    HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    // Add a work item to delete the ChromeFrame user agent registry value.
    base::win::RegistryValueIterator values(root, kPostPlatformUAKey);
    while (values.Valid()) {
      const wchar_t* name = values.Name();
      if (StartsWith(name, kChromeFramePrefix, true)) {
        item_list->AddDeleteRegValueWorkItem(root, kPostPlatformUAKey, name,
                                             REG_SZ);
      }
      ++values;
    }

    // Add a work item to update the Ready Mode state flag
    int64 timestamp = base::Time::Now().ToInternalValue();
    item_list->AddSetRegValueWorkItem(root, package_properties.GetStateKey(),
                                      kChromeFrameReadyModeField, timestamp,
                                      true);

    if (!item_list->Do()) {
      LOG(ERROR) << "Failed to temporarily opt out of GCF";
      item_list->Rollback();
      status = READY_MODE_TEMP_OPT_OUT_FAILED;
    }
  }

  return status;
}

InstallStatus ChromeFrameReadyModeEndTempOptOut(const CommandLine& cmd_line) {
  VLOG(1) << "Ending temporary opt-out of Chrome Frame";
  InstallStatus status = INSTALL_REPAIRED;

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  bool system_install = false;
  prefs.GetBool(master_preferences::kSystemLevel, &system_install);
  BrowserDistribution* cf = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME, prefs);

  installer::ActivePackageProperties package_properties;

  // Replace the ChromeFrame user agent string in the registry, modify the
  // ReadyMode state flag.
  FilePath path(GetChromeFrameInstallPath(true, system_install, cf));
  scoped_ptr<Version> installed_version(
      InstallUtil::GetChromeVersion(cf, system_install));

  if (path.empty()) {
    LOG(ERROR) << "Conflicting installations";
    status = NON_MULTI_INSTALLATION_EXISTS;
  } else if (installed_version == NULL) {
    LOG(ERROR) << "Failed to query installed version of Chrome Frame";
    status = READY_MODE_END_TEMP_OPT_OUT_FAILED;
  } else {
    scoped_ptr<WorkItemList> item_list(WorkItem::CreateWorkItemList());

    HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    std::wstring chrome_frame_ua_value_name = kChromeFramePrefix;
    chrome_frame_ua_value_name += ASCIIToWide(installed_version->GetString());

    // Store the Chrome Frame user agent string
    item_list->AddSetRegValueWorkItem(root, kPostPlatformUAKey,
                                      chrome_frame_ua_value_name, L"", true);
    // Add a work item to update the Ready Mode state flag
    item_list->AddSetRegValueWorkItem(root, package_properties.GetStateKey(),
                                      kChromeFrameReadyModeField,
                                      static_cast<int64>(1), true);

    if (!item_list->Do()) {
      LOG(ERROR) << "Failed to end temporary opt out of GCF";
      item_list->Rollback();
      status = READY_MODE_END_TEMP_OPT_OUT_FAILED;
    }
  }

  return status;
}

}  // namespace installer
