// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_channels_provider_android.h"

#include <map>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace {
const char kTestOrigin[] = "https://example.com";
}  // namespace

class FakeNotificationChannelsBridge
    : public NotificationChannelsProviderAndroid::NotificationChannelsBridge {
 public:
  explicit FakeNotificationChannelsBridge(bool should_use_channels) {
    should_use_channels_ = should_use_channels;
  }

  ~FakeNotificationChannelsBridge() override = default;

  void SetChannelStatus(const std::string& origin,
                        NotificationChannelStatus status) {
    switch (status) {
      case NotificationChannelStatus::UNAVAILABLE:
        channels_.erase(origin);
        return;
      case NotificationChannelStatus::ENABLED:
      case NotificationChannelStatus::BLOCKED:
        auto entry = channels_.find(origin);
        if (entry != channels_.end())
          entry->second.status = status;
        else
          channels_.emplace(origin, NotificationChannel(origin, status));
        return;
    }
  }

  // NotificationChannelsBridge methods.

  bool ShouldUseChannelSettings() override { return should_use_channels_; }

  void CreateChannel(const std::string& origin, bool enabled) override {
    // Note if a channel for the given origin was already created, this is a
    // no-op. This is intentional, to match the Android Channels API.
    channels_.emplace(
        origin, NotificationChannel(
                    origin, enabled ? NotificationChannelStatus::ENABLED
                                    : NotificationChannelStatus::BLOCKED));
  }

  NotificationChannelStatus GetChannelStatus(
      const std::string& origin) override {
    auto entry = channels_.find(origin);
    if (entry != channels_.end())
      return entry->second.status;
    return NotificationChannelStatus::UNAVAILABLE;
  }

  void DeleteChannel(const std::string& origin) override {
    channels_.erase(origin);
  }

  std::vector<NotificationChannel> GetChannels() override {
    std::vector<NotificationChannel> channels;
    for (auto it = channels_.begin(); it != channels_.end(); it++)
      channels.push_back(it->second);
    return channels;
  }

 private:
  bool should_use_channels_;
  std::map<std::string, NotificationChannel> channels_;

  DISALLOW_COPY_AND_ASSIGN(FakeNotificationChannelsBridge);
};

class NotificationChannelsProviderAndroidTest : public testing::Test {
 public:
  NotificationChannelsProviderAndroidTest() = default;

  ~NotificationChannelsProviderAndroidTest() override {
    channels_provider_->ShutdownOnUIThread();
  }

