// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "net/base/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContentSettingImageModelTest : public TabContentsWrapperTestHarness {
 public:
  ContentSettingImageModelTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

 private:
  BrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageModelTest);
};

TEST_F(ContentSettingImageModelTest, UpdateFromTabContents) {
  TabSpecificContentSettings* content_settings =
      contents_wrapper()->content_settings();
  scoped_ptr<ContentSettingImageModel> content_setting_image_model(
     ContentSettingImageModel::CreateContentSettingImageModel(
         CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->get_icon());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES,
                                     std::string());
  content_setting_image_model->UpdateFromTabContents(contents());

  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, CookieAccessed) {
  TabSpecificContentSettings* content_settings =
      contents_wrapper()->content_settings();
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
  content_setting_image_model->UpdateFromTabContents(contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, Prerender) {
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::PRERENDER_MODE_ENABLED);
  scoped_ptr<ContentSettingImageModel> content_setting_image_model(
     ContentSettingImageModel::CreateContentSettingImageModel(
         CONTENT_SETTINGS_TYPE_PRERENDER));
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());

  // Make the tab_contents prerendered
  contents()->profile()->GetPrerenderManager()->MarkTabContentsAsPrerendered(
      contents());
  content_setting_image_model->UpdateFromTabContents(contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}
