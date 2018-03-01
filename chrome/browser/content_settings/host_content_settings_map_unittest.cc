// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/static_cookie_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::MockFunction;
using ::testing::Return;

namespace {

bool MatchPrimaryPattern(const ContentSettingsPattern& expected_primary,
                         const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern) {
  return expected_primary == primary_pattern;
}

}  // namespace

class MockUserModifiableProvider
    : public content_settings::UserModifiableProvider {
 public:
  ~MockUserModifiableProvider() = default;
  MOCK_CONST_METHOD3(GetRuleIterator,
                     std::unique_ptr<content_settings::RuleIterator>(
                         ContentSettingsType,
                         const content_settings::ResourceIdentifier&,
                         bool));

  MOCK_METHOD5(SetWebsiteSetting,
               bool(const ContentSettingsPattern&,
                    const ContentSettingsPattern&,
                    ContentSettingsType,
                    const content_settings::ResourceIdentifier&,
                    base::Value*));

  MOCK_METHOD1(ClearAllContentSettingsRules, void(ContentSettingsType));

  MOCK_METHOD0(ShutdownOnUIThread, void());

  MOCK_METHOD4(GetWebsiteSettingLastModified,
               base::Time(const ContentSettingsPattern&,
                          const ContentSettingsPattern&,
                          ContentSettingsType,
                          const content_settings::ResourceIdentifier&));
};

class HostContentSettingsMapTest : public testing::Test {
 public:
  HostContentSettingsMapTest() = default;

 protected:
  const std::string& GetPrefName(ContentSettingsType type) {
    return content_settings::WebsiteSettingsRegistry::GetInstance()
        ->Get(type)
        ->pref_name();
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
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
      case CONTENT_SETTINGS_TYPE_ADS:
        policy_default_setting_ = prefs::kManagedDefaultAdsSetting;
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
                           std::make_unique<base::Value>(setting));
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
  }

 private:
  sync_preferences::TestingPrefServiceSyncable* prefs_;
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
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(
          GURL(chrome::kChromeUINewTabURL), GURL(chrome::kChromeUINewTabURL),
          CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));

#if BUILDFLAG(ENABLE_PLUGINS)
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
}

TEST_F(HostContentSettingsMapTest, IndividualSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check returning individual settings.
  GURL host("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
#if BUILDFLAG(ENABLE_PLUGINS)
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_PLUGINS, std::string()));
#endif

  // Check returning all settings for a host.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string()));
#if BUILDFLAG(ENABLE_PLUGINS)
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

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_AUTOPLAY, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_AUTOPLAY, std::string()));

  // Check returning all hosts for a setting.
  GURL host2("http://example.org/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(),
      CONTENT_SETTING_BLOCK);
#if BUILDFLAG(ENABLE_PLUGINS)
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_BLOCK);
#endif
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(), &host_settings);
  // |host_settings| contains the default setting and 2 exception.
  EXPECT_EQ(3U, host_settings.size());
#if BUILDFLAG(ENABLE_PLUGINS)
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

