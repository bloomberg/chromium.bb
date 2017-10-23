// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/tab_specific_content_settings.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
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

constexpr char kURL[] = "http://google.com";

}  // namespace

class TabSpecificContentSettingsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TabSpecificContentSettings::CreateForWebContents(web_contents());
  }

 protected:
  void SetSoundContentSetting(ContentSetting setting) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile);
    const GURL url = web_contents()->GetLastCommittedURL();
    map->SetContentSettingDefaultScope(url, url, CONTENT_SETTINGS_TYPE_SOUND,
                                       std::string(), setting);
  }

  void SimulateAudioStarting(TabSpecificContentSettings* content_settings) {
    content_settings->OnAudioStateChanged(true);
  }

  void SimulateAudioPlaying() {
    content::WebContentsTester::For(web_contents())
        ->SetIsCurrentlyAudible(true);
  }
};

TEST_F(TabSpecificContentSettingsTest, BlockedContent) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  net::CookieOptions options;

  // Check that after initializing, nothing is blocked.
#if !defined(OS_ANDROID)
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_SOUND));
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
#if !defined(OS_ANDROID)
  content_settings->OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES);
#endif
  content_settings->SetPopupsBlocked(true);
  TabSpecificContentSettings::MicrophoneCameraState
      blocked_microphone_camera_state =
          TabSpecificContentSettings::MICROPHONE_ACCESSED |
          TabSpecificContentSettings::MICROPHONE_BLOCKED |
          TabSpecificContentSettings::CAMERA_ACCESSED |
          TabSpecificContentSettings::CAMERA_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(GURL("http://google.com"),
                                               blocked_microphone_camera_state,
                                               std::string(),
                                               std::string(),
                                               std::string(),
                                               std::string());

  // Check that only the respective content types are affected.
#if !defined(OS_ANDROID)
  EXPECT_TRUE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_FALSE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_SOUND));
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

  // Block a javascript during a navigation.
  content_settings->OnServiceWorkerAccessed(GURL("http://google.com"),
                                            true, false);
  EXPECT_TRUE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));

  // Reset blocked content settings.
  content_settings
      ->ClearContentSettingsExceptForNavigationRelatedSettings();
#if !defined(OS_ANDROID)
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
#endif
  EXPECT_TRUE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  EXPECT_TRUE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));

  content_settings->ClearNavigationRelatedContentSettings();
#if !defined(OS_ANDROID)
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
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
  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      GURL("http://google.com"), "CookieName=CookieValue", base::Time::Now(),
      net::CookieOptions()));

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

TEST_F(TabSpecificContentSettingsTest, UnmutedAudioPlayingDoesNotBlockSound) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  NavigateAndCommit(GURL(kURL));

  // Play audio while sound content setting is allowed.
  SetSoundContentSetting(CONTENT_SETTING_ALLOW);
  SimulateAudioStarting(content_settings);
  EXPECT_FALSE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_SOUND));
}

TEST_F(TabSpecificContentSettingsTest, MutedAudioPlayingBlocksSound) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  NavigateAndCommit(GURL(kURL));

  // Play audio while sound content setting is blocked.
  SetSoundContentSetting(CONTENT_SETTING_BLOCK);
  SimulateAudioStarting(content_settings);
  EXPECT_TRUE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_SOUND));
}

TEST_F(TabSpecificContentSettingsTest,
       MutingAudioWhileSoundIsPlayingBlocksSound) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  NavigateAndCommit(GURL(kURL));

  // Unmuted audio starts playing.
  SimulateAudioPlaying();
  // Sound is not blocked.
  EXPECT_FALSE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_SOUND));
  // User mutes the site.
  SetSoundContentSetting(CONTENT_SETTING_BLOCK);
  // Sound is blocked.
  EXPECT_TRUE(content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_SOUND));
}
