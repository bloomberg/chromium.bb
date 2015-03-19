// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

using DeveloperPrivateApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest, Basics) {
  // Load up some extensions so that we can query their info and adjust their
  // setings in the API test.
  base::FilePath base_dir = test_data_dir_.AppendASCII("developer");
  EXPECT_TRUE(LoadExtension(base_dir.AppendASCII("hosted_app")));
  EXPECT_TRUE(InstallExtension(
      base_dir.AppendASCII("packaged_app"), 1, Manifest::INTERNAL));
  LoadExtension(base_dir.AppendASCII("simple_extension"));

  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "developer/test", kFlagLoadAsComponent));
}

}  // namespace extensions
