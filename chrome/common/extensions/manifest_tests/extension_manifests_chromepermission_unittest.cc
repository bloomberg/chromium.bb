// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, ChromeURLPermissionInvalid) {
  LoadAndExpectError("permission_chrome_url_invalid.json",
                     errors::kInvalidPermissionScheme);
}

TEST_F(ExtensionManifestTest, ChromeURLPermissionAllowedWithFlag) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  std::string error;
  scoped_refptr<extensions::Extension> extension =
    LoadAndExpectSuccess("permission_chrome_url_invalid.json");
  EXPECT_EQ("", error);
  const GURL newtab_url("chrome://newtab/");
  EXPECT_TRUE(extension->CanExecuteScriptOnPage(newtab_url,
                                                newtab_url,
                                                0,
                                                NULL,
                                                &error)) << error;
}

TEST_F(ExtensionManifestTest, ChromeResourcesPermissionValidOnlyForComponents) {
  LoadAndExpectError("permission_chrome_resources_url.json",
                     errors::kInvalidPermissionScheme);
  std::string error;
  LoadExtension(Manifest("permission_chrome_resources_url.json"),
                &error, extensions::Manifest::COMPONENT,
                extensions::Extension::NO_FLAGS);
  EXPECT_EQ("", error);
}
