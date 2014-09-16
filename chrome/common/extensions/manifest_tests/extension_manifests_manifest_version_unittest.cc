// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using extensions::Extension;

namespace errors = extensions::manifest_errors;

TEST_F(ExtensionManifestTest, ManifestVersionError) {
  scoped_ptr<base::DictionaryValue> manifest1(new base::DictionaryValue());
  manifest1->SetString("name", "Miles");
  manifest1->SetString("version", "0.55");

  scoped_ptr<base::DictionaryValue> manifest2(manifest1->DeepCopy());
  manifest2->SetInteger("manifest_version", 1);

  scoped_ptr<base::DictionaryValue> manifest3(manifest1->DeepCopy());
  manifest3->SetInteger("manifest_version", 2);

  struct {
    const char* test_name;
    bool require_modern_manifest_version;
    base::DictionaryValue* manifest;
    bool expect_error;
  } test_data[] = {
    { "require_modern_with_default", true, manifest1.get(), true },
    { "require_modern_with_v1", true, manifest2.get(), true },
    { "require_modern_with_v2", true, manifest3.get(), false },
    { "dont_require_modern_with_default", false, manifest1.get(), false },
    { "dont_require_modern_with_v1", false, manifest2.get(), false },
    { "dont_require_modern_with_v2", false, manifest3.get(), false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    int create_flags = Extension::NO_FLAGS;
    if (test_data[i].require_modern_manifest_version)
      create_flags |= Extension::REQUIRE_MODERN_MANIFEST_VERSION;
    if (test_data[i].expect_error) {
        LoadAndExpectError(
            Manifest(test_data[i].manifest,
                     test_data[i].test_name),
            errors::kInvalidManifestVersionOld,
            extensions::Manifest::UNPACKED,
            create_flags);
    } else {
      LoadAndExpectSuccess(Manifest(test_data[i].manifest,
                                    test_data[i].test_name),
                           extensions::Manifest::UNPACKED,
                           create_flags);
    }
  }
}
