// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::WebContentsTester;

class ContentSettingBubbleModelTest : public TabContentsTestHarness {
 protected:
  ContentSettingBubbleModelTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()) {
  }

  void CheckGeolocationBubble(size_t expected_domains,
                              bool expect_clear_link,
                              bool expect_reload_hint) {
    scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, tab_contents(), profile(),
            CONTENT_SETTINGS_TYPE_GEOLOCATION));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    EXPECT_TRUE(bubble_content.title.empty());
    EXPECT_TRUE(bubble_content.radio_group.radio_items.empty());
    EXPECT_TRUE(bubble_content.popup_items.empty());
    EXPECT_EQ(expected_domains, bubble_content.domain_lists.size());
    EXPECT_NE(expect_clear_link || expect_reload_hint,
              bubble_content.custom_link.empty());
    EXPECT_EQ(expect_clear_link, bubble_content.custom_link_enabled);
    EXPECT_FALSE(bubble_content.manage_link.empty());
  }

  content::TestBrowserThread ui_thread_;
};

TEST_F(ContentSettingBubbleModelTest, ImageRadios) {
  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, tab_contents(), profile(), CONTENT_SETTINGS_TYPE_IMAGES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.manage_link.empty());
}

TEST_F(ContentSettingBubbleModelTest, Cookies) {
  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, tab_contents(), profile(), CONTENT_SETTINGS_TYPE_COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_FALSE(bubble_content.radio_group.radio_items.empty());
  EXPECT_FALSE(bubble_content.custom_link.empty());
  EXPECT_TRUE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());
}

TEST_F(ContentSettingBubbleModelTest, Plugins) {
  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, tab_contents(), profile(),
         CONTENT_SETTINGS_TYPE_PLUGINS));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_FALSE(bubble_content.custom_link.empty());
  EXPECT_TRUE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());
}

TEST_F(ContentSettingBubbleModelTest, MultiplePlugins) {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();
  std::string fooPlugin = "foo";
  std::string barPlugin = "bar";

  // Navigating to some sample url prevents the GetURL method from returning an
  // invalid empty URL.
  WebContentsTester::For(
      contents())->NavigateAndCommit(GURL("http://www.example.com"));
  GURL url = contents()->GetURL();
  map->AddExceptionForURL(url,
                          url,
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          fooPlugin,
                          CONTENT_SETTING_ALLOW);
  map->AddExceptionForURL(url,
                          url,
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          barPlugin,
                          CONTENT_SETTING_ASK);

  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     fooPlugin);
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     barPlugin);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, tab_contents(), profile(), CONTENT_SETTINGS_TYPE_PLUGINS));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(1, bubble_content.radio_group.default_item);

  content_setting_bubble_model->OnRadioClicked(0);
  // Nothing should have changed.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url,
                                   url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   fooPlugin));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url,
                                   url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   barPlugin));

  content_setting_bubble_model.reset();
  // Both plug-ins should be click-to-play now.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url,
                                   url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   fooPlugin));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url,
                                   url,
                                   CONTENT_SETTINGS_TYPE_PLUGINS,
                                   barPlugin));
}

TEST_F(ContentSettingBubbleModelTest, Geolocation) {
  const GURL page_url("http://toplevel.example/");
  const GURL frame1_url("http://host1.example/");
  const GURL frame2_url("http://host2.example:999/");

  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();

  // One permitted frame, but not in the content map: requires reload.
  content_settings->OnGeolocationPermissionSet(frame1_url, true);
  CheckGeolocationBubble(1, false, true);

  // Add it to the content map, should now have a clear link.
  HostContentSettingsMap* setting_map =
      profile()->GetHostContentSettingsMap();
  setting_map->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(frame1_url),
      ContentSettingsPattern::FromURLNoWildcard(page_url),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);
  CheckGeolocationBubble(1, true, false);

  // Change the default to allow: no message needed.
  profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckGeolocationBubble(1, false, false);

  // Second frame denied, but not stored in the content map: requires reload.
  content_settings->OnGeolocationPermissionSet(frame2_url, false);
  CheckGeolocationBubble(2, false, true);

  // Change the default to block: offer a clear link for the persisted frame 1.
  profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_BLOCK);
  CheckGeolocationBubble(2, true, false);
}

