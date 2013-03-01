// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsPageManifestTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new extensions::DevToolsPageHandler)->Register();
  }
};

TEST_F(DevToolsPageManifestTest, DevToolsExtensions) {
  LoadAndExpectError("devtools_extension_url_invalid_type.json",
                     extension_manifest_errors::kInvalidDevToolsPage);

  scoped_refptr<extensions::Extension> extension;
  extension = LoadAndExpectSuccess("devtools_extension.json");
  EXPECT_EQ(extension->url().spec() + "devtools.html",
            extensions::ManifestURL::GetDevToolsPage(extension).spec());
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());
}
