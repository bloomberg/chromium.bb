// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/static_cookie_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

using ::testing::_;

class HostContentSettingsMapTest : public testing::Test {
 public:
  HostContentSettingsMapTest() : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  const std::string& GetPrefName(ContentSettingsType type) {
    return content_settings::WebsiteSettingsRegistry::GetInstance()
        ->Get(type)
        ->pref_name();
  }

  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
};

// Wrapper to TestingProfile to reduce test boilerplates, by keeping a fixed
// |content_type| so caller only need to specify it once.
class TesterForType {
 public:
  TesterForType(TestingProfile *profile, ContentSettingsType content_type)
      : prefs_(profile->GetTestingPrefService()),
        host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(profile)),
        content_type_(content_type) {
    switch (content_type_) {
      case CONTENT_SETTINGS_TYPE_COOKIES:
        policy_default_setting_ = prefs::kManagedDefaultCookiesSetting;
        break;
      case CONTENT_SETTINGS_TYPE_POPUPS:
        policy_default_setting_ = prefs::kManagedDefaultPopupsSetting;
        break;
      default:
        // Add support as needed.
        NOTREACHED();
    }
  }

  void ClearPolicyDefault() {
    prefs_->RemoveManagedPref(policy_default_setting_);
  }

  void SetPolicyDefault(ContentSetting setting) {
    prefs_->SetManagedPref(policy_default_setting_,
                           new base::FundamentalValue(setting));
  }

  bool AreUserExceptionsAllowed() {
    return host_content_settings_map_->AreUserExceptionsAllowedForType(
        content_type_);
  }

  void AddUserException(std::string exception,
                        ContentSetting content_settings) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(exception);
    host_content_settings_map_->SetContentSettingCustomScope(
        pattern, pattern, content_type_, std::string(), content_settings);
  }

  // Wrapper to query GetWebsiteSetting(), and only return the source.
  content_settings::SettingSource GetSettingSourceForURL(
      const std::string& url_str) {
    GURL url(url_str);
    content_settings::SettingInfo setting_info;
    std::unique_ptr<base::Value> result =
        host_content_settings_map_->GetWebsiteSetting(
            url, url, content_type_, std::string(), &setting_info);
    return setting_info.source;
  };

 private:
  syncable_prefs::TestingPrefServiceSyncable* prefs_;
  HostContentSettingsMap* host_content_settings_map_;
  ContentSettingsType content_type_;
  const char* policy_default_setting_;

  DISALLOW_COPY_AND_ASSIGN(TesterForType);
};

TEST_F(HostContentSettingsMapTest, DefaultValues) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_IMAGES, NULL));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_content_settings_map->GetContentSetting(
                GURL(chrome::kChromeUINewTabURL),
                GURL(chrome::kChromeUINewTabURL),
                CONTENT_SETTINGS_TYPE_IMAGES,
                std::string()));

#if defined(ENABLE_PLUGINS)
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
#endif

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_POPUPS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_POPUPS, NULL));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_KEYGEN, NULL));
}

TEST_F(HostContentSettingsMapTest, IndividualSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check returning individual settings.
  GURL host("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
#if defined(ENABLE_PLUGINS)
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, std::string()));
#endif

  // Check returning all settings for a host.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));
#if defined(ENABLE_PLUGINS)
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, std::string()));
#endif
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_FULLSCREEN, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_MOUSELOCK, std::string()));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_KEYGEN, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));

  // Check returning all hosts for a setting.
  GURL host2("http://example.org/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
#if defined(ENABLE_PLUGINS)
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_BLOCK);
#endif
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES, std::string(), &host_settings);
  // |host_settings| contains the default setting and an exception.
  EXPECT_EQ(2U, host_settings.size());
#if defined(ENABLE_PLUGINS)
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, std::string(), &host_settings);
  // |host_settings| contains the default setting and 2 exceptions.
  EXPECT_EQ(3U, host_settings.size());
#endif
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_POPUPS, std::string(), &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
}

