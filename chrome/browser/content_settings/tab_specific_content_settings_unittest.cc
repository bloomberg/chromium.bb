// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/parsed_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

class MockSiteDataObserver
    : public TabSpecificContentSettings::SiteDataObserver {
 public:
  explicit MockSiteDataObserver(
      TabSpecificContentSettings* tab_specific_content_settings)
      : SiteDataObserver(tab_specific_content_settings) {
  }

  virtual ~MockSiteDataObserver() {}

  MOCK_METHOD0(OnSiteDataAccessed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSiteDataObserver);
};

}  // namespace

class TabSpecificContentSettingsTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    TabSpecificContentSettings::CreateForWebContents(web_contents());
  }
};

TEST_F(TabSpecificContentSettingsTest, BlockedContent) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  net::CookieOptions options;

  // Check that after initializing, nothing is blocked.
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));

  // Set a cookie, block access to images, block mediastream access and block a
  // popup.
  content_settings->OnCookieChanged(GURL("http://google.com"),
                                    GURL("http://google.com"),
                                    "A=B",
                                    options,
                                    false);
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES);
  content_settings->SetPopupsBlocked(true);
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  // Check that only the respective content types are affected.
  EXPECT_TRUE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_TRUE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
  EXPECT_TRUE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  EXPECT_TRUE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  content_settings->OnCookieChanged(GURL("http://google.com"),
                                    GURL("http://google.com"),
                                    "A=B",
                                    options,
                                    false);

  // Block a cookie.
  content_settings->OnCookieChanged(GURL("http://google.com"),
                                    GURL("http://google.com"),
                                    "C=D",
                                    options,
                                    true);
  EXPECT_TRUE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));

  // Reset blocked content settings.
  content_settings->ClearBlockedContentSettingsExceptForCookies();
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_TRUE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));

  content_settings->ClearCookieSpecificContentSettings();
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
}

TEST_F(TabSpecificContentSettingsTest, BlockedFileSystems) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  // Access a file system.
  content_settings->OnFileSystemAccessed(GURL("http://google.com"), false);
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));

  // Block access to a file system.
  content_settings->OnFileSystemAccessed(GURL("http://google.com"), true);
  EXPECT_TRUE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, AllowedContent) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  net::CookieOptions options;

  // Test default settings.
  ASSERT_FALSE(
      content_settings->IsContentAllowed(CONTENT_SETTINGS_TYPE_IMAGES));
  ASSERT_FALSE(
      content_settings->IsContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));

  // Record a cookie.
  content_settings->OnCookieChanged(GURL("http://google.com"),
                                    GURL("http://google.com"),
                                    "A=B",
                                    options,
                                    false);
  ASSERT_TRUE(
      content_settings->IsContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));

  // Record a blocked cookie.
  content_settings->OnCookieChanged(GURL("http://google.com"),
                                    GURL("http://google.com"),
                                    "C=D",
                                    options,
                                    true);
  ASSERT_TRUE(
      content_settings->IsContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_TRUE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, AllowedBlockedMediaContent) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  ASSERT_EQ(TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED,
            content_settings->GetMicrophoneCameraState());

  // Request and allow microphone access.
  GURL security_origin("http://google.com");
  MediaStreamDevicesController::MediaStreamTypeSettingsMap
      request_permissions;
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  ASSERT_TRUE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED,
            content_settings->GetMicrophoneCameraState());

  // Request and allow camera access.
  request_permissions.clear();
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  ASSERT_TRUE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_EQ(TabSpecificContentSettings::MICROPHONE_CAMERA_ACCESSED,
            content_settings->GetMicrophoneCameraState());

  // Request and block microphone access.
  request_permissions.clear();
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  ASSERT_FALSE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_TRUE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_EQ(TabSpecificContentSettings::CAMERA_ACCESSED,
            content_settings->GetMicrophoneCameraState());

  // Request and block camera access.
  request_permissions.clear();
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  ASSERT_FALSE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_TRUE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_EQ(TabSpecificContentSettings::MICROPHONE_CAMERA_BLOCKED,
            content_settings->GetMicrophoneCameraState());

  // Request and allow microphone and camera access.
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  ASSERT_TRUE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_TRUE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_EQ(TabSpecificContentSettings::MICROPHONE_CAMERA_ACCESSED,
            content_settings->GetMicrophoneCameraState());

  // Request and block microphone and camera access.
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  ASSERT_FALSE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_TRUE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_TRUE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_EQ(TabSpecificContentSettings::MICROPHONE_CAMERA_BLOCKED,
            content_settings->GetMicrophoneCameraState());

  // Request microphone and camera access. Allow microphone, block camera.
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  ASSERT_TRUE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_TRUE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_EQ(TabSpecificContentSettings::MICROPHONE_ACCESSED,
            content_settings->GetMicrophoneCameraState());

  // Request microphone and camera access. Block microphone, allow camera.
  request_permissions[content::MEDIA_DEVICE_AUDIO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_BLOCKED_BY_USER;
  request_permissions[content::MEDIA_DEVICE_VIDEO_CAPTURE].permission =
      MediaStreamDevicesController::MEDIA_ALLOWED;
  content_settings->OnMediaStreamPermissionSet(security_origin,
                                               request_permissions);
  ASSERT_FALSE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_TRUE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  ASSERT_TRUE(content_settings->IsContentAllowed(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
  ASSERT_EQ(TabSpecificContentSettings::CAMERA_ACCESSED,
            content_settings->GetMicrophoneCameraState());
}

TEST_F(TabSpecificContentSettingsTest, EmptyCookieList) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  ASSERT_FALSE(
      content_settings->IsContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  content_settings->OnCookiesRead(GURL("http://google.com"),
                                  GURL("http://google.com"),
                                  net::CookieList(),
                                  true);
  ASSERT_FALSE(
      content_settings->IsContentAllowed(CONTENT_SETTINGS_TYPE_COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, SiteDataObserver) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  MockSiteDataObserver mock_observer(content_settings);
  EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(6);

  bool blocked_by_policy = false;
  content_settings->OnCookieChanged(GURL("http://google.com"),
                                    GURL("http://google.com"),
                                    "A=B",
                                    net::CookieOptions(),
                                    blocked_by_policy);
  net::CookieList cookie_list;
  scoped_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::Create(GURL("http://google.com"),
                                   "CookieName=CookieValue",
                                   base::Time::Now(), net::CookieOptions()));

  cookie_list.push_back(*cookie);
  content_settings->OnCookiesRead(GURL("http://google.com"),
                                  GURL("http://google.com"),
                                  cookie_list,
                                  blocked_by_policy);
  content_settings->OnFileSystemAccessed(GURL("http://google.com"),
                                              blocked_by_policy);
  content_settings->OnIndexedDBAccessed(GURL("http://google.com"),
                                        base::UTF8ToUTF16("text"),
                                        blocked_by_policy);
  content_settings->OnLocalStorageAccessed(GURL("http://google.com"),
                                           true,
                                           blocked_by_policy);
  content_settings->OnWebDatabaseAccessed(GURL("http://google.com"),
                                          base::UTF8ToUTF16("name"),
                                          base::UTF8ToUTF16("display_name"),
                                          blocked_by_policy);
}