TEST_F(HostContentSettingsMapTest, GetWebsiteSettingsForOneType) {
  TestingProfile profile;
  GURL hosts[] = {GURL("https://example1.com/"), GURL("https://example2.com/")};
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      &client_hints_settings);
  EXPECT_EQ(0U, client_hints_settings.size());

  // Add setting for hosts[0].
  const double expiration_time =
      (base::Time::Now() + base::TimeDelta::FromDays(1)).ToDoubleT();
  std::unique_ptr<base::ListValue> expiration_times_list =
      std::make_unique<base::ListValue>();
  expiration_times_list->AppendInteger(42 /* client hint  value */);
  auto expiration_times_dictionary = std::make_unique<base::DictionaryValue>();
  expiration_times_dictionary->SetList("client_hints",
                                       std::move(expiration_times_list));
  expiration_times_dictionary->SetDouble("expiration_time", expiration_time);
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[0], GURL(), CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      std::make_unique<base::Value>(expiration_times_dictionary->Clone()));

  // Reading the settings should now return one setting.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      &client_hints_settings);
  EXPECT_EQ(1U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(*expiration_times_dictionary,
              *client_hints_settings.at(i).setting_value);
  }

  // Add setting for hosts[1].
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[1], GURL(), CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      std::make_unique<base::Value>(expiration_times_dictionary->Clone()));

  // Reading the settings should now return two settings.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      &client_hints_settings);
  EXPECT_EQ(2U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(*expiration_times_dictionary,
              *client_hints_settings.at(i).setting_value);
  }

  // Add settings again for hosts[0].
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[0], GURL(), CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      std::make_unique<base::Value>(expiration_times_dictionary->Clone()));

  // Reading the settings should still return two settings.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_CLIENT_HINTS, std::string(),
      &client_hints_settings);
  EXPECT_EQ(2U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(*expiration_times_dictionary,
              *client_hints_settings.at(i).setting_value);
  }
}

TEST_F(HostContentSettingsMapTest, Clear) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check clearing one type.
  GURL host("http://example.org/");
  GURL host2("http://example.net/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
#if BUILDFLAG(ENABLE_PLUGINS)
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_BLOCK);
#endif
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
#if BUILDFLAG(ENABLE_PLUGINS)
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
  ContentSettingsPattern pattern1 =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("example.org");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern1, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, host2, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
}

// Changing a setting for one origin doesn't affect subdomains.
TEST_F(HostContentSettingsMapTest, Origins) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  GURL host4("http://example.com:8080");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(host1);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host2, host2, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host4, host4, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
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
  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 CONTENT_SETTINGS_TYPE_COOKIES,
                                                 false, primary_pattern,
                                                 secondary_pattern, false));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_ALLOW);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 CONTENT_SETTINGS_TYPE_COOKIES,
                                                 false, _, _, true));
  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 CONTENT_SETTINGS_TYPE_COOKIES,
                                                 false, _, _, true));
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
}

TEST_F(HostContentSettingsMapTest, ObserveDefaultPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  PrefService* prefs = profile.GetPrefs();
  GURL host("http://example.com");

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  const content_settings::WebsiteSettingsInfo* info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          CONTENT_SETTINGS_TYPE_COOKIES);
  // Clearing the backing pref should also clear the internal cache.
  prefs->ClearPref(info->default_value_pref_name());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  // Reseting the pref to its previous value should update the cache.
  prefs->SetInteger(info->default_value_pref_name(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
}

TEST_F(HostContentSettingsMapTest, ObserveExceptionPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  PrefService* prefs = profile.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  std::unique_ptr<base::Value> default_value(
      prefs->FindPreference(GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES))
          ->GetValue()
          ->DeepCopy());

  GURL host("http://example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  // Make a copy of the pref's new value so we can reset it later.
  std::unique_ptr<base::Value> new_value(
      prefs->FindPreference(GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES))
          ->GetValue()
          ->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES), *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES), *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
}

TEST_F(HostContentSettingsMapTest, HostTrimEndingDotCheck) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();

  GURL host_ending_with_dot("http://example.com./");

  EXPECT_TRUE(cookie_settings->IsCookieAccessAllowed(host_ending_with_dot,
                                                     host_ending_with_dot));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(cookie_settings->IsCookieAccessAllowed(host_ending_with_dot,
                                                     host_ending_with_dot));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_COOKIES,
      std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings->IsCookieAccessAllowed(host_ending_with_dot,
                                                      host_ending_with_dot));

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

#if BUILDFLAG(ENABLE_PLUGINS)
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

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_AUTOPLAY, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_AUTOPLAY,
      std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_AUTOPLAY, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), CONTENT_SETTINGS_TYPE_AUTOPLAY,
      std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                CONTENT_SETTINGS_TYPE_AUTOPLAY, std::string()));
}

