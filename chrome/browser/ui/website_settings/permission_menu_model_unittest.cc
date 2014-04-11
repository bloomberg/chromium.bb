// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class TestDelegate : public PermissionMenuModel::Delegate {
 public:
  TestDelegate() : current_(-1) {}

  virtual void ExecuteCommand(int command_id) OVERRIDE {
    current_ = command_id;
  }
  virtual bool IsCommandIdChecked(int command_id) OVERRIDE {
    return (current_ == command_id);
  }

  int current_;
};

}  // namespace

TEST(PermissionMenuModelTest, TestDefault) {
  TestDelegate delegate;
  PermissionMenuModel model(&delegate,
                            GURL("http://www.google.com"),
                            CONTENT_SETTINGS_TYPE_COOKIES,
                            CONTENT_SETTING_ALLOW,
                            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(3, model.GetItemCount());
}

TEST(PermissionMenuModelTest, TestDefaultMediaHttp) {
  TestDelegate delegate;
  PermissionMenuModel model(&delegate,
                            GURL("http://www.google.com"),
                            CONTENT_SETTINGS_TYPE_MEDIASTREAM,
                            CONTENT_SETTING_ALLOW,
                            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(2, model.GetItemCount());
}

TEST(PermissionMenuModelTest, TestAllowBlock) {
  TestDelegate delegate;
  PermissionMenuModel model(&delegate,
                            GURL("http://www.google.com"),
                            CONTENT_SETTINGS_TYPE_DEFAULT,
                            CONTENT_SETTING_NUM_SETTINGS,
                            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(2, model.GetItemCount());
}
