// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_settings_browsertest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_set.h"

using extensions::Extension;
using extensions::TestManagementPolicyProvider;

ExtensionSettingsUIBrowserTest::ExtensionSettingsUIBrowserTest()
    : profile_(NULL),
      policy_provider_(TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS |
                       TestManagementPolicyProvider::MUST_REMAIN_ENABLED |
                       TestManagementPolicyProvider::MUST_REMAIN_INSTALLED) {}

ExtensionSettingsUIBrowserTest::~ExtensionSettingsUIBrowserTest() {}

Profile* ExtensionSettingsUIBrowserTest::GetProfile() {
  if (!profile_) {
    profile_ = browser() ? browser()->profile() :
                           ProfileManager::GetActiveUserProfile();
  }
  return profile_;
}

void ExtensionSettingsUIBrowserTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();
  observer_.reset(new ExtensionTestNotificationObserver(browser()));
}

void ExtensionSettingsUIBrowserTest::InstallGoodExtension() {
  base::FilePath test_data_dir;
  if (!PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
    ADD_FAILURE();
    return;
  }
  base::FilePath extensions_data_dir = test_data_dir.AppendASCII("extensions");
  EXPECT_TRUE(InstallExtension(extensions_data_dir.AppendASCII("good.crx")));
}

void ExtensionSettingsUIBrowserTest::InstallErrorsExtension() {
  base::FilePath test_data_dir;
  if (!PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
    ADD_FAILURE();
    return;
  }
  base::FilePath extensions_data_dir = test_data_dir.AppendASCII("extensions");
  extensions_data_dir = extensions_data_dir.AppendASCII("error_console");
  EXPECT_TRUE(InstallUnpackedExtension(
      extensions_data_dir.AppendASCII("runtime_and_manifest_errors"),
      "pdlpifnclfacjobnmbpngemkalkjamnf"));
}

void ExtensionSettingsUIBrowserTest::AddManagedPolicyProvider() {
  auto* extension_service = extensions::ExtensionSystem::Get(GetProfile());
  extension_service->management_policy()->RegisterProvider(&policy_provider_);
}

class MockAutoConfirmExtensionInstallPrompt : public ExtensionInstallPrompt {
 public:
  explicit MockAutoConfirmExtensionInstallPrompt(
      content::WebContents* web_contents)
    : ExtensionInstallPrompt(web_contents) {}

  // Proceed without confirmation prompt.
  void ConfirmInstall(Delegate* delegate,
                      const Extension* extension,
                      const ShowDialogCallback& show_dialog_callback) override {
    delegate->InstallUIProceed();
  }
};

const Extension* ExtensionSettingsUIBrowserTest::InstallUnpackedExtension(
    const base::FilePath& path, const char* id) {
  if (path.empty())
    return NULL;

  Profile* profile = GetProfile();
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  service->set_show_extensions_prompts(false);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  extensions::TestExtensionRegistryObserver observer(registry, id);

  extensions::UnpackedInstaller::Create(service)->Load(path);

  // Test will timeout if extension is not loaded.
  observer.WaitForExtensionLoaded();
  return service->GetExtensionById(last_loaded_extension_id(), false);
}

const Extension* ExtensionSettingsUIBrowserTest::InstallExtension(
    const base::FilePath& path) {
  Profile* profile = GetProfile();
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  service->set_show_extensions_prompts(false);
  size_t num_before = registry->enabled_extensions().size();
  {
    scoped_ptr<ExtensionInstallPrompt> install_ui;
    install_ui.reset(new MockAutoConfirmExtensionInstallPrompt(
        browser()->tab_strip_model()->GetActiveWebContents()));

    base::FilePath crx_path = path;
    DCHECK(crx_path.Extension() == FILE_PATH_LITERAL(".crx"));
    if (crx_path.empty())
      return NULL;

    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::Create(service, install_ui.Pass()));
    installer->set_expected_id(std::string());
    installer->set_is_gallery_install(false);
    installer->set_install_source(extensions::Manifest::INTERNAL);
    installer->set_install_immediately(true);
    installer->set_off_store_install_allow_reason(
        extensions::CrxInstaller::OffStoreInstallAllowedInTest);

    observer_->Watch(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<extensions::CrxInstaller>(installer.get()));

    installer->InstallCrx(crx_path);

    observer_->Wait();
  }

  size_t num_after = registry->enabled_extensions().size();
  if (num_before + 1 != num_after) {
    VLOG(1) << "Num extensions before: " << base::IntToString(num_before)
            << " num after: " << base::IntToString(num_after)
            << " Installed extensions follow:";

    for (const scoped_refptr<const Extension>& extension :
         registry->enabled_extensions())
      VLOG(1) << "  " << extension->id();

    VLOG(1) << "Errors follow:";
    const std::vector<base::string16>* errors =
        ExtensionErrorReporter::GetInstance()->GetErrors();
    for (std::vector<base::string16>::const_iterator iter = errors->begin();
         iter != errors->end(); ++iter)
      VLOG(1) << *iter;

    return NULL;
  }

  if (!observer_->WaitForExtensionViewsToLoad())
    return NULL;
  return service->GetExtensionById(last_loaded_extension_id(), false);
}
