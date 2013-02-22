// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::WebContentsTester;

class ContentSettingBubbleModelTest : public ChromeRenderViewHostTestHarness {
 protected:
  ContentSettingBubbleModelTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()) {
  }

  void StartIOThread() {
    io_thread_.reset(new content::TestBrowserThread(BrowserThread::IO));
    io_thread_->StartIOThread();
  }

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    TabSpecificContentSettings::CreateForWebContents(web_contents());
    InfoBarService::CreateForWebContents(web_contents());
  }

  virtual void TearDown() OVERRIDE {
    // This will delete the TestingProfile on the UI thread.
    ChromeRenderViewHostTestHarness::TearDown();

    // Finish off deleting the ProtocolHandlerRegistry, which must be done on
    // the IO thread.
    if (io_thread_.get()) {
      io_thread_->Stop();
      io_thread_.reset(NULL);
    }
  }

  void CheckGeolocationBubble(size_t expected_domains,
                              bool expect_clear_link,
                              bool expect_reload_hint) {
    scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
            NULL, web_contents(), profile(),
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
  scoped_ptr<content::TestBrowserThread> io_thread_;
};

TEST_F(ContentSettingBubbleModelTest, ImageRadios) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, web_contents(), profile(),
         CONTENT_SETTINGS_TYPE_IMAGES));
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
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, web_contents(), profile(),
         CONTENT_SETTINGS_TYPE_COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_FALSE(bubble_content.radio_group.radio_items.empty());
  EXPECT_FALSE(bubble_content.custom_link.empty());
  EXPECT_TRUE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());
}

TEST_F(ContentSettingBubbleModelTest, Mediastream) {
  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, web_contents(), profile(),
         CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_FALSE(bubble_content.title.empty());
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());
  EXPECT_EQ(2U, bubble_content.media_menus.size());
}

TEST_F(ContentSettingBubbleModelTest, BlockedMediastream) {
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("https://www.example.com"));
  GURL url = web_contents()->GetURL();

  HostContentSettingsMap* host_content_settings_map =
      profile()->GetHostContentSettingsMap();
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURL(url);
  ContentSetting setting = CONTENT_SETTING_BLOCK;
  host_content_settings_map->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        std::string(),
        setting);
  host_content_settings_map->SetContentSetting(
        primary_pattern,
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
        std::string(),
        setting);

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_MEDIASTREAM,
                                     std::string());
  {
    scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
           NULL, web_contents(), profile(),
           CONTENT_SETTINGS_TYPE_MEDIASTREAM));
    const ContentSettingBubbleModel::BubbleContent& bubble_content =
        content_setting_bubble_model->bubble_content();
    // Test if the correct radio item is selected for the blocked mediastream
    // setting.
    EXPECT_EQ(1, bubble_content.radio_group.default_item);
  }

  // Test that the media settings where not changed.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                std::string()));

  {
    scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
        ContentSettingBubbleModel::CreateContentSettingBubbleModel(
           NULL, web_contents(), profile(),
           CONTENT_SETTINGS_TYPE_MEDIASTREAM));
    // Change the radio setting.
    content_setting_bubble_model->OnRadioClicked(0);
  }
  // Test that the media setting were change correctly.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                url,
                url,
                CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                std::string()));

  // Removing an |InfoBarDelegate| from the |InfoBarService| does not delete
  // it. Hence the |delegate| must be cleaned up after it was removed from the
  // |infobar_service|.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  scoped_ptr<InfoBarDelegate> delegate(
      infobar_service->GetInfoBarDelegateAt(0));
  infobar_service->RemoveInfoBar(delegate.get());
}

TEST_F(ContentSettingBubbleModelTest, Plugins) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, web_contents(), profile(),
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
  base::AutoReset<CommandLine> auto_reset(cmd, *cmd);
  cmd->AppendSwitch(switches::kEnableResourceContentSettings);

  HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();
  std::string fooPlugin = "foo";
  std::string barPlugin = "bar";

  // Navigating to some sample url prevents the GetURL method from returning an
  // invalid empty URL.
  WebContentsTester::For(web_contents())->
      NavigateAndCommit(GURL("http://www.example.com"));
  GURL url = web_contents()->GetURL();
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
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     fooPlugin);
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS,
                                     barPlugin);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_PLUGINS));
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