TEST_F(HostContentSettingsMapTest, Clear) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check clearing one type.
  GURL host("http://example.org/");
  GURL host2("http://example.net/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
#if defined(ENABLE_PLUGINS)
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_BLOCK);
#endif
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES, std::string(), &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
#if defined(ENABLE_PLUGINS)
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PLUGINS, std::string(), &host_settings);
  // |host_settings| contains the default setting and an exception.
  EXPECT_EQ(2U, host_settings.size());
#endif
}

TEST_F(HostContentSettingsMapTest, Patterns) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host1, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, host2, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host3, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
}

TEST_F(HostContentSettingsMapTest, Observer) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  MockSettingsObserver observer(host_content_settings_map);

  GURL host("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::Wildcard();
  EXPECT_CALL(observer,
              OnContentSettingsChanged(host_content_settings_map,
                                       CONTENT_SETTINGS_TYPE_IMAGES,
                                       false,
                                       primary_pattern,
                                       secondary_pattern,
                                       false));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_ALLOW);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnContentSettingsChanged(host_content_settings_map,
                                       CONTENT_SETTINGS_TYPE_IMAGES, false,
                                       _, _, true));
  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnContentSettingsChanged(host_content_settings_map,
                                       CONTENT_SETTINGS_TYPE_IMAGES, false,
                                       _, _, true));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
}

TEST_F(HostContentSettingsMapTest, ObserveDefaultPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  PrefService* prefs = profile.GetPrefs();
  GURL host("http://example.com");

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  const content_settings::WebsiteSettingsInfo* info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          CONTENT_SETTINGS_TYPE_IMAGES);
  // Clearing the backing pref should also clear the internal cache.
  prefs->ClearPref(info->default_value_pref_name());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  // Reseting the pref to its previous value should update the cache.
  prefs->SetInteger(info->default_value_pref_name(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
}

TEST_F(HostContentSettingsMapTest, ObserveExceptionPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  PrefService* prefs = profile.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  std::unique_ptr<base::Value> default_value(
      prefs->FindPreference(GetPrefName(CONTENT_SETTINGS_TYPE_IMAGES))
          ->GetValue()
          ->DeepCopy());

  GURL host("http://example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  // Make a copy of the pref's new value so we can reset it later.
  std::unique_ptr<base::Value> new_value(
      prefs->FindPreference(GetPrefName(CONTENT_SETTINGS_TYPE_IMAGES))
          ->GetValue()
          ->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(GetPrefName(CONTENT_SETTINGS_TYPE_IMAGES), *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(GetPrefName(CONTENT_SETTINGS_TYPE_IMAGES), *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
}

TEST_F(HostContentSettingsMapTest, HostTrimEndingDotCheck) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();

  GURL host_ending_with_dot("http://example.com./");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_IMAGES,
                std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(host_ending_with_dot,
                                                   host_ending_with_dot,
                                                   CONTENT_SETTINGS_TYPE_IMAGES,
                                                   std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(host_ending_with_dot,
                                                   host_ending_with_dot,
                                                   CONTENT_SETTINGS_TYPE_IMAGES,
                                                   std::string()));

  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      host_ending_with_dot, host_ending_with_dot));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(cookie_settings->IsSettingCookieAllowed(
      host_ending_with_dot, host_ending_with_dot));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings->IsSettingCookieAllowed(
      host_ending_with_dot, host_ending_with_dot));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                std::string()));

#if defined(ENABLE_PLUGINS)
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot,
                host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                std::string()));
#endif

  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(host_ending_with_dot,
                                                   host_ending_with_dot,
                                                   CONTENT_SETTINGS_TYPE_POPUPS,
                                                   std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_POPUPS, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(host_ending_with_dot,
                                                   host_ending_with_dot,
                                                   CONTENT_SETTINGS_TYPE_POPUPS,
                                                   std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_POPUPS, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(host_ending_with_dot,
                                                   host_ending_with_dot,
                                                   CONTENT_SETTINGS_TYPE_POPUPS,
                                                   std::string()));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_KEYGEN, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_KEYGEN, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));
}

TEST_F(HostContentSettingsMapTest, NestedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://a.b.example.com/");
  GURL host1("http://example.com/");
  GURL host2("http://b.example.com/");

  host_content_settings_map->SetContentSettingDefaultScope(
      host1, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);

  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_FULLSCREEN, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_MOUSELOCK, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));
}

