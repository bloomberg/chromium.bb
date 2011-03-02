// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "net/base/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContentSettingImageModelTest : public RenderViewHostTestHarness {
 public:
  ContentSettingImageModelTest()
      : RenderViewHostTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {}

 private:
  BrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageModelTest);
};

TEST_F(ContentSettingImageModelTest, UpdateFromTabContents) {
  TestTabContents tab_contents(profile_.get(), NULL);
  TabSpecificContentSettings* content_settings =
      tab_contents.GetTabSpecificContentSettings();
  scoped_ptr<ContentSettingImageModel> content_setting_image_model(
     ContentSettingImageModel::CreateContentSettingImageModel(
         CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->get_icon());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES,
                                     std::string());
  content_setting_image_model->UpdateFromTabContents(&tab_contents);

  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, CookieAccessed) {
  TestTabContents tab_contents(profile_.get(), NULL);
  TabSpecificContentSettings* content_settings =
      tab_contents.GetTabSpecificContentSettings();
  profile_->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  scoped_ptr<ContentSettingImageModel> content_setting_image_model(
     ContentSettingImageModel::CreateContentSettingImageModel(
         CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->get_icon());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  net::CookieOptions options;
  content_settings->OnCookieChanged(
      GURL("http://google.com"), "A=B", options, false);
  content_setting_image_model->UpdateFromTabContents(&tab_contents);
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, Prerender) {
  TestTabContents tab_contents(profile_.get(), NULL);
  scoped_ptr<ContentSettingImageModel> content_setting_image_model(
     ContentSettingImageModel::CreateContentSettingImageModel(
         CONTENT_SETTINGS_TYPE_PRERENDER));
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());

  // Make the tab_contents prerendered
  tab_contents.set_was_prerendered(true);
  content_setting_image_model->UpdateFromTabContents(&tab_contents);
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