TEST_F(HostContentSettingsMapTest, NestedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://b.example.com/");
  GURL host3("http://a.b.example.com/");
  GURL host4("http://a.example.com/");
  GURL host5("http://b.b.example.com/");
  ContentSettingsPattern pattern1 =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("[*.]b.example.com");
  ContentSettingsPattern pattern3 =
      ContentSettingsPattern::FromString("a.b.example.com");

  // Test nested patterns for one type.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, nullptr));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern1, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_ALLOW);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern3, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host2, host2, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host4, host4, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host5, host5, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  host_content_settings_map->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, nullptr));

  GURL https_host1("https://b.example.com/");
  GURL https_host2("https://a.b.example.com/");
  ContentSettingsPattern pattern4 =
      ContentSettingsPattern::FromString("b.example.com");

  host_content_settings_map->SetContentSettingCustomScope(
      pattern4, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  // Pattern "b.example.com" will affect (http|https)://b.example.com
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, host2, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                https_host1, https_host1, CONTENT_SETTINGS_TYPE_COOKIES,
                std::string()));
  // Pattern "b.example.com" will not affect (http|https)://a.b.example.com
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                https_host2, https_host2, CONTENT_SETTINGS_TYPE_COOKIES,
                std::string()));
}

TEST_F(HostContentSettingsMapTest, TypeIsolatedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoInheritInitialAllow) {
  // The cookie setting has an initial value of ALLOW, so all changes should be
  // inherited from regular to incognito mode.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  // Changing content settings on the main map should also affect the
  // incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_SESSION_ONLY);
  EXPECT_EQ(CONTENT_SETTING_SESSION_ONLY,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_SESSION_ONLY,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  // Changing content settings on the incognito map should NOT affect the
  // main map.
  otr_map->SetContentSettingDefaultScope(host, GURL(),
                                         CONTENT_SETTINGS_TYPE_COOKIES,
                                         std::string(), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoInheritPopups) {
  // The popup setting has an initial value of BLOCK, but it is allowed
  // to inherit ALLOW settings because it doesn't provide access to user data.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, std::string()));

  // Changing content settings on the main map should affect the
  // incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_POPUPS, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(host, host, CONTENT_SETTINGS_TYPE_POPUPS,
                                       std::string()));

  // Changing content settings on the incognito map should NOT affect the
  // main map.
  host_content_settings_map->SetContentSettingDefaultScope(host, GURL(),
                                         CONTENT_SETTINGS_TYPE_POPUPS,
                                         std::string(), CONTENT_SETTING_BLOCK);
  otr_map->SetContentSettingDefaultScope(host, GURL(),
                                         CONTENT_SETTINGS_TYPE_POPUPS,
                                         std::string(), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_POPUPS, std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoPartialInheritPref) {
  // Permissions marked INHERIT_IF_LESS_PERMISSIVE in
  // ContentSettingsRegistry only inherit BLOCK and ASK settings from regular
  // to incognito if the initial value is ASK.
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
          host, host, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string()));

  // BLOCK should be inherited from the main map to the incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string()));

  // ALLOW should not be inherited from the main map to the incognito map (but
  // it still overwrites the BLOCK, hence incognito reverts to ASK).
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoPartialInheritDefault) {
  // Permissions marked INHERIT_IF_LESS_PERMISSIVE in
  // ContentSettingsRegistry only inherit BLOCK and ASK settings from regular
  // to incognito if the initial value is ASK.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_GEOLOCATION, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_GEOLOCATION, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  // BLOCK should be inherited from the main map to the incognito map.
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_GEOLOCATION, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_GEOLOCATION, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));

  // ALLOW should not be inherited from the main map to the incognito map (but
  // it still overwrites the BLOCK, hence incognito reverts to ASK).
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_GEOLOCATION, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_GEOLOCATION, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoDontInheritSetting) {
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
      base::WrapUnique(test_value.DeepCopy()));

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

