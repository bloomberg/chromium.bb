// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST_F(ExtensionManifestTest, PageActionManifestVersion2) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("page_action_manifest_version_2.json"));
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->page_action_info());

  EXPECT_EQ("", extension->page_action_info()->id);
  EXPECT_TRUE(extension->page_action_info()->default_icon.empty());
  EXPECT_EQ("", extension->page_action_info()->default_title);
  EXPECT_TRUE(extension->page_action_info()->default_popup_url.is_empty());

  LoadAndExpectError("page_action_manifest_version_2b.json",
                     extension_manifest_errors::kInvalidPageActionPopup);
}

}  // namespace extensions
