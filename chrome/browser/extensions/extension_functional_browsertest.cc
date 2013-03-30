// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"

namespace extensions {

class ExtensionFunctionalTest : public ExtensionBrowserTest {
public:
  void InstallExtensionSilently(ExtensionService* service,
                                const char* filename) {
    service->set_show_extensions_prompts(false);
    size_t num_before = service->extensions()->size();

    base::FilePath path = test_data_dir_.AppendASCII(filename);

    content::WindowedNotificationObserver extension_loaded_observer(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        content::NotificationService::AllSources());

    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::Create(service, NULL));
    installer->set_is_gallery_install(false);
    installer->set_allow_silent_install(true);
    installer->set_install_source(Manifest::INTERNAL);
    installer->set_off_store_install_allow_reason(
        extensions::CrxInstaller::OffStoreInstallAllowedInTest);

    content::NotificationRegistrar registrar;
    registrar.Add(this, chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                  content::Source<extensions::CrxInstaller>(installer.get()));

    installer->InstallCrx(path);
    content::RunMessageLoop();

    size_t num_after = service->extensions()->size();
    EXPECT_EQ(num_before + 1, num_after);

    extension_loaded_observer.Wait();
    const Extension* extension = service->GetExtensionById(
        last_loaded_extension_id_, false);
    EXPECT_TRUE(extension != NULL);
  }
};

// Failing on mac_rel trybots with timeout error. Disabling for now.
#if defined(OS_MACOSX)
#define TestAdblockExtensionCrash DISABLED_TestAdblockExtensionCrash
#endif
IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest, TestAdblockExtensionCrash) {
  ExtensionService* service = profile()->GetExtensionService();
  InstallExtensionSilently(service, "adblock.crx");

  // Restart the browser.
  chrome::Exit();
  chrome::NewWindow(browser());

  // Verify that the extension is enabled and allowed in incognito
  // is disabled.
  EXPECT_TRUE(service->IsExtensionEnabled(last_loaded_extension_id_));
  EXPECT_FALSE(service->IsIncognitoEnabled(last_loaded_extension_id_));
}

IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest, TestSetExtensionsState) {
  ExtensionService* service = profile()->GetExtensionService();
  InstallExtensionSilently(service, "google_talk.crx");

  // Disable the extension and verify.
  service->SetIsIncognitoEnabled(last_loaded_extension_id_, false);
  service->DisableExtension(last_loaded_extension_id_,
                            Extension::DISABLE_USER_ACTION);
  EXPECT_FALSE(service->IsExtensionEnabled(last_loaded_extension_id_));

  // Enable the extension and verify.
  service->SetIsIncognitoEnabled(last_loaded_extension_id_, false);
  service->EnableExtension(last_loaded_extension_id_);
  EXPECT_TRUE(service->IsExtensionEnabled(last_loaded_extension_id_));

  // Allow extension in incognito mode and verify.
  service->EnableExtension(last_loaded_extension_id_);
  service->SetIsIncognitoEnabled(last_loaded_extension_id_, true);
  EXPECT_TRUE(service->IsIncognitoEnabled(last_loaded_extension_id_));

  // Disallow extension in incognito mode and verify.
  service->EnableExtension(last_loaded_extension_id_);
  service->SetIsIncognitoEnabled(last_loaded_extension_id_, false);
  EXPECT_FALSE(service->IsIncognitoEnabled(last_loaded_extension_id_));
}
}  // namespace extensions
