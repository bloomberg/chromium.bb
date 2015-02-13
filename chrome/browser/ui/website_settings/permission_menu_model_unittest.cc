// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class TestCallback {
 public:
  TestCallback() : current_(-1) {}

  PermissionMenuModel::ChangeCallback callback() {
    return base::Bind(&TestCallback::PermissionChanged, base::Unretained(this));
  }
  void PermissionChanged(const WebsiteSettingsUI::PermissionInfo& permission) {
    current_ = permission.setting;
  }

  int current_;
};

}  // namespace

TEST(PermissionMenuModelTest, TestDefault) {
  TestCallback callback;
  WebsiteSettingsUI::PermissionInfo permission;
  permission.type = CONTENT_SETTINGS_TYPE_COOKIES;
  permission.setting = CONTENT_SETTING_ALLOW;
  permission.default_setting = CONTENT_SETTING_ALLOW;
  PermissionMenuModel model(
      GURL("http://www.google.com"), permission, callback.callback());
  EXPECT_EQ(3, model.GetItemCount());
}

TEST(PermissionMenuModelTest, TestDefaultMediaHttp) {
  TestCallback callback;
  WebsiteSettingsUI::PermissionInfo permission;
  permission.type = CONTENT_SETTINGS_TYPE_MEDIASTREAM;
  permission.setting = CONTENT_SETTING_ALLOW;
  permission.default_setting = CONTENT_SETTING_ALLOW;
  PermissionMenuModel model(
      GURL("http://www.google.com"), permission, callback.callback());
  EXPECT_EQ(2, model.GetItemCount());
}

TEST(PermissionMenuModelTest, TestAllowBlock) {
  TestCallback callback;
  PermissionMenuModel model(GURL("http://www.google.com"),
                            CONTENT_SETTING_ALLOW,
                            callback.callback());
  EXPECT_EQ(2, model.GetItemCount());
}

TEST(PermissionMenuModelTest, TestFullscreenMouseLockFileUrl) {
  TestCallback callback;
  WebsiteSettingsUI::PermissionInfo permission;
  permission.type = CONTENT_SETTINGS_TYPE_FULLSCREEN;
  permission.setting = CONTENT_SETTING_ASK;
  permission.default_setting = CONTENT_SETTING_ASK;
  PermissionMenuModel fullscreen_model(GURL("file:///test.html"), permission,
                                       callback.callback());
  EXPECT_EQ(1, fullscreen_model.GetItemCount());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_ASK),
      fullscreen_model.GetLabelAt(0));

  permission.type = CONTENT_SETTINGS_TYPE_MOUSELOCK;
  PermissionMenuModel mouselock_model(GURL("file:///test.html"), permission,
                                      callback.callback());
  EXPECT_EQ(1, mouselock_model.GetItemCount());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_ASK),
      fullscreen_model.GetLabelAt(0));
}
