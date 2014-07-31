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
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"

using extensions::Extension;

ExtensionSettingsUIBrowserTest::ExtensionSettingsUIBrowserTest()
    : profile_(NULL) {}

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
  InstallExtension(extensions_data_dir.AppendASCII("good.crx"));
}

class MockAutoConfirmExtensionInstallPrompt : public ExtensionInstallPrompt {
 public:
  explicit MockAutoConfirmExtensionInstallPrompt(
      content::WebContents* web_contents)
    : ExtensionInstallPrompt(web_contents) {}

  // Proceed without confirmation prompt.
  virtual void ConfirmInstall(
      Delegate* delegate,
      const Extension* extension,
      const ShowDialogCallback& show_dialog_callback) OVERRIDE {
    delegate->InstallUIProceed();
  }
};

const Extension* ExtensionSettingsUIBrowserTest::InstallExtension(
    const base::FilePath& path) {
  Profile* profile = this->GetProfile();
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  service->set_show_extensions_prompts(false);
  size_t num_before = service->extensions()->size();
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

  size_t num_after = service->extensions()->size();
  if (num_before + 1 != num_after) {
    VLOG(1) << "Num extensions before: " << base::IntToString(num_before)
            << " num after: " << base::IntToString(num_after)
            << " Installed extensions follow:";

    for (extensions::ExtensionSet::const_iterator it =
             service->extensions()->begin();
         it != service->extensions()->end(); ++it)
      VLOG(1) << "  " << (*it)->id();

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
