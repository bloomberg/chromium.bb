// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_usages_state.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::NavigationEntry;

namespace {

class ContentSettingsUsagesStateTests : public testing::Test {
 public:
  ContentSettingsUsagesStateTests()
    : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  void ClearOnNewOrigin(ContentSettingsType type) {
    TestingProfile profile;
    ContentSettingsUsagesState state(&profile, type);
    GURL url_0("http://www.example.com");

    scoped_ptr<NavigationEntry> entry(NavigationEntry::Create());
    entry->SetURL(url_0);
    content::LoadCommittedDetails load_committed_details;
    load_committed_details.entry = entry.get();
    state.DidNavigate(load_committed_details);

    profile.GetHostContentSettingsMap()->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_0),
        ContentSettingsPattern::FromURLNoWildcard(url_0),
        type,
        std::string(),
        CONTENT_SETTING_ALLOW);
    state.OnPermissionSet(url_0, true);

    GURL url_1("http://www.example1.com");
    profile.GetHostContentSettingsMap()->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_1),
        ContentSettingsPattern::FromURLNoWildcard(url_0),
        type,
        std::string(),
        CONTENT_SETTING_BLOCK);
    state.OnPermissionSet(url_1, false);

    ContentSettingsUsagesState::StateMap state_map =
        state.state_map();
    EXPECT_EQ(2U, state_map.size());

    ContentSettingsUsagesState::FormattedHostsPerState formatted_host_per_state;
    unsigned int tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED)
                << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION)
                << tab_state_flags;
    EXPECT_FALSE(tab_state_flags &
                 ContentSettingsUsagesState::TABSTATE_HAS_CHANGED)
                 << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_ANY_ICON)
                << tab_state_flags;
    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
    EXPECT_EQ(1U,
              formatted_host_per_state[CONTENT_SETTING_ALLOW].count(
                  url_0.host()));

    EXPECT_EQ(1U, formatted_host_per_state[CONTENT_SETTING_BLOCK].size());
    EXPECT_EQ(1U,
              formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                  url_1.host()));

    state.OnPermissionSet(url_0, false);

    formatted_host_per_state.clear();
    tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
    EXPECT_FALSE(tab_state_flags &
                 ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED)
                 << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION)
                << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_CHANGED)
                << tab_state_flags;
    EXPECT_TRUE(tab_state_flags &
                ContentSettingsUsagesState::TABSTATE_HAS_ANY_ICON)
                << tab_state_flags;
    EXPECT_EQ(0U, formatted_host_per_state[CONTENT_SETTING_ALLOW].size());
    EXPECT_EQ(2U, formatted_host_per_state[CONTENT_SETTING_BLOCK].size());
    EXPECT_EQ(1U,
              formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                  url_0.host()));
    EXPECT_EQ(1U,
              formatted_host_per_state[CONTENT_SETTING_BLOCK].count(
                  url_1.host()));

    state.OnPermissionSet(url_0, true);

    load_committed_details.previous_url = url_0;
    state.DidNavigate(load_committed_details);

    ContentSettingsUsagesState::StateMap new_state_map =
        state.state_map();
    EXPECT_EQ(state_map.size(), new_state_map.size());

    GURL different_url("http://foo.com");
    entry->SetURL(different_url);
    state.DidNavigate(load_committed_details);

    EXPECT_TRUE(state.state_map().empty());

    formatted_host_per_state.clear();
    tab_state_flags = 0;
    state.GetDetailedInfo(&formatted_host_per_state, &tab_state_flags);
    EXPECT_TRUE(formatted_host_per_state.empty());
    EXPECT_EQ(0U, tab_state_flags);
  }

  void ShowPortOnSameHost(ContentSettingsType type) {
    TestingProfile profile;
    ContentSettingsUsagesState state(&profile, type);
    GURL url_0("http://www.example.com");

    scoped_ptr<NavigationEntry> entry(NavigationEntry::Create());
    entry->SetURL(url_0);
    content::LoadCommittedDetails load_committed_details;
    load_committed_details.entry = entry.get();
    state.DidNavigate(load_committed_details);

    profile.GetHostContentSettingsMap()->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_0),
        ContentSettingsPattern::FromURLNoWildcard(url_0),
        type,
        std::string(),
        CONTENT_SETTING_ALLOW);
    state.OnPermissionSet(url_0, true);

    GURL url_1("https://www.example.com");
    profile.GetHostContentSettingsMap()->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_1),
        ContentSettingsPattern::FromURLNoWildcard(url_0),
        type,
        std::string(),
        CONTENT_SETTING_ALLOW);
    state.OnPermissionSet(url_1, true);

    GURL url_2("http://www.example1.com");
    profile.GetHostContentSettingsMap()->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_2),
        ContentSettingsPattern::FromURLNoWildcard(url_0),
        type,
        std::string(),
        CONTENT_SETTING_ALLOW);
    state.OnPermissionSet(url_2, true);

    ContentSettingsUsagesState::StateMap state_map =
        state.state_map();
    EXPECT_EQ(3U, state_map.size());

    ContentSettingsUsagesState::FormattedHostsPerState formatted_host_per_state;
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

    state.OnPermissionSet(url_1, false);
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

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(ContentSettingsUsagesStateTests, ClearOnNewOriginForGeolocation) {
  ClearOnNewOrigin(CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

TEST_F(ContentSettingsUsagesStateTests, ClearOnNewOriginForMidi) {
  ClearOnNewOrigin(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
}

TEST_F(ContentSettingsUsagesStateTests, ShowPortOnSameHostForGeolocation) {
  ShowPortOnSameHost(CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

TEST_F(ContentSettingsUsagesStateTests, ShowPortOnSameHostForMidi) {
  ShowPortOnSameHost(CONTENT_SETTINGS_TYPE_MIDI_SYSEX);
}

}  // namespace