TEST_F(HostContentSettingsMapTest, GetUserModifiableContentSetting) {
  GURL url("http://user_exception_allow.com");

  TestingProfile profile;
  // Arbitrarily using cookies as content type to test.
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kManagedDefaultCookiesSetting,
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  map->SetContentSettingDefaultScope(url, url, CONTENT_SETTINGS_TYPE_COOKIES,
                                     std::string(), CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetUserModifiableContentSetting(
                url, url, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url, url, CONTENT_SETTINGS_TYPE_COOKIES,
                                   std::string()));
}

// For a single Unicode encoded pattern, check if it gets converted to punycode
// and old pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeOnly) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set utf-8 data.
  {
    DictionaryPrefUpdate update(prefs,
                                GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES));
    base::DictionaryValue* all_settings_dictionary = update.Get();
    ASSERT_TRUE(NULL != all_settings_dictionary);

    auto dummy_payload = std::make_unique<base::DictionaryValue>();
    dummy_payload->SetInteger("setting", CONTENT_SETTING_ALLOW);
    all_settings_dictionary->SetWithoutPathExpansion("[*.]\xC4\x87ira.com,*",
                                                     std::move(dummy_payload));
  }

  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::DictionaryValue* all_settings_dictionary =
      prefs->GetDictionary(GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES));
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
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_JAVASCRIPT, NULL));

#if BUILDFLAG(ENABLE_PLUGINS)
  // Set preference to manage the default-content-setting for Plugins.
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));

  // Remove the preference to manage the default-content-setting for Plugins.
  prefs->RemoveManagedPref(prefs::kManagedDefaultPluginsSetting);
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_PLUGINS, NULL));
#endif
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_ADS, NULL));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultAdsSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_ADS, NULL));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultAdsSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_ADS, NULL));
}

TEST_F(HostContentSettingsMapTest,
       GetNonDefaultContentSettingsIfTypeManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
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
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
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
  sync_preferences::TestingPrefServiceSyncable* prefs =
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
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
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
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Set user defined default-content-setting for Cookies.
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));

  // Set preference to manage the default-content-setting for Cookies.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
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
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));

  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                CONTENT_SETTINGS_TYPE_COOKIES, NULL));
}

TEST_F(HostContentSettingsMapTest, GetContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");
  GURL embedder("chrome://foo");
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                embedder, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
}

TEST_F(HostContentSettingsMapTest, AddContentSettingsObserver) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::MockObserver mock_observer;

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 pattern, ContentSettingsPattern::Wildcard(),
                                 CONTENT_SETTINGS_TYPE_COOKIES, ""));

  host_content_settings_map->AddObserver(&mock_observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_DEFAULT);
}

// Guest profiles do not exist on Android, so don't run these tests there.
#if !defined(OS_ANDROID)
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
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  // Changing content settings should not result in any prefs being stored
  // however the value should be set in memory.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  const base::DictionaryValue* all_settings_dictionary =
      profile.GetPrefs()->GetDictionary(
          GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES));
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
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));

  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
}

// We used to incorrectly store content settings in prefs for the guest profile.
// We need to ensure these get deleted appropriately.
TEST_F(HostContentSettingsMapTest, GuestProfileMigration) {
  TestingProfile profile;
  profile.SetGuestSession(true);

  // Set a pref manually in the guest profile.
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read("{\"[*.]\\xC4\\x87ira.com,*\":{\"setting\":1}}");
  profile.GetPrefs()->Set(GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES), *value);

  // Test that during construction all the prefs get cleared.
  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::DictionaryValue* all_settings_dictionary =
      profile.GetPrefs()->GetDictionary(
          GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES));
  EXPECT_TRUE(all_settings_dictionary->empty());
}
#endif  // !defined(OS_ANDROID)