TEST_F(ContentSettingBubbleModelTest, PepperBroker) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
                                     std::string());

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, web_contents(), profile(),
         CONTENT_SETTINGS_TYPE_PPAPI_BROKER));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();

  std::string title = bubble_content.title;
  EXPECT_FALSE(title.empty());
  ASSERT_EQ(2U, bubble_content.radio_group.radio_items.size());
  std::string radio1 = bubble_content.radio_group.radio_items[0];
  std::string radio2 = bubble_content.radio_group.radio_items[1];
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());

  content_settings->ClearBlockedContentSettingsExceptForCookies();
  content_settings->OnContentAllowed(CONTENT_SETTINGS_TYPE_PPAPI_BROKER);
  content_setting_bubble_model.reset(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_PPAPI_BROKER));
  const ContentSettingBubbleModel::BubbleContent& bubble_content_2 =
      content_setting_bubble_model->bubble_content();

  EXPECT_FALSE(bubble_content_2.title.empty());
  EXPECT_NE(title, bubble_content_2.title);
  ASSERT_EQ(2U, bubble_content_2.radio_group.radio_items.size());
  EXPECT_NE(radio1, bubble_content_2.radio_group.radio_items[0]);
  EXPECT_NE(radio2, bubble_content_2.radio_group.radio_items[1]);
  EXPECT_FALSE(bubble_content_2.custom_link_enabled);
  EXPECT_FALSE(bubble_content_2.manage_link.empty());
}

TEST_F(ContentSettingBubbleModelTest, Geolocation) {
  const GURL page_url("http://toplevel.example/");
  const GURL frame1_url("http://host1.example/");
  const GURL frame2_url("http://host2.example:999/");

  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

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
  TabSpecificContentSettings::FromWebContents(web_contents())->OnContentBlocked(
      CONTENT_SETTINGS_TYPE_IMAGES, std::string());
  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_IMAGES));
  std::string title =
      content_setting_bubble_model->bubble_content().radio_group.radio_items[0];
  ASSERT_NE(std::string::npos, title.find(file_url));
}

TEST_F(ContentSettingBubbleModelTest, RegisterProtocolHandler) {
  const GURL page_url("http://toplevel.example/");
  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->set_pending_protocol_handler(
      ProtocolHandler::CreateProtocolHandler("mailto",
          GURL("http://www.toplevel.example/"), ASCIIToUTF16("Handler")));

  ContentSettingRPHBubbleModel content_setting_bubble_model(
          NULL, web_contents(), profile(), NULL,
          CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS);

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model.bubble_content();
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
  virtual void RegisterExternalHandler(const std::string& protocol) OVERRIDE {
    // Overrides in order to not register the handler with the
    // ChildProcessSecurityPolicy. That has persistent and unalterable
    // side effects on other tests.
  }

  virtual ShellIntegration::DefaultProtocolClientWorker* CreateShellWorker(
      ShellIntegration::DefaultWebClientObserver* observer,
      const std::string& protocol) OVERRIDE {
    LOG(INFO) << "CreateShellWorker";
    return NULL;
  }

  virtual ProtocolHandlerRegistry::DefaultClientObserver* CreateShellObserver(
      ProtocolHandlerRegistry* registry) OVERRIDE {
    return NULL;
  }

  virtual void RegisterWithOSAsDefaultClient(
      const std::string& protocol,
      ProtocolHandlerRegistry* registry) OVERRIDE {
    LOG(INFO) << "Register With OS";
  }
};

TEST_F(ContentSettingBubbleModelTest, RPHAllow) {
  StartIOThread();

  ProtocolHandlerRegistry registry(profile(), new FakeDelegate());
  registry.InitProtocolSettings();

  const GURL page_url("http://toplevel.example/");
  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  ProtocolHandler test_handler = ProtocolHandler::CreateProtocolHandler(
      "mailto", GURL("http://www.toplevel.example/"),
      ASCIIToUTF16("Handler"));
  content_settings->set_pending_protocol_handler(test_handler);

  ContentSettingRPHBubbleModel content_setting_bubble_model(
          NULL, web_contents(), profile(), &registry,
          CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS);

  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_DEFAULT,
              content_settings->pending_protocol_handler_setting());
  }

  // "0" is the "Allow" radio button.
  content_setting_bubble_model.OnRadioClicked(0);
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    ASSERT_FALSE(handler.IsEmpty());
    EXPECT_EQ(ASCIIToUTF16("Handler"), handler.title());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              content_settings->pending_protocol_handler_setting());
  }

  // "1" is the "Deny" radio button.
  content_setting_bubble_model.OnRadioClicked(1);
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              content_settings->pending_protocol_handler_setting());
  }

  // "2" is the "Ignore button.
  content_setting_bubble_model.OnRadioClicked(2);
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    EXPECT_TRUE(handler.IsEmpty());
    EXPECT_EQ(CONTENT_SETTING_DEFAULT,
              content_settings->pending_protocol_handler_setting());
    EXPECT_TRUE(registry.IsIgnored(test_handler));
  }

  // "0" is the "Allow" radio button.
  content_setting_bubble_model.OnRadioClicked(0);
  {
    ProtocolHandler handler = registry.GetHandlerFor("mailto");
    ASSERT_FALSE(handler.IsEmpty());
    EXPECT_EQ(ASCIIToUTF16("Handler"), handler.title());
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              content_settings->pending_protocol_handler_setting());
    EXPECT_FALSE(registry.IsIgnored(test_handler));
  }

  registry.Shutdown();
}
