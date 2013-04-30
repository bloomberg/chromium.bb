// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

using extensions::Extension;

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SharedModule) {
  // import_pass depends on this shared module.
  // NB: We use LoadExtension instead of InstallExtension here so the public-key
  // in 'shared' is used to generate the extension ID so it can be imported
  // correctly.  We use InstallExtension otherwise so the loads happen through
  // the CRX installer which validates imports.
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