TEST_F(HostContentSettingsMapTest, InvalidPattern) {
  // This is a regression test for crbug.com/618529, which fixed a memory leak
  // when a website setting was set under a URL that mapped to an invalid
  // pattern.
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  GURL unsupported_url = GURL("view-source:http://www.google.com");
  base::DictionaryValue test_value;
  test_value.SetString("test", "value");
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      unsupported_url, unsupported_url, CONTENT_SETTINGS_TYPE_APP_BANNER,
      std::string(), base::WrapUnique(test_value.DeepCopy()));
  EXPECT_EQ(nullptr,
            host_content_settings_map->GetWebsiteSetting(
                unsupported_url, unsupported_url,
                CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(), nullptr));
}

TEST_F(HostContentSettingsMapTest, ClearSettingsForOneTypeWithPredicate) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  ContentSettingsForOneType host_settings;

  // Patterns with wildcards.
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.org");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("[*.]example.net");

  // Patterns without wildcards.
  GURL url1("https://www.google.com/");
  GURL url2("https://www.google.com/maps");
  GURL url3("http://www.google.com/maps");
  GURL url3_origin_only("http://www.google.com/");

  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetWebsiteSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(),
      base::WrapUnique(new base::DictionaryValue()));

  // First, test that we clear only COOKIES (not APP_BANNER), and pattern2.
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      CONTENT_SETTINGS_TYPE_COOKIES, base::Time(),
      base::Bind(&MatchPrimaryPattern, pattern2));
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), &host_settings);
  // |host_settings| contains default & block.
  EXPECT_EQ(2U, host_settings.size());
  EXPECT_EQ(pattern, host_settings[0].primary_pattern);
  EXPECT_EQ("*", host_settings[0].secondary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].secondary_pattern.ToString());

  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(), &host_settings);
  // |host_settings| still contains the value for APP_BANNER.
  EXPECT_EQ(1U, host_settings.size());
  EXPECT_EQ(pattern2, host_settings[0].primary_pattern);
  EXPECT_EQ("*", host_settings[0].secondary_pattern.ToString());

  // Next, test that we do correct pattern matching w/ an origin policy item.
  // We verify that we have no settings stored.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(0u, host_settings.size());
  // Add settings.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url1, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  // This setting should override the one above, as it's the same origin.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url2, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url3, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  // Verify we only have two.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(2u, host_settings.size());

  // Clear the http one, which we should be able to do w/ the origin only, as
  // the scope of CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT is
  // REQUESTING_ORIGIN_ONLY_SCOPE.
  ContentSettingsPattern http_pattern =
      ContentSettingsPattern::FromURLNoWildcard(url3_origin_only);
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, base::Time(),
      base::Bind(&MatchPrimaryPattern, http_pattern));
  // Verify we only have one, and it's url1.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(url1),
            host_settings[0].primary_pattern);
}

TEST_F(HostContentSettingsMapTest, ClearSettingsWithTimePredicate) {
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  map->SetClockForTesting(&test_clock);

  ContentSettingsForOneType host_settings;

  GURL url1("https://www.google.com/");
  GURL url2("https://maps.google.com/");

  // Add setting for url1.
  map->SetContentSettingDefaultScope(url1, GURL(), CONTENT_SETTINGS_TYPE_POPUPS,
                                     std::string(), CONTENT_SETTING_BLOCK);

  // Make sure that the timestamp for url1 is different from |t|.
  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  base::Time t = test_clock.Now();

  // Add setting for url2.
  map->SetContentSettingDefaultScope(url2, GURL(), CONTENT_SETTINGS_TYPE_POPUPS,
                                     std::string(), CONTENT_SETTING_BLOCK);

  // Verify we have two pattern and the default.
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS, std::string(),
                             &host_settings);
  EXPECT_EQ(3u, host_settings.size());

  // Clear all settings since |t|.
  map->ClearSettingsForOneTypeWithPredicate(
      CONTENT_SETTINGS_TYPE_POPUPS, t,
      HostContentSettingsMap::PatternSourcePredicate());

  // Verify we only have one pattern (url1) and the default.
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS, std::string(),
                             &host_settings);
  EXPECT_EQ(2u, host_settings.size());
  EXPECT_EQ("https://www.google.com:443",
            host_settings[0].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].primary_pattern.ToString());

  // Clear all settings since the beginning of time.
  map->ClearSettingsForOneTypeWithPredicate(
      CONTENT_SETTINGS_TYPE_POPUPS, base::Time(),
      HostContentSettingsMap::PatternSourcePredicate());

  // Verify we only have the default setting.
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_POPUPS, std::string(),
                             &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ("*", host_settings[0].primary_pattern.ToString());
}

