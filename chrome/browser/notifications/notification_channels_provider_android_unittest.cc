// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_channels_provider_android.h"

#include "base/memory/ptr_util.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;

namespace {
const char kTestOrigin[] = "https://example.com";
}  // namespace

class MockNotificationChannelsBridge
    : public NotificationChannelsProviderAndroid::NotificationChannelsBridge {
 public:
  ~MockNotificationChannelsBridge() = default;
  MOCK_METHOD0(ShouldUseChannelSettings, bool());
  MOCK_METHOD2(CreateChannel, void(const std::string&, bool));
  MOCK_METHOD1(GetChannelStatus, NotificationChannelStatus(const std::string&));
  MOCK_METHOD1(DeleteChannel, void(const std::string&));
  MOCK_METHOD0(GetChannels, std::vector<NotificationChannel>());
};

class NotificationChannelsProviderAndroidTest : public testing::Test {
 public:
  NotificationChannelsProviderAndroidTest()
      : mock_bridge_(new MockNotificationChannelsBridge()) {}
  ~NotificationChannelsProviderAndroidTest() override {
    channels_provider_->ShutdownOnUIThread();
  }

 protected:
  void InitChannelsProvider(bool should_use_channels) {
    EXPECT_CALL(*mock_bridge_, ShouldUseChannelSettings())
        .WillOnce(Return(should_use_channels));
    ON_CALL(*mock_bridge_, GetChannelStatus(_))
        .WillByDefault(Return(NotificationChannelStatus::UNAVAILABLE));

    // Can't use base::MakeUnique because the provider's constructor is private.
    channels_provider_ =
        base::WrapUnique(new NotificationChannelsProviderAndroid(
            base::WrapUnique(mock_bridge_)));
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  // No leak because ownership is passed to channels_provider_ in constructor.
  MockNotificationChannelsBridge* mock_bridge_;

  std::unique_ptr<NotificationChannelsProviderAndroid> channels_provider_;
};

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingWhenChannelsShouldNotBeUsed_NoopAndReturnsFalse) {
  this->InitChannelsProvider(false /* should_use_channels */);
  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(result);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingAllowedWhenChannelUnavailable_CreatesEnabledChannel) {
  InitChannelsProvider(true /* should_use_channels */);
  EXPECT_CALL(*mock_bridge_, GetChannelStatus(kTestOrigin))
      .WillOnce(Return(NotificationChannelStatus::UNAVAILABLE));
  EXPECT_CALL(*mock_bridge_, CreateChannel(kTestOrigin, true /* enabled */));

  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_ALLOW));

  EXPECT_TRUE(result);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingBlockedWhenChannelUnavailable_CreatesDisabledChannel) {
  InitChannelsProvider(true /* should_use_channels */);
  EXPECT_CALL(*mock_bridge_, GetChannelStatus(kTestOrigin))
      .WillOnce(Return(NotificationChannelStatus::UNAVAILABLE));
  EXPECT_CALL(*mock_bridge_, CreateChannel(kTestOrigin, false /* enabled */));

  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(result);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingAllowedWhenChannelAllowed_NoopAndReturnsTrue) {
  InitChannelsProvider(true /* should_use_channels */);
  EXPECT_CALL(*mock_bridge_, GetChannelStatus(kTestOrigin))
      .WillOnce(Return(NotificationChannelStatus::ENABLED));

  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_ALLOW));

  EXPECT_TRUE(result);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingBlockedWhenChannelBlocked_NoopAndReturnsTrue) {
  InitChannelsProvider(true /* should_use_channels */);
  EXPECT_CALL(*mock_bridge_, GetChannelStatus(kTestOrigin))
      .WillOnce(Return(NotificationChannelStatus::BLOCKED));

  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(result);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingDefault_DeletesChannelAndReturnsTrue) {
  InitChannelsProvider(true /* should_use_channels */);
  EXPECT_CALL(*mock_bridge_, DeleteChannel(kTestOrigin));
  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(), nullptr);

  EXPECT_TRUE(result);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetRuleIteratorWhenChannelsShouldNotBeUsed) {
  InitChannelsProvider(false /* should_use_channels */);
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      false /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest, GetRuleIteratorForIncognito) {
  InitChannelsProvider(true /* should_use_channels */);
  EXPECT_FALSE(
      channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                          std::string(), true /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetRuleIteratorWhenNoChannelsExist) {
  InitChannelsProvider(true /* should_use_channels */);
  EXPECT_CALL(*mock_bridge_, GetChannels());
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      false /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetRuleIteratorWhenOneBlockedChannelExists) {
  InitChannelsProvider(true /* should_use_channels */);
  std::vector<NotificationChannel> channels;
  channels.emplace_back(kTestOrigin, NotificationChannelStatus::BLOCKED);
  EXPECT_CALL(*mock_bridge_, GetChannels()).WillOnce(Return(channels));
  std::unique_ptr<content_settings::RuleIterator> result =
      channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(result->HasNext());
  content_settings::Rule rule = result->Next();
  EXPECT_EQ(ContentSettingsPattern::FromString(kTestOrigin),
            rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(rule.value.get()));
  EXPECT_FALSE(result->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetRuleIteratorWhenOneAllowedChannelExists) {
  InitChannelsProvider(true /* should_use_channels */);
  std::vector<NotificationChannel> channels;
  channels.emplace_back(kTestOrigin, NotificationChannelStatus::ENABLED);
  EXPECT_CALL(*mock_bridge_, GetChannels()).WillOnce(Return(channels));
  std::unique_ptr<content_settings::RuleIterator> result =
      channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(result->HasNext());
  content_settings::Rule rule = result->Next();
  EXPECT_EQ(ContentSettingsPattern::FromString(kTestOrigin),
            rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(rule.value.get()));
  EXPECT_FALSE(result->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetRuleIteratorWhenMultipleChannelsExist) {
  InitChannelsProvider(true /* should_use_channels */);
  std::vector<NotificationChannel> channels;
  channels.emplace_back("https://abc.com", NotificationChannelStatus::ENABLED);
  channels.emplace_back("https://xyz.com", NotificationChannelStatus::BLOCKED);
  EXPECT_CALL(*mock_bridge_, GetChannels()).WillOnce(Return(channels));
  std::unique_ptr<content_settings::RuleIterator> result =
      channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(result->HasNext());
  content_settings::Rule first_rule = result->Next();
  EXPECT_EQ(ContentSettingsPattern::FromString("https://abc.com"),
            first_rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(first_rule.value.get()));
  EXPECT_TRUE(result->HasNext());
  content_settings::Rule second_rule = result->Next();
  EXPECT_EQ(ContentSettingsPattern::FromString("https://xyz.com"),
            second_rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(second_rule.value.get()));
  EXPECT_FALSE(result->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetRuleIteratorNotifiesObserversIfStatusChanges) {
  InitChannelsProvider(true /* should_use_channels */);
  content_settings::MockObserver mock_observer;
  channels_provider_->AddObserver(&mock_observer);

  // Set up sequenced expectations. The observer should be notified on the first
  // GetRuleIterator, and then only if channel status has changed to blocked.
  // See "Using Check Points" section of the GTest Cookbook:
  // https://github.com/google/googletest/blob/master/googlemock/docs/CookBook.md#using-check-points
  MockFunction<void(std::string check_point_name)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("0: GetRuleIterator never called"));
    // Report channel as enabled initially.
    std::vector<NotificationChannel> channels;
    channels.emplace_back("https://example.com",
                          NotificationChannelStatus::ENABLED);
    EXPECT_CALL(*mock_bridge_, GetChannels()).WillOnce(Return(channels));

    // Observer should be notified on first invocation of GetRuleIterator.
    EXPECT_CALL(
        mock_observer,
        OnContentSettingChanged(_, _, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, ""));
    EXPECT_CALL(check, Call("1: GetRuleIterator()"));

    // Continue to report channel as enabled.
    EXPECT_CALL(*mock_bridge_, GetChannels()).WillOnce(Return(channels));

    // Observer should not be notified this time.
    EXPECT_CALL(check, Call("2: GetRuleIterator()"));

    // Now report channel as blocked.
    channels[0].status = NotificationChannelStatus::BLOCKED;
    EXPECT_CALL(*mock_bridge_, GetChannels()).WillOnce(Return(channels));

    // GetRuleIterator should now notify observer.
    EXPECT_CALL(
        mock_observer,
        OnContentSettingChanged(_, _, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, ""));
    EXPECT_CALL(check, Call("3: GetRuleIterator()"));
  }

  check.Call("0: GetRuleIterator never called");
  channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), false /* incognito */);
  content::RunAllBlockingPoolTasksUntilIdle();
  check.Call("1: GetRuleIterator()");
  channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), false /* incognito */);
  content::RunAllBlockingPoolTasksUntilIdle();
  check.Call("2: GetRuleIterator()");
  channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), false /* incognito */);
  content::RunAllBlockingPoolTasksUntilIdle();
  check.Call("3: GetRuleIterator()");
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingNotifiesObserver) {
  InitChannelsProvider(true /* should_use_channels */);
  content_settings::MockObserver mock_observer;
  channels_provider_->AddObserver(&mock_observer);

  EXPECT_CALL(*mock_bridge_, CreateChannel(kTestOrigin, true /* enabled */));
  EXPECT_CALL(
      mock_observer,
      OnContentSettingChanged(_, _, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, ""));
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_ALLOW));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesDeletesChannelsAndNotifiesObservers) {
  InitChannelsProvider(true /* should_use_channels */);
  content_settings::MockObserver mock_observer;
  channels_provider_->AddObserver(&mock_observer);

  std::vector<NotificationChannel> channels;
  channels.emplace_back("https://abc.com", NotificationChannelStatus::ENABLED);
  channels.emplace_back("https://xyz.com", NotificationChannelStatus::BLOCKED);
  ON_CALL(*mock_bridge_, GetChannels()).WillByDefault(Return(channels));

  EXPECT_CALL(*mock_bridge_, DeleteChannel("https://abc.com"));
  EXPECT_CALL(*mock_bridge_, DeleteChannel("https://xyz.com"));
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  ContentSettingsPattern(), ContentSettingsPattern(),
                  CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));
  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesNoopsForUnrelatedContentSettings) {
  InitChannelsProvider(true /* should_use_channels */);

  EXPECT_CALL(*mock_bridge_, GetChannels()).Times(0);
  EXPECT_CALL(*mock_bridge_, DeleteChannel(_)).Times(0);

  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_COOKIES);
  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesNoopsIfNotUsingChannels) {
  InitChannelsProvider(false /* should_use_channels */);

  EXPECT_CALL(*mock_bridge_, GetChannels()).Times(0);
  EXPECT_CALL(*mock_bridge_, DeleteChannel(_)).Times(0);

  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}
