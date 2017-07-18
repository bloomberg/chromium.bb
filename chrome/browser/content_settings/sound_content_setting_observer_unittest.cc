// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/sound_content_setting_observer.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kURL1[] = "http://google.com";
constexpr char kURL2[] = "http://youtube.com";

}  // anonymous namespace

class SoundContentSettingObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  SoundContentSettingObserverTest() = default;
  ~SoundContentSettingObserverTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SoundContentSettingObserver::CreateForWebContents(web_contents());
    host_content_settings_map_ = HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

    NavigateAndCommit(GURL(kURL1));
  }

 protected:
  void ChangeSoundContentSettingTo(ContentSetting setting) {
    GURL url = web_contents()->GetLastCommittedURL();
    host_content_settings_map_->SetContentSettingDefaultScope(
        url, url, CONTENT_SETTINGS_TYPE_SOUND, std::string(), setting);
  }

  void ChangeDefaultSoundContentSettingTo(ContentSetting setting) {
    host_content_settings_map_->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_SOUND, setting);
  }

 private:
  HostContentSettingsMap* host_content_settings_map_;

  DISALLOW_COPY_AND_ASSIGN(SoundContentSettingObserverTest);
};

TEST_F(SoundContentSettingObserverTest, AudioMutingUpdatesWithContentSetting) {
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Block site.
  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Allow site.
  ChangeSoundContentSettingTo(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Set site to default.
  ChangeSoundContentSettingTo(CONTENT_SETTING_DEFAULT);
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Block by default.
  ChangeDefaultSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Should not affect site if explicitly allowed.
  ChangeSoundContentSettingTo(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Change site back to default.
  ChangeSoundContentSettingTo(CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Allow by default.
  ChangeDefaultSoundContentSettingTo(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(web_contents()->IsAudioMuted());
}

TEST_F(SoundContentSettingObserverTest, AudioMutingUpdatesWithNavigation) {
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Block for kURL1.
  ChangeSoundContentSettingTo(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(web_contents()->IsAudioMuted());

  // Should not be muted for kURL2.
  NavigateAndCommit(GURL(kURL2));
  EXPECT_FALSE(web_contents()->IsAudioMuted());

  // Should be muted for kURL1.
  NavigateAndCommit(GURL(kURL1));
  EXPECT_TRUE(web_contents()->IsAudioMuted());
}
