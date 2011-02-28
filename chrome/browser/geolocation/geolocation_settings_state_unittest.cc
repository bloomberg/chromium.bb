// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_settings_state.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class GeolocationSettingsStateTests : public testing::Test {
 public:
   GeolocationSettingsStateTests()
    : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(GeolocationSettingsStateTests, ClearOnNewOrigin) {
  TestingProfile profile;
  GeolocationSettingsState state(&profile);
  GURL url_0("http://www.example.com");

  NavigationEntry entry;
  entry.set_url(url_0);
  NavigationController::LoadCommittedDetails load_committed_details;
  load_committed_details.entry = &entry;
  state.DidNavigate(load_committed_details);

  profile.GetGeolocationContentSettingsMap()->SetContentSetting(
      url_0, url_0, CONTENT_SETTING_ALLOW);
  state.OnGeolocationPermissionSet(url_0, true);

  GURL url_1("http://www.example1.com");
  profile.GetGeolocationContentSettingsMap()->SetContentSetting(
    url_1, url_0, CONTENT_SETTING_BLOCK);
  state.OnGeolocationPermissionSet(url_1, false);

  GeolocationSettingsState::StateMap state_map =
      state.state_map();
  EXPECT_EQ(2U, state_map.size());

  GeolocationSettingsState::FormattedHostsPerState formatted_host_per_state;
  unsigned int tab_state_flags = 0;
  state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
  EXPECT_TRUE(tab_state_flags &
              GeolocationSettingsState::TABSTATE_HAS_ANY_ALLOWED)
              << tab_state_flags;
  EXPECT_TRUE(tab_state_flags &
              GeolocationSettingsState::TABSTATE_HAS_EXCEPTION)
              << tab_state_flags;
  EXPECT_FALSE(tab_state_flags &
               GeolocationSettingsState::TABSTATE_HAS_CHANGED)
               << tab_state_flags;
  EXPECT_TRUE(tab_state_flags &
              GeolocationSettingsState::TABSTATE_HAS_ANY_ICON)
              << tab_state_flags;
  EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                url_0.host()));

  EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].size());
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                url_1.host()));

  state.OnGeolocationPermissionSet(url_0, false);

  formatted_host_per_state.clear();
  tab_state_flags = 0;
  state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
  EXPECT_FALSE(tab_state_flags &
               GeolocationSettingsState::TABSTATE_HAS_ANY_ALLOWED)
               << tab_state_flags;
  EXPECT_TRUE(tab_state_flags &
              GeolocationSettingsState::TABSTATE_HAS_EXCEPTION)
              << tab_state_flags;
  EXPECT_TRUE(tab_state_flags &
              GeolocationSettingsState::TABSTATE_HAS_CHANGED)
              << tab_state_flags;
  EXPECT_TRUE(tab_state_flags &
              GeolocationSettingsState::TABSTATE_HAS_ANY_ICON)
              << tab_state_flags;
  EXPECT_EQ(0U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
  EXPECT_EQ(2U, formatted_host_per_state[CONTENT_SETTING_BLOCK].size());
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                url_0.host()));
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                url_1.host()));

  state.OnGeolocationPermissionSet(url_0, true);

  load_committed_details.previous_url = url_0;
  state.DidNavigate(load_committed_details);

  GeolocationSettingsState::StateMap new_state_map =
      state.state_map();
  EXPECT_EQ(state_map.size(), new_state_map.size());

  GURL different_url("http://foo.com");
  entry.set_url(different_url);
  state.DidNavigate(load_committed_details);

  EXPECT_TRUE(state.state_map().empty());

  formatted_host_per_state.clear();
  tab_state_flags = 0;
  state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
  EXPECT_TRUE(formatted_host_per_state.empty());
  EXPECT_EQ(0U, tab_state_flags);
}

TEST_F(GeolocationSettingsStateTests, ShowPortOnSameHost) {
  TestingProfile profile;
  GeolocationSettingsState state(&profile);
  GURL url_0("http://www.example.com");

  NavigationEntry entry;
  entry.set_url(url_0);
  NavigationController::LoadCommittedDetails load_committed_details;
  load_committed_details.entry = &entry;
  state.DidNavigate(load_committed_details);

  profile.GetGeolocationContentSettingsMap()->SetContentSetting(
      url_0, url_0, CONTENT_SETTING_ALLOW);
  state.OnGeolocationPermissionSet(url_0, true);

  GURL url_1("https://www.example.com");
  profile.GetGeolocationContentSettingsMap()->SetContentSetting(
      url_1, url_0, CONTENT_SETTING_ALLOW);
  state.OnGeolocationPermissionSet(url_1, true);

  GURL url_2("http://www.example1.com");
  profile.GetGeolocationContentSettingsMap()->SetContentSetting(
  url_2, url_0, CONTENT_SETTING_ALLOW);
  state.OnGeolocationPermissionSet(url_2, true);

  GeolocationSettingsState::StateMap state_map =
      state.state_map();
  EXPECT_EQ(3U, state_map.size());

  GeolocationSettingsState::FormattedHostsPerState formatted_host_per_state;
  unsigned int tab_state_flags = 0;
  state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);

  EXPECT_EQ(3U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                url_0.spec()));
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                url_1.spec()));
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                url_2.host()));

  state.OnGeolocationPermissionSet(url_1, false);
  formatted_host_per_state.clear();
  tab_state_flags = 0;
  state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);

  EXPECT_EQ(2U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                url_0.spec()));
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                url_2.host()));
  EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].size());
  EXPECT_EQ(1U,
            formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                url_1.spec()));
}


}  // namespace
