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
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

// If Chrome is not multi-installed at the appropriate level, error.
// If Chrome Frame is already multi-installed at the appropriate level, noop.
// If Chrome Frame is single-installed at the appropriate level, error.
// Add uninstall for Chrome Frame.
// Update uninstall for Chrome.
// Update ChannelInfo for all multi-installed products.
// Remove ready-mode.
InstallStatus ChromeFrameReadyModeOptIn(
    const InstallationState& machine_state,
    const InstallerState& installer_state) {
  VLOG(1) << "Opting into Chrome Frame";
  InstallStatus status = INSTALL_REPAIRED;

  // Make sure Chrome and Chrome Frame are both multi-installed.
  const ProductState* chrome_state =
      machine_state.GetProductState(installer_state.system_install(),
                                    BrowserDistribution::CHROME_BROWSER);
  const ProductState* cf_state =
      machine_state.GetProductState(installer_state.system_install(),
                                    BrowserDistribution::CHROME_FRAME);
  if (chrome_state == NULL) {
    LOG(ERROR) << "Chrome Frame opt-in requires multi-install of Chrome.";
    return CHROME_NOT_INSTALLED;
  }
  if (!chrome_state->is_multi_install()) {
    LOG(ERROR) << "Chrome Frame opt-in requires multi-install of Chrome.";
    return NON_MULTI_INSTALLATION_EXISTS;
  }
  if (cf_state == NULL) {
    LOG(ERROR) << "Chrome Frame opt-in requires multi-install of Chrome Frame.";
    return CHROME_NOT_INSTALLED;
  }
  if (!cf_state->is_multi_install()) {
    LOG(ERROR) << "Chrome Frame opt-in requires multi-install of Chrome Frame.";
    return NON_MULTI_INSTALLATION_EXISTS;
  }

  // Create a new InstallerState to be used for this operation.
  InstallerState opt_in_state(installer_state.level());

  // Add the two products we're going to operate on.
  const Product* chrome =
      opt_in_state.AddProductFromState(BrowserDistribution::CHROME_BROWSER,
                                       *chrome_state);
  Product* cf =
      opt_in_state.AddProductFromState(BrowserDistribution::CHROME_FRAME,
                                       *cf_state);
  // DCHECKs will fire in this case if it ever happens (it won't).
  if (chrome == NULL || cf == NULL)
    return READY_MODE_OPT_IN_FAILED;

  // Turn off ready-mode on Chrome Frame, thereby making it fully installed.
  if (!cf->SetOption(kOptionReadyMode, false)) {
    LOG(WARNING)
        << "Chrome Frame is already fully installed; opting-in nonetheless.";
  }

  // Update Chrome's uninstallation commands to only uninstall Chrome, and add
  // an entry to the Add/Remove Programs dialog for GCF.
  DCHECK(cf->ShouldCreateUninstallEntry() || opt_in_state.is_msi());

  scoped_ptr<WorkItemList> item_list(WorkItem::CreateWorkItemList());

  // This creates the uninstallation entry for GCF.
  AddUninstallShortcutWorkItems(opt_in_state, cf_state->GetSetupPath(),
      cf_state->version(), item_list.get(), *cf);
  // This updates the Chrome uninstallation entries.
  AddUninstallShortcutWorkItems(opt_in_state, chrome_state->GetSetupPath(),
      chrome_state->version(), item_list.get(), *chrome);

  // Add a work item to delete the ChromeFrameReadyMode registry value.
  HKEY root = opt_in_state.root_key();
  item_list->AddDeleteRegValueWorkItem(root,
      opt_in_state.multi_package_binaries_distribution()->GetStateKey(),
      kChromeFrameReadyModeField);

  // Update the Google Update channel ("ap") value.
  AddGoogleUpdateWorkItems(opt_in_state, item_list.get());

  // Delete the command elevation registry keys
  std::wstring version_key(cf->distribution()->GetVersionKey());
  item_list->AddDeleteRegValueWorkItem(
      root, version_key, google_update::kRegCFTempOptOutCmdField);
  item_list->AddDeleteRegValueWorkItem(
      root, version_key, google_update::kRegCFEndTempOptOutCmdField);
  item_list->AddDeleteRegValueWorkItem(root, version_key,
                                       google_update::kRegCFOptOutCmdField);
  item_list->AddDeleteRegValueWorkItem(root, version_key,
                                       google_update::kRegCFOptInCmdField);

  if (!item_list->Do()) {
    LOG(ERROR) << "Failed to opt into GCF";
    item_list->Rollback();
    status = READY_MODE_OPT_IN_FAILED;
  }

  return status;
}

