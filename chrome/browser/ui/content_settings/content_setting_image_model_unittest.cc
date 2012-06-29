// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "net/cookies/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class ContentSettingImageModelTest : public TabContentsTestHarness {
 public:
  ContentSettingImageModelTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

 private:
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageModelTest);
};

TEST_F(ContentSettingImageModelTest, UpdateFromWebContents) {
  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();
  scoped_ptr<ContentSettingImageModel> content_setting_image_model(
     ContentSettingImageModel::CreateContentSettingImageModel(
         CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->get_icon());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES,
                                     std::string());
  content_setting_image_model->UpdateFromWebContents(contents());

  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}

TEST_F(ContentSettingImageModelTest, CookieAccessed) {
  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();
  profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  scoped_ptr<ContentSettingImageModel> content_setting_image_model(
     ContentSettingImageModel::CreateContentSettingImageModel(
         CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(content_setting_image_model->is_visible());
  EXPECT_EQ(0, content_setting_image_model->get_icon());
  EXPECT_TRUE(content_setting_image_model->get_tooltip().empty());

  net::CookieOptions options;
  content_settings->OnCookieChanged(GURL("http://google.com"),
                                    GURL("http://google.com"),
                                    "A=B",
                                    options,
                                    false);
  content_setting_image_model->UpdateFromWebContents(contents());
  EXPECT_TRUE(content_setting_image_model->is_visible());
  EXPECT_NE(0, content_setting_image_model->get_icon());
  EXPECT_FALSE(content_setting_image_model->get_tooltip().empty());
}
