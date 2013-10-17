// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"

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
        extensions::CrxInstaller::CreateSilent(service));
    installer->set_is_gallery_install(false);
    installer->set_allow_silent_install(true);
    installer->set_install_source(Manifest::INTERNAL);
    installer->set_off_store_install_allow_reason(
        extensions::CrxInstaller::OffStoreInstallAllowedInTest);

    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<extensions::CrxInstaller>(installer.get()));
    content::NotificationRegistrar registrar;
    registrar.Add(this, chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                  content::Source<extensions::CrxInstaller>(installer.get()));

    installer->InstallCrx(path);
    observer.Wait();

    size_t num_after = service->extensions()->size();
    EXPECT_EQ(num_before + 1, num_after);

    extension_loaded_observer.Wait();
    const Extension* extension = service->GetExtensionById(
        last_loaded_extension_id_, false);
    EXPECT_TRUE(extension != NULL);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest,
                       PRE_TestAdblockExtensionCrash) {
  ExtensionService* service = profile()->GetExtensionService();
  InstallExtensionSilently(service, "adblock.crx");
}

IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest, TestAdblockExtensionCrash) {
  ExtensionService* service = profile()->GetExtensionService();
  // Verify that the extension is enabled and allowed in incognito
  // is disabled.
  EXPECT_TRUE(service->IsExtensionEnabled(last_loaded_extension_id_));
  EXPECT_FALSE(extension_util::IsIncognitoEnabled(last_loaded_extension_id_,
                                                  service));
}

IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest, TestSetExtensionsState) {
  ExtensionService* service = profile()->GetExtensionService();
  InstallExtensionSilently(service, "google_talk.crx");

  // Disable the extension and verify.
  extension_util::SetIsIncognitoEnabled(
      last_loaded_extension_id_, service, false);
  service->DisableExtension(last_loaded_extension_id_,
                            Extension::DISABLE_USER_ACTION);
  EXPECT_FALSE(service->IsExtensionEnabled(last_loaded_extension_id_));

  // Enable the extension and verify.
  extension_util::SetIsIncognitoEnabled(
      last_loaded_extension_id_, service, false);
  service->EnableExtension(last_loaded_extension_id_);
  EXPECT_TRUE(service->IsExtensionEnabled(last_loaded_extension_id_));

  // Allow extension in incognito mode and verify.
  service->EnableExtension(last_loaded_extension_id_);
  extension_util::SetIsIncognitoEnabled(
      last_loaded_extension_id_, service, true);
  EXPECT_TRUE(extension_util::IsIncognitoEnabled(last_loaded_extension_id_,
                                                 service));

  // Disallow extension in incognito mode and verify.
  service->EnableExtension(last_loaded_extension_id_);
  extension_util::SetIsIncognitoEnabled(
      last_loaded_extension_id_, service, false);
  EXPECT_FALSE(extension_util::IsIncognitoEnabled(last_loaded_extension_id_,
                                                  service));
}
}  // namespace extensions
