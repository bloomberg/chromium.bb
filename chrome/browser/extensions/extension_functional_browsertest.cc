// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"

namespace extensions {

class ExtensionFunctionalTest : public ExtensionBrowserTest {
 public:
  void InstallExtensionSilently(ExtensionService* service,
                                const char* filename) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
    size_t num_before = registry->enabled_extensions().size();

    base::FilePath path = test_data_dir_.AppendASCII(filename);

    TestExtensionRegistryObserver extension_observer(registry);

    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service));
    installer->set_is_gallery_install(false);
    installer->set_allow_silent_install(true);
    installer->set_install_source(Manifest::INTERNAL);
    installer->set_off_store_install_allow_reason(
        CrxInstaller::OffStoreInstallAllowedInTest);

    observer_->Watch(NOTIFICATION_CRX_INSTALLER_DONE,
                     content::Source<CrxInstaller>(installer.get()));

    installer->InstallCrx(path);
    observer_->Wait();

    size_t num_after = registry->enabled_extensions().size();
    EXPECT_EQ(num_before + 1, num_after);

    extension_observer.WaitForExtensionLoaded();
    const Extension* extension =
        registry->enabled_extensions().GetByID(last_loaded_extension_id());
    EXPECT_TRUE(extension);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest,
                       PRE_TestAdblockExtensionCrash) {
  InstallExtensionSilently(extension_service(), "adblock.crx");
}

// Timing out on XP and Vista: http://crbug.com/387866
#if defined(OS_WIN)
#define MAYBE_TestAdblockExtensionCrash DISABLED_TestAdblockExtensionCrash
#else
#define MAYBE_TestAdblockExtensionCrash TestAdblockExtensionCrash
#endif
IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest,
                       MAYBE_TestAdblockExtensionCrash) {
  ExtensionService* service = extension_service();
  // Verify that the extension is enabled and allowed in incognito
  // is disabled.
  EXPECT_TRUE(service->IsExtensionEnabled(last_loaded_extension_id()));
  EXPECT_FALSE(util::IsIncognitoEnabled(last_loaded_extension_id(), profile()));
}

// Failing on Linux: http://crbug.com/654945
#if defined(OS_LINUX)
#define MAYBE_TestSetExtensionsState DISABLED_TestSetExtensionsState
#else
#define MAYBE_TestSetExtensionsState TestSetExtensionsState
#endif
IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest, MAYBE_TestSetExtensionsState) {
  InstallExtensionSilently(extension_service(), "google_talk.crx");

  // Disable the extension and verify.
  util::SetIsIncognitoEnabled(last_loaded_extension_id(), profile(), false);
  ExtensionService* service = extension_service();
  service->DisableExtension(last_loaded_extension_id(),
                            Extension::DISABLE_USER_ACTION);
  EXPECT_FALSE(service->IsExtensionEnabled(last_loaded_extension_id()));

  // Enable the extension and verify.
  util::SetIsIncognitoEnabled(last_loaded_extension_id(), profile(), false);
  service->EnableExtension(last_loaded_extension_id());
  EXPECT_TRUE(service->IsExtensionEnabled(last_loaded_extension_id()));

  // Allow extension in incognito mode and verify.
  service->EnableExtension(last_loaded_extension_id());
  util::SetIsIncognitoEnabled(last_loaded_extension_id(), profile(), true);
  EXPECT_TRUE(util::IsIncognitoEnabled(last_loaded_extension_id(), profile()));

  // Disallow extension in incognito mode and verify.
  service->EnableExtension(last_loaded_extension_id());
  util::SetIsIncognitoEnabled(last_loaded_extension_id(), profile(), false);
  EXPECT_FALSE(util::IsIncognitoEnabled(last_loaded_extension_id(), profile()));
}

}  // namespace extensions