 protected:
  void InitChannelsProvider(bool should_use_channels) {
    fake_bridge_ = new FakeNotificationChannelsBridge(should_use_channels);

    // Can't use base::MakeUnique because the provider's constructor is private.
    channels_provider_ =
        base::WrapUnique(new NotificationChannelsProviderAndroid(
            base::WrapUnique(fake_bridge_)));
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  std::unique_ptr<NotificationChannelsProviderAndroid> channels_provider_;

  // No leak because ownership is passed to channels_provider_ in constructor.
  FakeNotificationChannelsBridge* fake_bridge_;
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

  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_ALLOW));

  EXPECT_TRUE(result);
  EXPECT_EQ(1u, fake_bridge_->GetChannels().size());
  EXPECT_EQ(
      NotificationChannel(kTestOrigin, NotificationChannelStatus::ENABLED),
      fake_bridge_->GetChannels()[0]);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingBlockedWhenChannelUnavailable_CreatesDisabledChannel) {
  InitChannelsProvider(true /* should_use_channels */);

  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(result);
  EXPECT_EQ(1u, fake_bridge_->GetChannels().size());
  EXPECT_EQ(
      NotificationChannel(kTestOrigin, NotificationChannelStatus::BLOCKED),
      fake_bridge_->GetChannels()[0]);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingAllowedWhenChannelAllowed_NoopAndReturnsTrue) {
  InitChannelsProvider(true /* should_use_channels */);
  fake_bridge_->CreateChannel(kTestOrigin, true);

  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_ALLOW));

  EXPECT_TRUE(result);
  EXPECT_EQ(1u, fake_bridge_->GetChannels().size());
  EXPECT_EQ(
      NotificationChannel(kTestOrigin, NotificationChannelStatus::ENABLED),
      fake_bridge_->GetChannels()[0]);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingBlockedWhenChannelBlocked_NoopAndReturnsTrue) {
  InitChannelsProvider(true /* should_use_channels */);
  fake_bridge_->CreateChannel(kTestOrigin, false);

  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      new base::Value(CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(result);
  EXPECT_EQ(1u, fake_bridge_->GetChannels().size());
  EXPECT_EQ(
      NotificationChannel(kTestOrigin, NotificationChannelStatus::BLOCKED),
      fake_bridge_->GetChannels()[0]);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingDefault_DeletesChannelAndReturnsTrue) {
  InitChannelsProvider(true /* should_use_channels */);
  fake_bridge_->CreateChannel(kTestOrigin, true);
  bool result = channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(kTestOrigin), ContentSettingsPattern(),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(), nullptr);

  EXPECT_TRUE(result);
  EXPECT_EQ(0u, fake_bridge_->GetChannels().size());
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
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
      false /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetRuleIteratorWhenOneBlockedChannelExists) {
  InitChannelsProvider(true /* should_use_channels */);
  fake_bridge_->CreateChannel(kTestOrigin, false);

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
  fake_bridge_->CreateChannel(kTestOrigin, true);

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
  fake_bridge_->CreateChannel("https://abc.com", true);
  fake_bridge_->CreateChannel("https://xyz.com", false);

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

  // Create channel as enabled initially.
  fake_bridge_->CreateChannel("https://example.com", true);

  // Observer should be notified on first invocation of GetRuleIterator.
  EXPECT_CALL(
      mock_observer,
      OnContentSettingChanged(_, _, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, ""));
  channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), false /* incognito */);
  content::RunAllBlockingPoolTasksUntilIdle();

  // Observer should not be notified the second time.
  channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), false /* incognito */);
  content::RunAllBlockingPoolTasksUntilIdle();

  // Now emulate user blocking the channel.
  fake_bridge_->SetChannelStatus("https://example.com",
                                 NotificationChannelStatus::BLOCKED);
  // GetRuleIterator should now notify observer.
  EXPECT_CALL(
      mock_observer,
      OnContentSettingChanged(_, _, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, ""));
  channels_provider_->GetRuleIterator(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                      std::string(), false /* incognito */);
  content::RunAllBlockingPoolTasksUntilIdle();
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingNotifiesObserver) {
  InitChannelsProvider(true /* should_use_channels */);
  content_settings::MockObserver mock_observer;
  channels_provider_->AddObserver(&mock_observer);

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

  // Set up some channels.
  fake_bridge_->SetChannelStatus("https://abc.com",
                                 NotificationChannelStatus::ENABLED);
  fake_bridge_->SetChannelStatus("https://xyz.com",
                                 NotificationChannelStatus::BLOCKED);

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  ContentSettingsPattern(), ContentSettingsPattern(),
                  CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string()));

  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS);

  // Check channels were deleted.
  EXPECT_EQ(0u, fake_bridge_->GetChannels().size());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesNoopsForUnrelatedContentSettings) {
  InitChannelsProvider(true /* should_use_channels */);

  // Set up some channels.
  fake_bridge_->SetChannelStatus("https://abc.com",
                                 NotificationChannelStatus::ENABLED);
  fake_bridge_->SetChannelStatus("https://xyz.com",
                                 NotificationChannelStatus::BLOCKED);

  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_COOKIES);
  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_GEOLOCATION);

  // Check the channels still exist.
  EXPECT_EQ(2u, fake_bridge_->GetChannels().size());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesNoopsIfNotUsingChannels) {
  InitChannelsProvider(false /* should_use_channels */);
  channels_provider_->ClearAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}
