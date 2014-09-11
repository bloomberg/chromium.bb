// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContentsTester;

class ContentSettingBubbleModelTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    TabSpecificContentSettings::CreateForWebContents(web_contents());
    InfoBarService::CreateForWebContents(web_contents());
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
};

TEST_F(ContentSettingBubbleModelTest, ImageRadios) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES);

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
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(), CONTENT_SETTINGS_TYPE_COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  std::string title = bubble_content.title;
  EXPECT_FALSE(title.empty());
  ASSERT_EQ(2U, bubble_content.radio_group.radio_items.size());
  std::string radio1 = bubble_content.radio_group.radio_items[0];
  std::string radio2 = bubble_content.radio_group.radio_items[1];
  EXPECT_FALSE(bubble_content.custom_link.empty());
  EXPECT_TRUE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());

  content_settings->ClearCookieSpecificContentSettings();
  content_settings->OnContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES);
  content_setting_bubble_model.reset(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(), CONTENT_SETTINGS_TYPE_COOKIES));
  const ContentSettingBubbleModel::BubbleContent& bubble_content_2 =
      content_setting_bubble_model->bubble_content();

  EXPECT_FALSE(bubble_content_2.title.empty());
  EXPECT_NE(title, bubble_content_2.title);
  ASSERT_EQ(2U, bubble_content_2.radio_group.radio_items.size());
  // TODO(bauerb): Update this once the strings have been updated.
  EXPECT_EQ(radio1, bubble_content_2.radio_group.radio_items[0]);
  EXPECT_EQ(radio2, bubble_content_2.radio_group.radio_items[1]);
  EXPECT_FALSE(bubble_content_2.custom_link.empty());
  EXPECT_TRUE(bubble_content_2.custom_link_enabled);
  EXPECT_FALSE(bubble_content_2.manage_link.empty());
}

TEST_F(ContentSettingBubbleModelTest, MediastreamMicAndCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  MediaStreamDevicesController::MediaStreamTypeSettingsMap
      request_permissions;
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
         NULL, web_contents(), profile(),
         CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF8(IDS_MICROPHONE_CAMERA_ALLOWED));
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF8(
                IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION,
                base::UTF8ToUTF16(request_host)));
  EXPECT_EQ(bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF8(
                IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_BLOCK));
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());
  EXPECT_EQ(2U, bubble_content.media_menus.size());
}

TEST_F(ContentSettingBubbleModelTest, BlockedMediastreamMicAndCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

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
  MediaStreamDevicesController::MediaStreamTypeSettingsMap
      request_permissions;
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  content_settings->OnMediaStreamPermissionSet(url, request_permissions);
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

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  infobar_service->RemoveInfoBar(infobar_service->infobar_at(0));
}

TEST_F(ContentSettingBubbleModelTest, MediastreamMic) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  MediaStreamDevicesController::MediaStreamTypeSettingsMap
      request_permissions;
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF8(IDS_MICROPHONE_ACCESSED));
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF8(
                IDS_ALLOWED_MEDIASTREAM_MIC_NO_ACTION,
                base::UTF8ToUTF16(request_host)));
  EXPECT_EQ(bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF8(
                IDS_ALLOWED_MEDIASTREAM_MIC_BLOCK));
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());
  EXPECT_EQ(1U, bubble_content.media_menus.size());
  EXPECT_EQ(content::MEDIA_DEVICE_AUDIO_CAPTURE,
            bubble_content.media_menus.begin()->first);

  // Change the microphone access.
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  content_setting_bubble_model.reset(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  const ContentSettingBubbleModel::BubbleContent& new_bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(new_bubble_content.title,
            l10n_util::GetStringUTF8(IDS_MICROPHONE_BLOCKED));
  EXPECT_EQ(2U, new_bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF8(
                IDS_BLOCKED_MEDIASTREAM_MIC_ASK,
                base::UTF8ToUTF16(request_host)));
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF8(
                IDS_BLOCKED_MEDIASTREAM_MIC_NO_ACTION));
  EXPECT_EQ(1, new_bubble_content.radio_group.default_item);
  EXPECT_TRUE(new_bubble_content.custom_link.empty());
  EXPECT_FALSE(new_bubble_content.custom_link_enabled);
  EXPECT_FALSE(new_bubble_content.manage_link.empty());
  EXPECT_EQ(1U, new_bubble_content.media_menus.size());
  EXPECT_EQ(content::MEDIA_DEVICE_AUDIO_CAPTURE,
            new_bubble_content.media_menus.begin()->first);
}

