// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using extensions::Extension;

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, ManifestVersionWarning) {
  scoped_ptr<DictionaryValue> manifest1(new DictionaryValue());
  manifest1->SetString("name", "Miles");
  manifest1->SetString("version", "0.55");

  scoped_ptr<DictionaryValue> manifest2(manifest1->DeepCopy());
  manifest2->SetInteger("manifest_version", 1);

  scoped_ptr<DictionaryValue> manifest3(manifest1->DeepCopy());
  manifest3->SetInteger("manifest_version", 2);

  struct {
    const char* test_name;
    DictionaryValue* manifest;
    bool expect_warning;
  } test_data[] = {
    { "default_manifest_version", manifest1.get(), true },
    { "manifest_version_1", manifest2.get(), true },
    { "manifest_version_2", manifest3.get(), false }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    scoped_refptr<Extension> extension(
        LoadAndExpectSuccess(Manifest(test_data[i].manifest,
                                      test_data[i].test_name),
                             Extension::LOAD));
    if (test_data[i].expect_warning) {
      EXPECT_THAT(
          extension->install_warnings(),
          testing::Contains(
              testing::Field(
                  &Extension::InstallWarning::message,
                  testing::HasSubstr(
                      "Support for manifest version 1 is being phased out."))))
          << test_data[i].test_name;
    } else {
      EXPECT_THAT(
          extension->install_warnings(),
          testing::Not(
              testing::Contains(
                  testing::Field(
                      &Extension::InstallWarning::message,
                      testing::HasSubstr(
                          "Support for manifest version 1 is being phased "
                          "out.")))))
          << test_data[i].test_name;
    }
  }
}
