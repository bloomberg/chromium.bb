// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/chrome_frame_quick_enable.h"

#include <windows.h>

#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

#include "installer_util_strings.h"  // NOLINT

using base::win::RegKey;

namespace installer {

namespace {

InstallStatus CheckQuickEnablePreconditions(
    const InstallationState& machine_state,
    const InstallerState& installer_state) {
  // Make sure Chrome is multi-installed.
  const ProductState* chrome_state =
      machine_state.GetProductState(installer_state.system_install(),
                                    BrowserDistribution::CHROME_BROWSER);
  if (chrome_state == NULL) {
    LOG(ERROR) << "Chrome Frame quick enable requires Chrome to be installed.";
    return CHROME_NOT_INSTALLED;
  } else if (!chrome_state->is_multi_install()) {
    LOG(ERROR) << "Chrome Frame quick enable requires multi-install of Chrome.";
    return NON_MULTI_INSTALLATION_EXISTS;
  }

  // Chrome Frame must not be installed (ready-mode doesn't count).
  const ProductState* cf_state =
      machine_state.GetProductState(installer_state.system_install(),
                                    BrowserDistribution::CHROME_FRAME);
  // Make sure we check both user and system installations.
  if (!cf_state) {
    cf_state = machine_state.GetProductState(!installer_state.system_install(),
                                             BrowserDistribution::CHROME_FRAME);
  }

  if (cf_state != NULL &&
      !cf_state->uninstall_command().HasSwitch(
          switches::kChromeFrameReadyMode)) {
    LOG(ERROR) << "Chrome Frame already installed.";
    return installer_state.system_install() ?
        SYSTEM_LEVEL_INSTALL_EXISTS : USER_LEVEL_INSTALL_EXISTS;
  }

  return FIRST_INSTALL_SUCCESS;
}

}  // end namespace

InstallStatus ChromeFrameQuickEnable(const InstallationState& machine_state,
                                     InstallerState* installer_state) {
  DCHECK(installer_state);
  VLOG(1) << "Chrome Frame Quick Enable";
  InstallStatus status = CheckQuickEnablePreconditions(machine_state,
                                                       *installer_state);

  if (status == FIRST_INSTALL_SUCCESS) {
    scoped_ptr<Product> multi_cf(new Product(
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_FRAME)));
    multi_cf->SetOption(installer::kOptionMultiInstall, true);
    Product* cf = installer_state->AddProduct(&multi_cf);
    if (!cf) {
      LOG(ERROR) << "AddProduct failed";
      status = INSTALL_FAILED;
    } else {
      ScopedTempDir temp_path;
      if (!temp_path.CreateUniqueTempDir()) {
        PLOG(ERROR) << "Failed to create Temp directory";
        return INSTALL_FAILED;
      }
      scoped_ptr<WorkItemList> item_list(WorkItem::CreateWorkItemList());
      const ProductState* chrome_state =
          machine_state.GetProductState(installer_state->system_install(),
                                        BrowserDistribution::CHROME_BROWSER);
      DCHECK(chrome_state);  // Checked in CheckQuickEnablePreconditions.

      // Temporarily remove Chrome from the product list.
      // This is so that the operations below do not affect the installation
      // state of Chrome.
      installer_state->RemoveProduct(
          installer_state->FindProduct(BrowserDistribution::CHROME_BROWSER));

      FilePath setup_path(chrome_state->GetSetupPath());
      const Version& new_version = chrome_state->version();
      FilePath new_chrome_exe(
          installer_state->target_path().Append(installer::kChromeNewExe));

      // This creates the uninstallation entry for GCF.
      AddUninstallShortcutWorkItems(*installer_state, setup_path, new_version,
                                    item_list.get(), *cf);
      AddVersionKeyWorkItems(installer_state->root_key(), cf->distribution(),
                             new_version, item_list.get());
      AddChromeFrameWorkItems(machine_state, *installer_state, setup_path,
                              new_version, *cf, item_list.get());

      const Version* opv = chrome_state->old_version();
      AppendPostInstallTasks(*installer_state, setup_path, new_chrome_exe, opv,
                             new_version, temp_path.path(), item_list.get());

      // Before updating the channel values, add Chrome back to the mix so that
      // all multi-installed products' channel values get updated.
      installer_state->AddProductFromState(BrowserDistribution::CHROME_BROWSER,
                                           *chrome_state);
      AddGoogleUpdateWorkItems(*installer_state, item_list.get());

      // Add the items to remove the quick-enable-cf command from the registry.
      AddQuickEnableWorkItems(*installer_state, machine_state,
                              &chrome_state->uninstall_command().GetProgram(),
                              &chrome_state->version(),
                              item_list.get());

      if (!item_list->Do()) {
        item_list->Rollback();
        status = INSTALL_FAILED;
      } else {
        DCHECK_EQ(FIRST_INSTALL_SUCCESS, status);
        VLOG(1) << "Chrome Frame successfully activated.";
      }
    }
  }

  return status;
}

}  // namespace installer