TEST_F(ContentSettingBubbleModelTest, MediastreamCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);
  MediaStreamDevicesController::MediaStreamTypeSettingsMap
      request_permissions;
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF8(IDS_CAMERA_ACCESSED));
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF8(
                IDS_ALLOWED_MEDIASTREAM_CAMERA_NO_ACTION,
                base::UTF8ToUTF16(request_host)));
  EXPECT_EQ(bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF8(
                IDS_ALLOWED_MEDIASTREAM_CAMERA_BLOCK));
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_TRUE(bubble_content.custom_link.empty());
  EXPECT_FALSE(bubble_content.custom_link_enabled);
  EXPECT_FALSE(bubble_content.manage_link.empty());
  EXPECT_EQ(1U, bubble_content.media_menus.size());
  EXPECT_EQ(content::MEDIA_DEVICE_VIDEO_CAPTURE,
            bubble_content.media_menus.begin()->first);

  // Change the camera access.
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  content_setting_bubble_model.reset(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  const ContentSettingBubbleModel::BubbleContent& new_bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(new_bubble_content.title,
            l10n_util::GetStringUTF8(IDS_CAMERA_BLOCKED));
  EXPECT_EQ(2U, new_bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF8(
                IDS_BLOCKED_MEDIASTREAM_CAMERA_ASK,
                base::UTF8ToUTF16(request_host)));
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF8(
                IDS_BLOCKED_MEDIASTREAM_CAMERA_NO_ACTION));
  EXPECT_EQ(1, new_bubble_content.radio_group.default_item);
  EXPECT_TRUE(new_bubble_content.custom_link.empty());
  EXPECT_FALSE(new_bubble_content.custom_link_enabled);
  EXPECT_FALSE(new_bubble_content.manage_link.empty());
  EXPECT_EQ(1U, new_bubble_content.media_menus.size());
  EXPECT_EQ(content::MEDIA_DEVICE_VIDEO_CAPTURE,
            new_bubble_content.media_menus.begin()->first);
}

TEST_F(ContentSettingBubbleModelTest, AccumulateMediastreamMicAndCamera) {
  // Required to break dependency on BrowserMainLoop.
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  std::string request_host = "google.com";
  GURL security_origin("http://" + request_host);

  // Firstly, add microphone access.
  MediaStreamDevicesController::MediaStreamTypeSettingsMap
      request_permissions;
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);

  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(bubble_content.title,
            l10n_util::GetStringUTF8(IDS_MICROPHONE_ACCESSED));
  EXPECT_EQ(2U, bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF8(
                IDS_ALLOWED_MEDIASTREAM_MIC_NO_ACTION,
                base::UTF8ToUTF16(request_host)));
  EXPECT_EQ(bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF8(
                IDS_ALLOWED_MEDIASTREAM_MIC_BLOCK));
  EXPECT_EQ(0, bubble_content.radio_group.default_item);
  EXPECT_EQ(1U, bubble_content.media_menus.size());
  EXPECT_EQ(content::MEDIA_DEVICE_AUDIO_CAPTURE,
            bubble_content.media_menus.begin()->first);

  // Then add camera access.
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);

  content_setting_bubble_model.reset(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          NULL, web_contents(), profile(),
          CONTENT_SETTINGS_TYPE_MEDIASTREAM));
  const ContentSettingBubbleModel::BubbleContent& new_bubble_content =
      content_setting_bubble_model->bubble_content();
  EXPECT_EQ(new_bubble_content.title,
            l10n_util::GetStringUTF8(IDS_MICROPHONE_CAMERA_ALLOWED));
  EXPECT_EQ(2U, new_bubble_content.radio_group.radio_items.size());
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[0],
            l10n_util::GetStringFUTF8(
                IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION,
                base::UTF8ToUTF16(request_host)));
  EXPECT_EQ(new_bubble_content.radio_group.radio_items[1],
            l10n_util::GetStringUTF8(
                IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_BLOCK));
  EXPECT_EQ(0, new_bubble_content.radio_group.default_item);
  EXPECT_EQ(2U, new_bubble_content.media_menus.size());
}

TEST_F(ContentSettingBubbleModelTest, Plugins) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS);

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
  EXPECT_FALSE(bubble_content.learn_more_link.empty());
}

TEST_F(ContentSettingBubbleModelTest, PepperBroker) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_PPAPI_BROKER);

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
      CONTENT_SETTINGS_TYPE_IMAGES);
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
      ProtocolHandler::CreateProtocolHandler(
          "mailto", GURL("http://www.toplevel.example/")));

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
    VLOG(1) << "CreateShellWorker";
    return NULL;
  }

  virtual ProtocolHandlerRegistry::DefaultClientObserver* CreateShellObserver(
      ProtocolHandlerRegistry* registry) OVERRIDE {
    return NULL;
  }

  virtual void RegisterWithOSAsDefaultClient(
      const std::string& protocol,
      ProtocolHandlerRegistry* registry) OVERRIDE {
    VLOG(1) << "Register With OS";
  }
};

TEST_F(ContentSettingBubbleModelTest, RPHAllow) {
  ProtocolHandlerRegistry registry(profile(), new FakeDelegate());
  registry.InitProtocolSettings();

  const GURL page_url("http://toplevel.example/");
  NavigateAndCommit(page_url);
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  ProtocolHandler test_handler = ProtocolHandler::CreateProtocolHandler(
      "mailto", GURL("http://www.toplevel.example/"));
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
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              content_settings->pending_protocol_handler_setting());
    EXPECT_FALSE(registry.IsIgnored(test_handler));
  }

  registry.Shutdown();
}