TEST_F(ContentSettingBubbleModelTest, FileURL) {
  std::string file_url("file:///tmp/test.html");
  NavigateAndCommit(GURL(file_url));
  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, tab_contents(), profile(),
          CONTENT_SETTINGS_TYPE_IMAGES));
  std::string title =
      content_setting_bubble_model->bubble_content().radio_group.radio_items[0];
  ASSERT_NE(std::string::npos, title.find(file_url));
}

TEST_F(ContentSettingBubbleModelTest, RegisterProtocolHandler) {
  const GURL page_url("http://toplevel.example/");
  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();
  content_settings->set_pending_protocol_handler(
      ProtocolHandler::CreateProtocolHandler("mailto",
          GURL("http://www.toplevel.example/"), ASCIIToUTF16("Handler")));

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, tab_contents(), profile(),
          CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_FALSE(bubble_content.radio_group.radio_items.empty());
  EXPECT_TRUE(bubble_content.popup_items.empty());
  EXPECT_TRUE(bubble_content.domain_lists.empty());
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());
}

class FakeDelegate : public ProtocolHandlerRegistry::Delegate {
 public:
  virtual void RegisterExternalHandler(const std::string& protocol) {
    // Overrides in order to not register the handler with the
    // ChildProcessSecurityPolicy. That has persistent and unalterable
    // side effects on other tests.
  }
};

TEST_F(ContentSettingBubbleModelTest, RPHAllow) {
  FakeDelegate delegate;
  profile()->CreateProtocolHandlerRegistry(&delegate);

  const GURL page_url("http://toplevel.example/");
  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      tab_contents()->content_settings();
  ProtocolHandler test_handler = ProtocolHandler::CreateProtocolHandler(
      "mailto", GURL("http://www.toplevel.example/"),
      ASCIIToUTF16("Handler"));
  content_settings->set_pending_protocol_handler(test_handler);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, tab_contents(), profile(),
          CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS));

  {
    ProtocolHandler handler =
        profile()->GetProtocolHandlerRegistry()->GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_DEFAULT,
              content_settings->pending_protocol_handler_setting());
  }

  // "0" is the "Allow" radio button.
  content_setting_bubble_model->OnRadioClicked(0);
  {
    ProtocolHandler handler =
        profile()->GetProtocolHandlerRegistry()->GetHandlerFor("mailto");
    ASSERT_FALSE(handler.IsEmpty());
    EXPECT_EQ(ASCIIToUTF16("Handler"), handler.title());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              content_settings->pending_protocol_handler_setting());
  }

  // "1" is the "Deny" radio button.
  content_setting_bubble_model->OnRadioClicked(1);
  {
    ProtocolHandler handler =
        profile()->GetProtocolHandlerRegistry()->GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              content_settings->pending_protocol_handler_setting());
  }

  // "2" is the "Ignore button.
  content_setting_bubble_model->OnRadioClicked(2);
  {
    ProtocolHandler handler =
        profile()->GetProtocolHandlerRegistry()->GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_DEFAULT,
              content_settings->pending_protocol_handler_setting());
    EXPECT_TRUE(profile()->GetProtocolHandlerRegistry()->IsIgnored(
        test_handler));
  }

  // "0" is the "Allow" radio button.
  content_setting_bubble_model->OnRadioClicked(0);
  {
    ProtocolHandler handler =
        profile()->GetProtocolHandlerRegistry()->GetHandlerFor("mailto");
    ASSERT_FALSE(handler.IsEmpty());
    EXPECT_EQ(ASCIIToUTF16("Handler"), handler.title());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              content_settings->pending_protocol_handler_setting());
    EXPECT_FALSE(profile()->GetProtocolHandlerRegistry()->IsIgnored(
        test_handler));
  }
}
