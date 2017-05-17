// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extensions::manifest_keys;

TEST_F(ChromeManifestTest, StorageAPIManifestVersionAvailability) {
  base::DictionaryValue base_manifest;
  {
    base_manifest.SetString(keys::kName, "test");
    base_manifest.SetString(keys::kVersion, "0.1");
    auto permissions = base::MakeUnique<base::ListValue>();
    permissions->AppendString("storage");
    base_manifest.Set(keys::kPermissions, std::move(permissions));
  }

  std::string kManifestVersionError =
      "'storage' requires manifest version of at least 2.";

  // Extension with no manifest version cannot use storage API.
  {
    ManifestData manifest(&base_manifest, "test");
    LoadAndExpectWarning(manifest, kManifestVersionError);
  }

  // Extension with manifest version 1 cannot use storage API.
  {
    base::DictionaryValue manifest_with_version;
    manifest_with_version.SetInteger(keys::kManifestVersion, 1);
    manifest_with_version.MergeDictionary(&base_manifest);

    ManifestData manifest(&manifest_with_version, "test");
    LoadAndExpectWarning(manifest, kManifestVersionError);
  }

  // Extension with manifest version 2 *can* use storage API.
  {
    base::DictionaryValue manifest_with_version;
    manifest_with_version.SetInteger(keys::kManifestVersion, 2);
    manifest_with_version.MergeDictionary(&base_manifest);

    ManifestData manifest(&manifest_with_version, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());
  }
}
