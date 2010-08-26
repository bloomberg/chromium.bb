// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_content_settings_map.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

class GeolocationContentSettingsMapTests : public testing::Test {
 public:
  GeolocationContentSettingsMapTests()
    : ui_thread_(ChromeThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  ChromeThread ui_thread_;
};

TEST_F(GeolocationContentSettingsMapTests, DefaultValues) {
  TestingProfile profile;
  GeolocationContentSettingsMap* map =
      profile.GetGeolocationContentSettingsMap();

  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetDefaultContentSetting());
  map->SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, map->GetDefaultContentSetting());
}

TEST_F(GeolocationContentSettingsMapTests, Embedder) {
  TestingProfile profile;
  GeolocationContentSettingsMap* map =
      profile.GetGeolocationContentSettingsMap();
  GURL top_level("http://www.toplevel0.com/foo/bar");
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(top_level, top_level));
  // Now set the permission for requester_0.
  map->SetContentSetting(top_level, top_level, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(top_level, top_level));

  GURL requester_0("http://www.frame0.com/foo/bar");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(requester_0, top_level));
  // Now set the permission for only requester_1.
  map->SetContentSetting(requester_0, top_level, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(top_level, top_level));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_0, top_level));
  // Block only requester_1.
  map->SetContentSetting(requester_0, top_level, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(requester_0, top_level));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(top_level, top_level));
}

TEST_F(GeolocationContentSettingsMapTests, MultipleEmbeddersAndOrigins) {
  TestingProfile profile;
  GeolocationContentSettingsMap* map =
      profile.GetGeolocationContentSettingsMap();
  GURL requester_0("http://www.iframe0.com/foo/bar");
  GURL requester_1("http://www.iframe1.co.uk/bar/foo");
  GURL embedder_0("http://www.toplevel0.com/foo/bar");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(requester_0, embedder_0));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(requester_1, embedder_0));
  // Now set the permission for only one origin.
  map->SetContentSetting(requester_0, embedder_0, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_0, embedder_0));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(requester_1, embedder_0));
  // Set the permission for the other origin.
  map->SetContentSetting(requester_1, embedder_0, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_1, embedder_0));
  // Check they're not allowed on a different embedder.
  GURL embedder_1("http://www.toplevel1.com/foo/bar");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(requester_0, embedder_1));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(requester_1, embedder_1));
  // Check all settings are valid.
  GeolocationContentSettingsMap::AllOriginsSettings content_settings(
      map->GetAllOriginsSettings());
  EXPECT_EQ(0U, content_settings.count(requester_0));
  EXPECT_EQ(1U, content_settings.count(requester_0.GetOrigin()));
  GeolocationContentSettingsMap::OneOriginSettings one_origin_settings(
      content_settings[requester_0.GetOrigin()]);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, one_origin_settings[embedder_0.GetOrigin()]);

  EXPECT_EQ(0U, content_settings.count(requester_1));
  EXPECT_EQ(1U, content_settings.count(requester_1.GetOrigin()));
  one_origin_settings = content_settings[requester_1.GetOrigin()];
  EXPECT_EQ(CONTENT_SETTING_ALLOW, one_origin_settings[embedder_0.GetOrigin()]);
  // Block requester_1 on the first embedder and add it to the second.
  map->SetContentSetting(requester_1, embedder_0, CONTENT_SETTING_BLOCK);
  map->SetContentSetting(requester_1, embedder_1, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(requester_1, embedder_0));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_1, embedder_1));
}

TEST_F(GeolocationContentSettingsMapTests, SetContentSettingDefault) {
  TestingProfile profile;
  GeolocationContentSettingsMap* map =
      profile.GetGeolocationContentSettingsMap();
  GURL top_level("http://www.toplevel0.com/foo/bar");
  map->SetContentSetting(top_level, top_level, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(top_level, top_level));
  // Set to CONTENT_SETTING_DEFAULT and check the actual value has changed.
  map->SetContentSetting(top_level, top_level, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetContentSetting(top_level, top_level));
}