TEST_F(HostContentSettingsMapTest, OffTheRecord) {
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  // Changing content settings on the main map should also affect the
  // incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  // Changing content settings on the incognito map should NOT affect the
  // main map.
  otr_map->SetContentSettingDefaultScope(host, GURL(),
                                         CONTENT_SETTINGS_TYPE_IMAGES,
                                         std::string(), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
}

TEST_F(HostContentSettingsMapTest, OffTheRecordPartialInheritPref) {
  // Permissions marked INHERIT_IN_INCOGNITO_EXCEPT_ALLOW in
  // ContentSettingsRegistry (e.g. push & notifications) only inherit BLOCK
  // settings from regular to incognito.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));

  // BLOCK should be inherited from the main map to the incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));

  // ALLOW should not be inherited from the main map to the incognito map (but
  // it still overwrites the BLOCK, hence incognito reverts to ASK).
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
}

TEST_F(HostContentSettingsMapTest, OffTheRecordPartialInheritDefault) {
  // Permissions marked INHERIT_IN_INCOGNITO_EXCEPT_ALLOW in
  // ContentSettingsRegistry (e.g. push & notifications) only inherit BLOCK
  // default settings from regular to incognito.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));

  // BLOCK should be inherited from the main map to the incognito map.
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));

  // ALLOW should not be inherited from the main map to the incognito map (but
  // it still overwrites the BLOCK, hence incognito reverts to ASK).
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
}

TEST_F(HostContentSettingsMapTest, OffTheRecordDontInheritSetting) {
  // Website settings marked DONT_INHERIT_IN_INCOGNITO in
  // WebsiteSettingsRegistry (e.g. usb chooser data) don't inherit any values
  // from from regular to incognito.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  // USB chooser data defaults to |nullptr|.
  EXPECT_EQ(nullptr, host_content_settings_map->GetWebsiteSetting(
                         host, host, CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA,
                         std::string(), nullptr));
  EXPECT_EQ(nullptr, otr_map->GetWebsiteSetting(
                         host, host, CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA,
                         std::string(), nullptr));

  base::DictionaryValue test_value;
  test_value.SetString("test", "value");
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      host, host, CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, std::string(),
      test_value.DeepCopy());

  // The setting is not inherted by |otr_map|.
  std::unique_ptr<base::Value> stored_value =
      host_content_settings_map->GetWebsiteSetting(
          host, host, CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, std::string(),
          nullptr);
  EXPECT_TRUE(stored_value && stored_value->Equals(&test_value));
  EXPECT_EQ(nullptr, otr_map->GetWebsiteSetting(
                         host, host, CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA,
                         std::string(), nullptr));
}

TEST_F(HostContentSettingsMapTest, AreUserExceptionsAllowedForType) {
  ContentSettingsType kContentTypesToTest[] = {
    CONTENT_SETTINGS_TYPE_COOKIES,
    CONTENT_SETTINGS_TYPE_POPUPS,
  };

  TestingProfile profile;

  for (ContentSettingsType type : kContentTypesToTest) {
    TesterForType tester(&profile, type);

    // No settings: Yes.
    tester.ClearPolicyDefault();
    EXPECT_TRUE(tester.AreUserExceptionsAllowed());

    // Policy enforces default value: No.
    tester.SetPolicyDefault(CONTENT_SETTING_ALLOW);
    EXPECT_FALSE(tester.AreUserExceptionsAllowed());
    tester.SetPolicyDefault(CONTENT_SETTING_BLOCK);
    EXPECT_FALSE(tester.AreUserExceptionsAllowed());

    // Cleanup for next iteration.
    tester.ClearPolicyDefault();
  }
}