const wchar_t kPostPlatformUAKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\"
    L"User Agent\\Post Platform";
const wchar_t kChromeFramePrefix[] = L"chromeframe/";

InstallStatus ChromeFrameReadyModeTempOptOut(
    const InstallationState& machine_state,
    const InstallerState& installer_state) {
  VLOG(1) << "Temporarily opting out of Chrome Frame";
  InstallStatus status = INSTALL_REPAIRED;

  // Make sure Chrome Frame is multi-installed.
  const ProductState* cf_state =
      machine_state.GetProductState(installer_state.system_install(),
                                    BrowserDistribution::CHROME_FRAME);
  if (cf_state == NULL) {
    LOG(ERROR)
        << "Chrome Frame temp opt-out requires multi-install of Chrome Frame.";
    return CHROME_NOT_INSTALLED;
  }
  if (!cf_state->is_multi_install()) {
    LOG(ERROR)
        << "Chrome Frame temp opt-out requires multi-install of Chrome Frame.";
    return NON_MULTI_INSTALLATION_EXISTS;
  }

  scoped_ptr<WorkItemList> item_list(WorkItem::CreateWorkItemList());

  HKEY root = installer_state.root_key();

  // Add a work item to delete the ChromeFrame user agent registry value.
  base::win::RegistryValueIterator values(root, kPostPlatformUAKey);
  while (values.Valid()) {
    const wchar_t* name = values.Name();
    if (StartsWith(name, kChromeFramePrefix, true)) {
      item_list->AddDeleteRegValueWorkItem(root, kPostPlatformUAKey, name);
    }
    ++values;
  }

  // Add a work item to update the Ready Mode state flag
  int64 timestamp = base::Time::Now().ToInternalValue();
  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  item_list->AddSetRegValueWorkItem(root, dist->GetStateKey(),
                                    kChromeFrameReadyModeField, timestamp,
                                    true);

  if (!item_list->Do()) {
    LOG(ERROR) << "Failed to temporarily opt out of GCF";
    item_list->Rollback();
    status = READY_MODE_TEMP_OPT_OUT_FAILED;
  }

  return status;
}

InstallStatus ChromeFrameReadyModeEndTempOptOut(
    const InstallationState& machine_state,
    const InstallerState& installer_state) {
  VLOG(1) << "Ending temporary opt-out of Chrome Frame";
  InstallStatus status = INSTALL_REPAIRED;

  // Make sure Chrome Frame is multi-installed.
  const ProductState* cf_state =
      machine_state.GetProductState(installer_state.system_install(),
                                    BrowserDistribution::CHROME_FRAME);
  if (cf_state == NULL) {
    LOG(ERROR)
        << "Chrome Frame temp opt-out requires multi-install of Chrome Frame.";
    return CHROME_NOT_INSTALLED;
  }
  if (!cf_state->is_multi_install()) {
    LOG(ERROR)
        << "Chrome Frame temp opt-out requires multi-install of Chrome Frame.";
    return NON_MULTI_INSTALLATION_EXISTS;
  }

  // Replace the ChromeFrame user agent string in the registry, modify the
  // ReadyMode state flag.
  const Version& installed_version = cf_state->version();

  scoped_ptr<WorkItemList> item_list(WorkItem::CreateWorkItemList());

  HKEY root = installer_state.root_key();

  std::wstring chrome_frame_ua_value_name(kChromeFramePrefix);
  chrome_frame_ua_value_name += ASCIIToWide(installed_version.GetString());

  // Store the Chrome Frame user agent string
  item_list->AddSetRegValueWorkItem(root, kPostPlatformUAKey,
                                    chrome_frame_ua_value_name, L"", true);
  // Add a work item to update the Ready Mode state flag
  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  item_list->AddSetRegValueWorkItem(root, dist->GetStateKey(),
                                    kChromeFrameReadyModeField,
                                    static_cast<int64>(1), true);

  if (!item_list->Do()) {
    LOG(ERROR) << "Failed to end temporary opt out of GCF";
    item_list->Rollback();
    status = READY_MODE_END_TEMP_OPT_OUT_FAILED;
  }

  return status;
}

}  // namespace installer
