// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"

using extensions::Extension;

// NB: We use LoadExtension instead of InstallExtension for shared modules so
// the public-keys in their manifests are used to generate the extension ID, so
// it can be imported correctly.  We use InstallExtension otherwise so the loads
// happen through the CRX installer which validates imports.

#if defined(OS_WIN)
// Timing out on Win7 Tests bot. See http://crbug.com/395796 .
#define MAYBE_SharedModule DISABLED_SharedModule
#else
#define MAYBE_SharedModule SharedModule
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_SharedModule) {
  // import_pass depends on this shared module.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("shared_module").AppendASCII("shared")));

  EXPECT_TRUE(RunExtensionTest("shared_module/import_pass"));

  EXPECT_FALSE(InstallExtension(
      test_data_dir_.AppendASCII("shared_module")
          .AppendASCII("import_wrong_version"), 0));
  EXPECT_FALSE(InstallExtension(
      test_data_dir_.AppendASCII("shared_module")
          .AppendASCII("import_non_existent"), 0));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SharedModuleWhitelist) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("shared_module")
          .AppendASCII("shared_whitelist")));

  EXPECT_FALSE(InstallExtension(
      test_data_dir_.AppendASCII("shared_module")
          .AppendASCII("import_not_in_whitelist"), 0));
}

#if defined(OS_WIN)
// Timing out on Win7 Tests bot. See http://crbug.com/395796 .
#define MAYBE_SharedModuleInstallEvent DISALBED_SharedModuleInstallEvent
#else
#define MAYBE_SharedModuleInstallEvent SharedModuleInstallEvent
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_SharedModuleInstallEvent) {
  ExtensionTestMessageListener listener1("ready", false);

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("shared_module").AppendASCII("shared"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("shared_module").AppendASCII("import_pass"),
      1));

  EXPECT_TRUE(listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener listener2("shared_module_updated", false);
  ReloadExtension(extension->id());

  EXPECT_TRUE(listener2.WaitUntilSatisfied());
}