TEST_F(HostContentSettingsMapTest, PrefExceptionsOperation) {
  using content_settings::SETTING_SOURCE_POLICY;
  using content_settings::SETTING_SOURCE_USER;

  const char kUrl1[] = "http://user_exception_allow.com";
  const char kUrl2[] = "http://user_exception_block.com";
  const char kUrl3[] = "http://non_exception.com";

  TestingProfile profile;
  // Arbitrarily using cookies as content type to test.
  TesterForType tester(&profile, CONTENT_SETTINGS_TYPE_COOKIES);

  // Add |kUrl1| and |kUrl2| only.
  tester.AddUserException(kUrl1, CONTENT_SETTING_ALLOW);
  tester.AddUserException(kUrl2, CONTENT_SETTING_BLOCK);

  // No policy setting: follow users settings.
  tester.ClearPolicyDefault();
  // User exceptions.
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl2));
  // User default.
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl3));

  // Policy overrides users always.
  tester.SetPolicyDefault(CONTENT_SETTING_ALLOW);
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl2));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl3));
  tester.SetPolicyDefault(CONTENT_SETTING_BLOCK);
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl2));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl3));
}

// For a single Unicode encoded pattern, check if it gets converted to punycode
// and old pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeOnly) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set utf-8 data.
  {
    DictionaryPrefUpdate update(prefs,
                                GetPrefName(CONTENT_SETTINGS_TYPE_PLUGINS));
    base::DictionaryValue* all_settings_dictionary = update.Get();
    ASSERT_TRUE(NULL != all_settings_dictionary);

    base::DictionaryValue* dummy_payload = new base::DictionaryValue;
    dummy_payload->SetInteger("setting", CONTENT_SETTING_ALLOW);
    all_settings_dictionary->SetWithoutPathExpansion("[*.]\xC4\x87ira.com,*",
                                                     dummy_payload);
  }

  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::DictionaryValue* all_settings_dictionary =
      prefs->GetDictionary(GetPrefName(CONTENT_SETTINGS_TYPE_PLUGINS));
  const base::DictionaryValue* result = NULL;
  EXPECT_FALSE(all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      "[*.]\xC4\x87ira.com,*", &result));
  EXPECT_TRUE(all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      "[*.]xn--ira-ppa.com,*", &result));
}

// If both Unicode and its punycode pattern exist, make sure we don't touch the
// settings for the punycode, and that Unicode pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeAndPunycode) {
  TestingProfile profile;

  std::unique_ptr<base::Value> value =
      base::JSONReader::Read("{\"[*.]\\xC4\\x87ira.com,*\":{\"setting\":1}}");
  profile.GetPrefs()->Set(GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES), *value);

  // Set punycode equivalent, with different setting.
  std::unique_ptr<base::Value> puny_value =
      base::JSONReader::Read("{\"[*.]xn--ira-ppa.com,*\":{\"setting\":2}}");
  profile.GetPrefs()->Set(GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES),
                          *puny_value);

  // Initialize the content map.
  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::DictionaryValue& content_setting_prefs =
      *profile.GetPrefs()->GetDictionary(
          GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES));
  std::string prefs_as_json;
  base::JSONWriter::Write(content_setting_prefs, &prefs_as_json);
  EXPECT_STREQ("{\"[*.]xn--ira-ppa.com,*\":{\"setting\":2}}",
               prefs_as_json.c_str());
}

// If a default-content-setting is managed, the managed value should be used
// instead of the default value.
TEST_F(HostContentSettingsMapTest, ManagedDefaultContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  syncable_prefs::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  // Set preference to manage the default-content-setting for Plugins.
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));

#if defined(ENABLE_PLUGINS)
  // Remove the preference to manage the default-content-setting for Plugins.
  prefs->RemoveManagedPref(prefs::kManagedDefaultPluginsSetting);
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
#endif

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_KEYGEN, NULL));

  // Set managed-default content setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultKeygenSetting,
                        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_KEYGEN, NULL));

  // Remove managed-default content settings preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultKeygenSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_KEYGEN, NULL));
}

TEST_F(HostContentSettingsMapTest,
       GetNonDefaultContentSettingsIfTypeManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  syncable_prefs::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Set url for JavaScript setting.
  GURL host("http://example.com/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(),
      CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));

  // Set managed-default-content-setting for content-settings-type JavaScript.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));
}

// Managed default content setting should have higher priority
// than user defined patterns.
TEST_F(HostContentSettingsMapTest,
       ManagedDefaultContentSettingIgnoreUserPattern) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  syncable_prefs::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Block all JavaScript.
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  // Set an exception to allow "[*.]example.com"
  GURL host("http://example.com/");

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));

  // Set managed-default-content-settings-preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));
}

