// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/page_launcher/page_launcher_handler.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

namespace extensions {

class PageLauncherManifestTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new PageLauncherHandler)->Register();
    // Required to be recognized as a platform app.
    (new BackgroundManifestHandler)->Register();
  }

  virtual char const* test_data_dir() OVERRIDE {
    return "page_launcher";
  }
};

TEST_F(PageLauncherManifestTest, AppPageLauncherNotPresent) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("app_page_launcher_not_present.json");
  const ActionInfo* page_launcher_info =
      ActionInfo::GetPageLauncherInfo(extension.get());
  EXPECT_EQ(NULL, page_launcher_info);
}

TEST_F(PageLauncherManifestTest, AppPageLauncherPresent) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("app_page_launcher_present.json");
  const ActionInfo* page_launcher_info =
      ActionInfo::GetPageLauncherInfo(extension.get());
  ASSERT_TRUE(page_launcher_info);
  EXPECT_EQ("title", page_launcher_info->default_title);
  EXPECT_TRUE(page_launcher_info->default_icon.ContainsPath("image.jpg"));
}

TEST_F(PageLauncherManifestTest, ExtensionWithPageLauncher) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("extension_page_launcher_present.json");
  const ActionInfo* page_launcher_info =
      ActionInfo::GetPageLauncherInfo(extension.get());
  ASSERT_EQ(NULL, page_launcher_info);
}

}  // namespace extensions