TEST_F(GeolocationContentSettingsMapTests, Reset) {
  TestingProfile profile;
  GeolocationContentSettingsMap* map =
      profile.GetGeolocationContentSettingsMap();
  GURL requester_0("http://www.iframe0.com/foo/bar");
  GURL embedder_0("http://www.toplevel0.com/foo/bar");
  map->SetContentSetting(requester_0, embedder_0, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_0, embedder_0));
  GeolocationContentSettingsMap::AllOriginsSettings content_settings(
      map->GetAllOriginsSettings());
  EXPECT_EQ(1U, content_settings.size());

  // Set to CONTENT_SETTING_DEFAULT and check the actual value has changed.
  map->SetContentSetting(requester_0, embedder_0, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(requester_0, embedder_0));
  content_settings = map->GetAllOriginsSettings();
  EXPECT_TRUE(content_settings.empty());
}

TEST_F(GeolocationContentSettingsMapTests, ClearsWhenSettingBackToDefault) {
  TestingProfile profile;
  GeolocationContentSettingsMap* map =
      profile.GetGeolocationContentSettingsMap();
  GURL requester_0("http://www.iframe0.com/foo/bar");
  GURL requester_1("http://www.iframe1.com/foo/bar");
  GURL embedder_0("http://www.toplevel0.com/foo/bar");
  map->SetContentSetting(requester_0, embedder_0, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_0, embedder_0));
  GeolocationContentSettingsMap::AllOriginsSettings content_settings(
      map->GetAllOriginsSettings());
  EXPECT_EQ(1U, content_settings.size());

  map->SetContentSetting(requester_1, embedder_0, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_1, embedder_0));
  content_settings = map->GetAllOriginsSettings();
  EXPECT_EQ(2U, content_settings.size());
  EXPECT_EQ(1U, content_settings[requester_0.GetOrigin()].size());
  EXPECT_EQ(1U, content_settings[requester_1.GetOrigin()].size());

  // Set to default.
  map->SetContentSetting(requester_0, embedder_0, CONTENT_SETTING_DEFAULT);
  content_settings = map->GetAllOriginsSettings();
  EXPECT_EQ(1U, content_settings.size());

  map->SetContentSetting(requester_1, embedder_0, CONTENT_SETTING_DEFAULT);
  content_settings = map->GetAllOriginsSettings();
  EXPECT_TRUE(content_settings.empty());
}

TEST_F(GeolocationContentSettingsMapTests, WildCardForEmptyEmbedder) {
  TestingProfile profile;
  GeolocationContentSettingsMap* map =
      profile.GetGeolocationContentSettingsMap();
  GURL requester_0("http://www.iframe0.com/foo/bar");
  GURL embedder_0("http://www.toplevel0.com/foo/bar");
  GURL embedder_1("http://www.toplevel1.com/foo/bar");
  GURL empty_url;
  map->SetContentSetting(requester_0, embedder_0, CONTENT_SETTING_BLOCK);
  map->SetContentSetting(requester_0, empty_url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(requester_0, embedder_0));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_0, embedder_1));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(requester_0, requester_0));

  // Change the wildcard behavior.
  map->SetContentSetting(requester_0, embedder_0, CONTENT_SETTING_ALLOW);
  map->SetContentSetting(requester_0, empty_url, CONTENT_SETTING_BLOCK);
  map->SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_0, embedder_0));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(requester_0, embedder_1));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(requester_0, requester_0));
}

TEST_F(GeolocationContentSettingsMapTests, IgnoreInvalidURLsInPrefs) {
  TestingProfile profile;
  DictionaryValue* all_settings_dictionary =
      profile.GetPrefs()->GetMutableDictionary(
          prefs::kGeolocationContentSettings);
  // For simplicity, use the overloads that do path expansion. As '.' is the
  // path separator, we can't have dotted hostnames (which is fine).
  all_settings_dictionary->SetInteger("http://a/.http://b/",
                                      CONTENT_SETTING_ALLOW);
  all_settings_dictionary->SetInteger("bad_requester.http://b/",
                                      CONTENT_SETTING_ALLOW);
  all_settings_dictionary->SetInteger("http://a/.bad-embedder",
                                      CONTENT_SETTING_ALLOW);

  GeolocationContentSettingsMap* map =
      profile.GetGeolocationContentSettingsMap();
  EXPECT_EQ(CONTENT_SETTING_ASK, map->GetDefaultContentSetting());

  // Check the valid entry was read OK.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(GURL("http://a/"), GURL("http://b/")));
  // But everything else should be according to the default.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(GURL("http://a/"),
            GURL("http://bad-embedder")));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(GURL("http://a/"),
            GURL("http://example.com")));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(GURL("http://bad_requester/"),
            GURL("http://b/")));
}

}  // namespace
