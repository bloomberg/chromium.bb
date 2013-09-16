// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class UIManifestTest : public ExtensionManifestTest {
};

TEST_F(UIManifestTest, DisallowMultipleUISurfaces) {
  LoadAndExpectError("multiple_ui_surfaces.json",
                     manifest_errors::kOneUISurfaceOnly);
}

}  // namespace extensions
