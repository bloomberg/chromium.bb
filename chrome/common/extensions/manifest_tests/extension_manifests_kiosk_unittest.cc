// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace errors = extension_manifest_errors;

namespace extensions {

class ExtensionManifestKioskEnabledTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new BackgroundManifestHandler)->Register();
  }
};

TEST_F(ExtensionManifestKioskEnabledTest, InvalidKioskEnabled) {
  LoadAndExpectError("kiosk_enabled_invalid.json",
                     errors::kInvalidKioskEnabled);
}

TEST_F(ExtensionManifestKioskEnabledTest, KioskEnabledHostedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_hosted_app.json"));
  EXPECT_FALSE(extension->kiosk_enabled());
}

TEST_F(ExtensionManifestKioskEnabledTest, KioskEnabledPackagedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_packaged_app.json"));
  EXPECT_FALSE(extension->kiosk_enabled());
}

TEST_F(ExtensionManifestKioskEnabledTest, KioskEnabledExtension) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_extension.json"));
  EXPECT_FALSE(extension->kiosk_enabled());
}

TEST_F(ExtensionManifestKioskEnabledTest, KioskEnabledPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_platform_app.json"));
  EXPECT_TRUE(extension->kiosk_enabled());
}

TEST_F(ExtensionManifestKioskEnabledTest, KioskDisabledPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_disabled_platform_app.json"));
  EXPECT_FALSE(extension->kiosk_enabled());
}

TEST_F(ExtensionManifestKioskEnabledTest, KioskDefaultPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_default_platform_app.json"));
  EXPECT_FALSE(extension->kiosk_enabled());
}

}  // namespace extensions
