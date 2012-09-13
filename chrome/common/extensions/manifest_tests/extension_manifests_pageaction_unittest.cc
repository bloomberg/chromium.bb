// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, PageActionManifestVersion2) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("page_action_manifest_version_2.json"));
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->page_action());

  EXPECT_EQ("", extension->page_action()->id());
  EXPECT_TRUE(extension->page_action()->default_icon_path().empty());
  EXPECT_EQ("", extension->page_action()->GetTitle(
      ExtensionAction::kDefaultTabId));
  EXPECT_FALSE(extension->page_action()->HasPopup(
      ExtensionAction::kDefaultTabId));

  LoadAndExpectError("page_action_manifest_version_2b.json",
                     errors::kInvalidPageActionPopup);
}
