// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/api/extension_action/browser_action_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class UIManifestTest : public ExtensionManifestTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ManifestHandler::Register(extension_manifest_keys::kBrowserAction,
                              make_linked_ptr(new BrowserActionHandler));
  }
};

TEST_F(UIManifestTest, DisallowMultipleUISurfaces) {
  LoadAndExpectError("multiple_ui_surfaces.json",
                     extension_manifest_errors::kOneUISurfaceOnly);
}

}  // namespace extensions
