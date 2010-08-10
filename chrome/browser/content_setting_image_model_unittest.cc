// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_image_model.h"

#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef RenderViewHostTestHarness ContentSettingImageModelTest;

TEST_F(ContentSettingImageModelTest, UpdateFromTabContents) {
  TestTabContents tab_contents(profile_.get(), NULL);
  TabSpecificContentSettings* content_settings =
      tab_contents.GetTabSpecificContentSettings();
  scoped_ptr<ContentSettingImageModel> content_setting_image_model(
     ContentSettingImageModel::CreateContentSettingImageModel(
         CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->get_icon());
  EXPECT_EQ("", content_setting_image_model->get_tooltip());

  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES);
  content_setting_image_model->UpdateFromTabContents(&tab_contents);

  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_NE("", content_setting_image_model->get_tooltip());
}
