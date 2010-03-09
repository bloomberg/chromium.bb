// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_bubble_model.h"

#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef RenderViewHostTestHarness ContentSettingBubbleModelTest;

TEST_F(ContentSettingBubbleModelTest, ImageRadios) {
  TestTabContents tab_contents(profile_.get(), NULL);
  RenderViewHostDelegate::Resource* render_view_host_delegate = &tab_contents;
  render_view_host_delegate->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         &tab_contents, profile_.get(), CONTENT_SETTINGS_TYPE_IMAGES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(1U, bubble_content.radio_groups.size());
  EXPECT_EQ(2U, bubble_content.radio_groups[0].radio_items.size());
  EXPECT_EQ(0, bubble_content.radio_groups[0].default_item);
  EXPECT_NE(std::string(), bubble_content.manage_link);
  EXPECT_NE(std::string(), bubble_content.title);
}

TEST_F(ContentSettingBubbleModelTest, Cookies) {
  TestTabContents tab_contents(profile_.get(), NULL);
  RenderViewHostDelegate::Resource* render_view_host_delegate = &tab_contents;
  render_view_host_delegate->OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         &tab_contents, profile_.get(), CONTENT_SETTINGS_TYPE_COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(0U, bubble_content.radio_groups.size());
  EXPECT_NE(std::string(), bubble_content.manage_link);
  EXPECT_NE(std::string(), bubble_content.title);
}
