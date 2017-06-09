// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_channels_provider_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "jni/NotificationSettingsBridge_jni.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

class NotificationChannelsBridgeImpl
    : public NotificationChannelsProviderAndroid::NotificationChannelsBridge {
 public:
  NotificationChannelsBridgeImpl() = default;
  ~NotificationChannelsBridgeImpl() override = default;

  bool ShouldUseChannelSettings() override {
    return Java_NotificationSettingsBridge_shouldUseChannelSettings(
        AttachCurrentThread());
  }

  void CreateChannel(const std::string& origin, bool enabled) override {
    JNIEnv* env = AttachCurrentThread();
    Java_NotificationSettingsBridge_createChannel(
        env, ConvertUTF8ToJavaString(env, origin), enabled);
  }

  NotificationChannelStatus GetChannelStatus(
      const std::string& origin) override {
    JNIEnv* env = AttachCurrentThread();
    return static_cast<NotificationChannelStatus>(
        Java_NotificationSettingsBridge_getChannelStatus(
            env, ConvertUTF8ToJavaString(env, origin)));
  }

  void DeleteChannel(const std::string& origin) override {
    JNIEnv* env = AttachCurrentThread();
    Java_NotificationSettingsBridge_deleteChannel(
        env, ConvertUTF8ToJavaString(env, origin));
  }
};

}  // anonymous namespace

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid()
    : NotificationChannelsProviderAndroid(
          base::MakeUnique<NotificationChannelsBridgeImpl>()) {}

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid(
    std::unique_ptr<NotificationChannelsBridge> bridge)
    : bridge_(std::move(bridge)),
      should_use_channels_(bridge_->ShouldUseChannelSettings()) {}

NotificationChannelsProviderAndroid::~NotificationChannelsProviderAndroid() =
    default;

std::unique_ptr<content_settings::RuleIterator>
NotificationChannelsProviderAndroid::GetRuleIterator(
    ContentSettingsType content_type,
    const content_settings::ResourceIdentifier& resource_identifier,
    bool incognito) const {
  // TODO(crbug.com/700377) return rule iterator over all channels
  return nullptr;
}

bool NotificationChannelsProviderAndroid::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const content_settings::ResourceIdentifier& resource_identifier,
    base::Value* value) {
  if (content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS ||
      !should_use_channels_) {
    return false;
  }
  // This provider only handles settings for specific origins.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard() &&
      resource_identifier.empty()) {
    return false;
  }
  url::Origin origin = url::Origin(GURL(primary_pattern.ToString()));
  DCHECK(!origin.unique());
  const std::string origin_string = origin.Serialize();
  ContentSetting setting = content_settings::ValueToContentSetting(value);
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      CreateChannelIfRequired(origin_string,
                              NotificationChannelStatus::ENABLED);
      break;
    case CONTENT_SETTING_BLOCK:
      CreateChannelIfRequired(origin_string,
                              NotificationChannelStatus::BLOCKED);
      break;
    case CONTENT_SETTING_DEFAULT:
      bridge_->DeleteChannel(origin_string);
      break;
    default:
      // We rely on notification settings being one of ALLOW/BLOCK/DEFAULT.
      NOTREACHED();
      break;
  }
  return true;
}

void NotificationChannelsProviderAndroid::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // TODO(crbug.com/700377): If |content_type| == NOTIFICATIONS, delete
  // all channels.
}

void NotificationChannelsProviderAndroid::ShutdownOnUIThread() {
  RemoveAllObservers();
}

void NotificationChannelsProviderAndroid::CreateChannelIfRequired(
    const std::string& origin_string,
    NotificationChannelStatus new_channel_status) {
  auto old_channel_status = bridge_->GetChannelStatus(origin_string);
  if (old_channel_status == NotificationChannelStatus::UNAVAILABLE) {
    bridge_->CreateChannel(
        origin_string,
        new_channel_status == NotificationChannelStatus::ENABLED);
  } else {
    // TODO(awdf): Maybe remove this DCHECK - channel status could change any
    // time so this may be vulnerable to a race condition.
    DCHECK(old_channel_status == new_channel_status);
  }
}
