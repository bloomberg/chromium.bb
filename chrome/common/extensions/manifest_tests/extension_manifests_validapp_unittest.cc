// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/values.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST_F(ExtensionManifestTest, ValidApp) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("valid_app.json"));
  extensions::URLPatternSet expected_patterns;
  AddPattern(&expected_patterns, "http://www.google.com/mail/*");
  AddPattern(&expected_patterns, "http://www.google.com/foobar/*");
  EXPECT_EQ(expected_patterns, extension->web_extent());
  EXPECT_EQ(extension_misc::LAUNCH_TAB, extension->launch_container());
  EXPECT_EQ("http://www.google.com/mail/", extension->launch_web_url());
}

TEST_F(ExtensionManifestTest, AllowUnrecognizedPermissions) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(LoadManifest("valid_app.json", &error));
  ListValue* permissions = NULL;
  ASSERT_TRUE(manifest->GetList("permissions", &permissions));
  permissions->Append(new StringValue("not-a-valid-permission"));
  LoadAndExpectSuccess(Manifest(manifest.get(), ""));
}