// If a default-content-setting is set to managed setting, the user defined
// setting should be preserved.
TEST_F(HostContentSettingsMapTest, OverwrittenDefaultContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  syncable_prefs::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Set user defined default-content-setting for Cookies.
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));

  // Set preference to manage the default-content-setting for Cookies.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));

  // Remove the preference to manage the default-content-setting for Cookies.
  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));
}

// If a setting for a default-content-setting-type is set while the type is
// managed, then the new setting should be preserved and used after the
// default-content-setting-type is not managed anymore.
TEST_F(HostContentSettingsMapTest, SettingDefaultContentSettingsWhenManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  syncable_prefs::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        new base::FundamentalValue(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));

  prefs->RemoveManagedPref(prefs::kManagedDefaultPluginsSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
}

TEST_F(HostContentSettingsMapTest, GetContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");
  GURL embedder("chrome://foo");
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                embedder, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
}

TEST_F(HostContentSettingsMapTest, IsDefaultSettingAllowedForType) {
  EXPECT_FALSE(HostContentSettingsMap::IsDefaultSettingAllowedForType(
      CONTENT_SETTING_ALLOW, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
  EXPECT_FALSE(HostContentSettingsMap::IsDefaultSettingAllowedForType(
      CONTENT_SETTING_ALLOW, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
}

TEST_F(HostContentSettingsMapTest, AddContentSettingsObserver) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::MockObserver mock_observer;

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(pattern,
                                      ContentSettingsPattern::Wildcard(),
                                      CONTENT_SETTINGS_TYPE_IMAGES,
                                      ""));

  host_content_settings_map->AddObserver(&mock_observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_DEFAULT);
}

TEST_F(HostContentSettingsMapTest, GuestProfile) {
  TestingProfile profile;
  profile.SetGuestSession(true);
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  // Changing content settings should not result in any prefs being stored
  // however the value should be set in memory.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_IMAGES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  const base::DictionaryValue* all_settings_dictionary =
      profile.GetPrefs()->GetDictionary(
          GetPrefName(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_TRUE(all_settings_dictionary->empty());
}

// Default settings should not be modifiable for the guest profile (there is no
// UI to do this).
TEST_F(HostContentSettingsMapTest, GuestProfileDefaultSetting) {
  TestingProfile profile;
  profile.SetGuestSession(true);
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");

  // There are no custom rules, so this should be the default.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_IMAGES, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_IMAGES, std::string()));
}

// We used to incorrectly store content settings in prefs for the guest profile.
// We need to ensure these get deleted appropriately.
TEST_F(HostContentSettingsMapTest, GuestProfileMigration) {
  TestingProfile profile;
  profile.SetGuestSession(true);

  // Set a pref manually in the guest profile.
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read("{\"[*.]\\xC4\\x87ira.com,*\":{\"setting\":1}}");
  profile.GetPrefs()->Set(GetPrefName(CONTENT_SETTINGS_TYPE_IMAGES), *value);

  // Test that during construction all the prefs get cleared.
  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::DictionaryValue* all_settings_dictionary =
      profile.GetPrefs()->GetDictionary(
          GetPrefName(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_TRUE(all_settings_dictionary->empty());
}

TEST_F(HostContentSettingsMapTest, MigrateOldSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Set old formatted settings.
  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(host);

  // Default setting is BLOCK.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));

  host_content_settings_map->SetContentSettingCustomScope(
      pattern, pattern, CONTENT_SETTINGS_TYPE_KEYGEN, std::string(),
      CONTENT_SETTING_ALLOW);
  // Because of the old formatted setting entry which has two same patterns,
  // SetContentSetting() to (host, GURL()) will be ignored.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_KEYGEN, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));

  host_content_settings_map->MigrateOldSettings();

  ContentSettingsForOneType settings;
  host_content_settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_KEYGEN,
                                                   std::string(), &settings);
  for (const ContentSettingPatternSource& setting_entry : settings) {
    EXPECT_EQ(setting_entry.secondary_pattern,
              ContentSettingsPattern::Wildcard());
  }

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));

  // After migrating old settings, changes to the setting works.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_KEYGEN, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_KEYGEN, std::string()));
}
