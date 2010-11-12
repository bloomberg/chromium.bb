// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_bubble_model.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContentSettingBubbleModelTest : public RenderViewHostTestHarness {
 protected:
  ContentSettingBubbleModelTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()) {
  }

  void CheckGeolocationBubble(size_t expected_domains,
                              bool expect_clear_link,
                              bool expect_reload_hint) {
    scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
           contents(), profile_.get(), CONTENT_SETTINGS_TYPE_GEOLOCATION));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    EXPECT_EQ(0U, bubble_content.radio_group.radio_items.size());
    EXPECT_EQ(0U, bubble_content.popup_items.size());
    // The reload hint is currently implemented as a tacked on domain title, so
    // account for this.
    if (expect_reload_hint)
      ++expected_domains;
    EXPECT_EQ(expected_domains, bubble_content.domain_lists.size());
    if (expect_clear_link)
      EXPECT_NE(std::string(), bubble_content.clear_link);
    else
      EXPECT_EQ(std::string(), bubble_content.clear_link);
    EXPECT_NE(std::string(), bubble_content.manage_link);
    EXPECT_EQ(std::string(), bubble_content.info_link);
    EXPECT_EQ(std::string(), bubble_content.title);
    EXPECT_EQ(std::string(), bubble_content.load_plugins_link_title);
  }

  BrowserThread ui_thread_;
};

TEST_F(ContentSettingBubbleModelTest, ImageRadios) {
  TabSpecificContentSettings* content_settings =
      contents()->GetTabSpecificContentSettings();
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         contents(), profile_.get(), CONTENT_SETTINGS_TYPE_IMAGES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_NE(std::string(), bubble_content.manage_link);
  EXPECT_EQ(std::string(), bubble_content.info_link);
  EXPECT_NE(std::string(), bubble_content.title);
  EXPECT_EQ(std::string(), bubble_content.load_plugins_link_title);
}

TEST_F(ContentSettingBubbleModelTest, Cookies) {
  TabSpecificContentSettings* content_settings =
      contents()->GetTabSpecificContentSettings();
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         contents(), profile_.get(), CONTENT_SETTINGS_TYPE_COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(0U, bubble_content.radio_group.radio_items.size());
  EXPECT_NE(std::string(), bubble_content.manage_link);
  EXPECT_NE(std::string(), bubble_content.info_link);
  EXPECT_NE(std::string(), bubble_content.title);
  EXPECT_EQ(std::string(), bubble_content.load_plugins_link_title);
}

TEST_F(ContentSettingBubbleModelTest, Plugins) {
  TabSpecificContentSettings* content_settings =
      contents()->GetTabSpecificContentSettings();
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         contents(), profile_.get(), CONTENT_SETTINGS_TYPE_PLUGINS));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_NE(std::string(), bubble_content.manage_link);
  EXPECT_EQ(std::string(), bubble_content.info_link);
  EXPECT_NE(std::string(), bubble_content.title);
  EXPECT_NE(std::string(), bubble_content.load_plugins_link_title);
}

TEST_F(ContentSettingBubbleModelTest, MultiplePlugins) {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);
  cmd->AppendSwitch(switches::kEnableClickToPlay);

  HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();
  std::string fooPlugin = "foo";
  std::string barPlugin = "bar";
  GURL url = contents()->GetURL();
  map->AddExceptionForURL(url,
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          fooPlugin,
                          CONTENT_SETTING_ALLOW);
  map->AddExceptionForURL(url,
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          barPlugin,
                          CONTENT_SETTING_ASK);

  TabSpecificContentSettings* content_settings =
      contents()->GetTabSpecificContentSettings();
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     fooPlugin);
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     barPlugin);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         contents(), profile_.get(), CONTENT_SETTINGS_TYPE_PLUGINS));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(1, bubble_content.radio_group.default_item);

  content_setting_bubble_model->OnRadioClicked(0);
  // Both plug-ins should be allowed now.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   fooPlugin));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   barPlugin));

  content_setting_bubble_model->OnRadioClicked(1);
  // Both plug-ins should be click-to-play now.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   fooPlugin));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   barPlugin));
}

TEST_F(ContentSettingBubbleModelTest, Geolocation) {
  const GURL page_url("http://toplevel.example/");
  const GURL frame1_url("http://host1.example/");
  const GURL frame2_url("http://host2.example:999/");

  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      contents()->GetTabSpecificContentSettings();

  // One permitted frame, but not in the content map: requires reload.
  content_settings->OnGeolocationPermissionSet(frame1_url, true);
  CheckGeolocationBubble(1, false, true);

  // Add it to the content map, should now have a clear link.
  GeolocationContentSettingsMap* setting_map =
      profile_->GetGeolocationContentSettingsMap();
  setting_map->SetContentSetting(frame1_url, page_url, CONTENT_SETTING_ALLOW);
  CheckGeolocationBubble(1, true, false);

  // Change the default to allow: no message needed.
  setting_map->SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  CheckGeolocationBubble(1, false, false);

  // Second frame denied, but not stored in the content map: requires reload.
  content_settings->OnGeolocationPermissionSet(frame2_url, false);
  CheckGeolocationBubble(2, false, true);

  // Change the default to block: offer a clear link for the persisted frame 1.
  setting_map->SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  CheckGeolocationBubble(2, true, false);
}
