// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(GOOGLE_CHROME_BUILD)

#include "chrome/installer/setup/app_launcher_installer.h"

#include "base/strings/string16.h"
#include "base/version.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/updating_app_registration_data.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

#include "installer_util_strings.h"  // NOLINT

namespace installer {

namespace {

// The legacy command ids for installing an application or extension. These are
// only here so they can be removed from the registry.
const wchar_t kLegacyCmdInstallApp[] = L"install-application";
const wchar_t kLegacyCmdInstallExtension[] = L"install-extension";
const wchar_t kLegacyCmdQueryEULAAcceptance[] = L"query-eula-acceptance";
const wchar_t kLegacyCmdQuickEnableApplicationHost[] =
    L"quick-enable-application-host";

// The legacy app_host.exe executable, which should be eradicated.
const wchar_t kLegacyChromeAppHostExe[] = L"app_host.exe";

base::string16 GetAppLauncherDisplayName() {
  return GetLocalizedString(IDS_PRODUCT_APP_LAUNCHER_NAME_BASE);
}

void AddLegacyAppCommandRemovalItem(const InstallerState& installer_state,
                                    const AppRegistrationData& reg_data,
                                    const wchar_t* name,
                                    WorkItemList* list) {
  // Ignore failures since this is a clean-up operation and shouldn't block
  // install or update.
  list->AddDeleteRegKeyWorkItem(
                      installer_state.root_key(),
                      GetRegistrationDataCommandKey(reg_data, name),
                      KEY_WOW64_32KEY)
      ->set_ignore_failure(true);
}

}  // namespace

void AddAppLauncherVersionKeyWorkItems(HKEY root,
                                       const base::Version& new_version,
                                       bool add_language_identifier,
                                       WorkItemList* list) {
  DCHECK(!InstallUtil::IsChromeSxSProcess());
  const UpdatingAppRegistrationData app_launcher_reg_data(kAppLauncherGuid);
  AddVersionKeyWorkItems(root,
                         app_launcher_reg_data.GetVersionKey(),
                         GetAppLauncherDisplayName(),
                         new_version,
                         add_language_identifier,
                         list);
}

void RemoveAppLauncherVersionKey(HKEY reg_root) {
  DCHECK(!InstallUtil::IsChromeSxSProcess());
  const UpdatingAppRegistrationData app_launcher_reg_data(kAppLauncherGuid);
  InstallUtil::DeleteRegistryKey(
      reg_root, app_launcher_reg_data.GetVersionKey(), KEY_WOW64_32KEY);
}

void AddRemoveLegacyAppHostExeWorkItems(const base::FilePath& target_path,
                                        const base::FilePath& temp_path,
                                        WorkItemList* list) {
  DCHECK(!InstallUtil::IsChromeSxSProcess());
  list->AddDeleteTreeWorkItem(
      target_path.Append(kLegacyChromeAppHostExe),
      temp_path)->set_ignore_failure(true);
}

void AddRemoveLegacyAppCommandsWorkItems(const InstallerState& installer_state,
                                         WorkItemList* list) {
  DCHECK(!InstallUtil::IsChromeSxSProcess());
  DCHECK(list);
  for (const auto* p : installer_state.products()) {
    if (p->is_chrome()) {
      // Remove "install-application" command from App Launcher.
      const UpdatingAppRegistrationData app_launcher_reg_data(kAppLauncherGuid);
      AddLegacyAppCommandRemovalItem(installer_state, app_launcher_reg_data,
                                     kLegacyCmdInstallApp, list);

      // Remove "install-extension" command from Chrome.
      const AppRegistrationData& chrome_reg_data =
          p->distribution()->GetAppRegistrationData();
      AddLegacyAppCommandRemovalItem(installer_state, chrome_reg_data,
                                     kLegacyCmdInstallExtension, list);
    }
    if (p->is_chrome_binaries()) {
      const AppRegistrationData& binaries_reg_data =
          p->distribution()->GetAppRegistrationData();
      // Remove "query-eula-acceptance" command from Binaries.
      AddLegacyAppCommandRemovalItem(installer_state, binaries_reg_data,
                                     kLegacyCmdQueryEULAAcceptance, list);
      // Remove "quick-enable-application-host" command from Binaries.
      AddLegacyAppCommandRemovalItem(installer_state, binaries_reg_data,
          kLegacyCmdQuickEnableApplicationHost, list);
    }
  }
}

}  // namespace installer

#endif  // GOOGLE_CHROME_BUILD