TEST_F(HostContentSettingsMapTest, GetSettingLastModified) {
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  map->SetClockForTesting(&test_clock);

  ContentSettingsForOneType host_settings;

  GURL url("https://www.google.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);

  // Last modified date for non existant settings should be base::Time().
  base::Time t = map->GetSettingLastModifiedDate(
      pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_POPUPS);
  EXPECT_EQ(base::Time(), t);

  // Add setting for url.
  map->SetContentSettingDefaultScope(url, GURL(), CONTENT_SETTINGS_TYPE_POPUPS,
                                     std::string(), CONTENT_SETTING_BLOCK);
  t = map->GetSettingLastModifiedDate(pattern,
                                      ContentSettingsPattern::Wildcard(),
                                      CONTENT_SETTINGS_TYPE_POPUPS);
  EXPECT_EQ(t, test_clock.Now());

  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  // Modify setting.
  map->SetContentSettingDefaultScope(url, GURL(), CONTENT_SETTINGS_TYPE_POPUPS,
                                     std::string(), CONTENT_SETTING_ALLOW);

  t = map->GetSettingLastModifiedDate(pattern,
                                      ContentSettingsPattern::Wildcard(),
                                      CONTENT_SETTINGS_TYPE_POPUPS);
  EXPECT_EQ(t, test_clock.Now());
}

TEST_F(HostContentSettingsMapTest, LastModifiedMultipleModifiableProviders) {
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  GURL url("https://www.google.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);

  base::Time t1 = base::Time::Now();
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(t1);

  base::SimpleTestClock* clock = test_clock.get();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  base::Time t2 = clock->Now();

  // Register a provider which reports a modification time of t1.
  std::unique_ptr<MockUserModifiableProvider> provider =
      std::make_unique<MockUserModifiableProvider>();
  EXPECT_CALL(*provider, GetWebsiteSettingLastModified(
                             _, _, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, _))
      .WillOnce(Return(t1));
  MockUserModifiableProvider* weak_provider = provider.get();
  map->RegisterUserModifiableProvider(
      HostContentSettingsMap::PROVIDER_FOR_TESTS, std::move(provider));

  // Register another provider which reports a modification time of t2.
  std::unique_ptr<MockUserModifiableProvider> other_provider =
      std::make_unique<MockUserModifiableProvider>();
  EXPECT_CALL(*other_provider,
              GetWebsiteSettingLastModified(
                  _, _, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, _))
      .WillRepeatedly(Return(t2));
  MockUserModifiableProvider* weak_other_provider = other_provider.get();
  map->RegisterUserModifiableProvider(
      HostContentSettingsMap::OTHER_PROVIDER_FOR_TESTS,
      std::move(other_provider));

  // Expect the more recent modification time to be reported.
  EXPECT_EQ(t2, map->GetSettingLastModifiedDate(
                    pattern, ContentSettingsPattern::Wildcard(),
                    CONTENT_SETTINGS_TYPE_NOTIFICATIONS));

  // Now have original provider report a more recent modification time.
  clock->Advance(base::TimeDelta::FromSeconds(1));
  base::Time t3 = clock->Now();
  EXPECT_CALL(*weak_provider, GetWebsiteSettingLastModified(
                                  _, _, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, _))
      .WillOnce(Return(t3));

  // Expect the timestamp from the registered provider to be reported now.
  EXPECT_EQ(t3, map->GetSettingLastModifiedDate(
                    pattern, ContentSettingsPattern::Wildcard(),
                    CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  weak_provider->RemoveObserver(map);
  weak_other_provider->RemoveObserver(map);
}

TEST_F(HostContentSettingsMapTest, CanSetNarrowestSetting) {
  TestingProfile profile;
  const auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL valid_url("http://google.com");
  EXPECT_TRUE(map->CanSetNarrowestContentSetting(
      valid_url, valid_url,
      CONTENT_SETTINGS_TYPE_POPUPS));

  GURL invalid_url("about:blank");
  EXPECT_FALSE(map->CanSetNarrowestContentSetting(
      invalid_url, invalid_url,
      CONTENT_SETTINGS_TYPE_POPUPS));
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Test that existing Flash preferences should get copied into the
// |CONTENT_SETTINGS_TYPE_PLUGINS_DATA| setting on the creation of a new
// |HostContentSettingsMap|.
TEST_F(HostContentSettingsMapTest, PluginDataMigration) {
  TestingProfile profile;
  // Set a website-specific Flash preference and a pattern exception.
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      "{\"https://urlwithflashchanged.com:443,*\":{\"setting\":1}, "
      "\"[*.]patternurl.com:443,*\":{\"setting\":1}}");
  profile.GetPrefs()->Set(GetPrefName(CONTENT_SETTINGS_TYPE_PLUGINS), *value);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check it was copied successfully.
  const GURL url1("https://urlwithflashchanged.com");
  EXPECT_NE(nullptr, map->GetWebsiteSetting(url1, url1,
                                            CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
                                            std::string(), nullptr));
  // Check other urls were not affected.
  const GURL url2("https://urlwithflashdefault.com");
  EXPECT_EQ(nullptr, map->GetWebsiteSetting(url2, url2,
                                            CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
                                            std::string(), nullptr));
  // Check patterns are also unaffected.
  const GURL pattern("[*.]patternurl.com");
  EXPECT_EQ(nullptr, map->GetWebsiteSetting(pattern, pattern,
                                            CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
                                            std::string(), nullptr));
}

// If there are existing |CONTENT_SETTINGS_TYPE_PLUGINS_DATA| preferences
// stored, check we skip the migration.
TEST_F(HostContentSettingsMapTest, PluginDataMigrated) {
  TestingProfile profile;
  // Set a website-specific Flash preference and another preference indicating
  // that the Flash setting has changed for a different website.
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      "{\"https://unmigratedurl.com:443,*\":{\"setting\":1}}");
  profile.GetPrefs()->Set(GetPrefName(CONTENT_SETTINGS_TYPE_PLUGINS), *value);
  value = base::JSONReader::Read(
      "{\"https://"
      "example.com:443,*\":{\"setting\":{\"flashPreviouslyChanged\":true}}}");
  profile.GetPrefs()->Set(GetPrefName(CONTENT_SETTINGS_TYPE_PLUGINS_DATA),
                          *value);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check it was copied successfully.
  const GURL flash_data_url("https://example.com");
  EXPECT_NE(nullptr, map->GetWebsiteSetting(flash_data_url, flash_data_url,
                                            CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
                                            std::string(), nullptr));
  // Check the migration code was not run (i.e. the other Flash preference set
  // above was not migrated). Theoretically this should never happen, but this
  // scenario is useful for testing.
  const GURL unmigrated_url("https://unmigratedurl.com");
  EXPECT_EQ(nullptr, map->GetWebsiteSetting(unmigrated_url, unmigrated_url,
                                            CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
                                            std::string(), nullptr));
}
#endif
