// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.notifications
enum NotificationChannelStatus { ENABLED, BLOCKED, UNAVAILABLE };

struct NotificationChannel {
  NotificationChannel(std::string origin, NotificationChannelStatus status)
      : origin_(origin), status_(status) {}
  bool operator==(const NotificationChannel& other) const {
    return origin_ == other.origin_ && status_ == other.status_;
  }
  bool operator<(const NotificationChannel& other) const {
    return std::tie(origin_, status_) < std::tie(other.origin_, other.status_);
  }
  std::string origin_;
  NotificationChannelStatus status_ = NotificationChannelStatus::UNAVAILABLE;
};

// This class provides notification content settings from system notification
// channels on Android O+. This provider takes precedence over pref-provided
// content settings, but defers to supervised user and policy settings - see
// ordering of the ProviderType enum values in HostContentSettingsMap.
class NotificationChannelsProviderAndroid
    : public content_settings::ObservableProvider {
 public:
  // Helper class to make the JNI calls.
  class NotificationChannelsBridge {
   public:
    virtual ~NotificationChannelsBridge() = default;
    virtual bool ShouldUseChannelSettings() = 0;
    virtual void CreateChannel(const std::string& origin, bool enabled) = 0;
    virtual NotificationChannelStatus GetChannelStatus(
        const std::string& origin) = 0;
    virtual void DeleteChannel(const std::string& origin) = 0;
    virtual std::vector<NotificationChannel> GetChannels() = 0;
  };

  NotificationChannelsProviderAndroid();
  ~NotificationChannelsProviderAndroid() override;

  // ProviderInterface methods:
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      const content_settings::ResourceIdentifier& resource_identifier,
      bool incognito) const override;
  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const content_settings::ResourceIdentifier& resource_identifier,
      base::Value* value) override;
  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;
  void ShutdownOnUIThread() override;

 private:
  explicit NotificationChannelsProviderAndroid(
      std::unique_ptr<NotificationChannelsBridge> bridge);
  friend class NotificationChannelsProviderAndroidTest;

  void CreateChannelIfRequired(const std::string& origin_string,
                               NotificationChannelStatus new_channel_status);

  std::unique_ptr<NotificationChannelsBridge> bridge_;
  bool should_use_channels_;

  // This cache is updated every time GetRuleIterator is called. It is used to
  // check if any channels have changed their status since the previous call,
  // in order to notify observers. This is necessary to detect channels getting
  // blocked/enabled by the user, in the absence of a callback for this event.
  std::vector<NotificationChannel> cached_channels_;

  base::WeakPtrFactory<NotificationChannelsProviderAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotificationChannelsProviderAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_
