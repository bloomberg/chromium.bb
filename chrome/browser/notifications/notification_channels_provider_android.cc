// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_channels_provider_android.h"

#include <algorithm>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/browser_thread.h"
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

  std::vector<NotificationChannel> GetChannels() override {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobjectArray> raw_channels =
        Java_NotificationSettingsBridge_getSiteChannels(env);
    jsize num_channels = env->GetArrayLength(raw_channels.obj());
    std::vector<NotificationChannel> channels;
    for (jsize i = 0; i < num_channels; ++i) {
      jobject jchannel = env->GetObjectArrayElement(raw_channels.obj(), i);
      channels.emplace_back(
          ConvertJavaStringToUTF8(Java_SiteChannel_getOrigin(env, jchannel)),
          static_cast<NotificationChannelStatus>(
              Java_SiteChannel_getStatus(env, jchannel)));
    }
    return channels;
  }
};

ContentSetting ChannelStatusToContentSetting(NotificationChannelStatus status) {
  switch (status) {
    case NotificationChannelStatus::ENABLED:
      return CONTENT_SETTING_ALLOW;
    case NotificationChannelStatus::BLOCKED:
      return CONTENT_SETTING_BLOCK;
    case NotificationChannelStatus::UNAVAILABLE:
      NOTREACHED();
  }
  return CONTENT_SETTING_DEFAULT;
}

class ChannelsRuleIterator : public content_settings::RuleIterator {
 public:
  explicit ChannelsRuleIterator(std::vector<NotificationChannel> channels)
      : channels_(std::move(channels)), index_(0) {}

  ~ChannelsRuleIterator() override = default;

  bool HasNext() const override { return index_ < channels_.size(); }

  content_settings::Rule Next() override {
    DCHECK(HasNext());
    DCHECK_NE(channels_[index_].status_,
              NotificationChannelStatus::UNAVAILABLE);
    content_settings::Rule rule = content_settings::Rule(
        ContentSettingsPattern::FromString(channels_[index_].origin_),
        ContentSettingsPattern::Wildcard(),
        new base::Value(
            ChannelStatusToContentSetting(channels_[index_].status_)));
    index_++;
    return rule;
  }

 private:
  std::vector<NotificationChannel> channels_;
  size_t index_;
  DISALLOW_COPY_AND_ASSIGN(ChannelsRuleIterator);
};

}  // anonymous namespace

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid()
    : NotificationChannelsProviderAndroid(
          base::MakeUnique<NotificationChannelsBridgeImpl>()) {}

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid(
    std::unique_ptr<NotificationChannelsBridge> bridge)
    : bridge_(std::move(bridge)),
      should_use_channels_(bridge_->ShouldUseChannelSettings()),
      weak_factory_(this) {}

NotificationChannelsProviderAndroid::~NotificationChannelsProviderAndroid() =
    default;

std::unique_ptr<content_settings::RuleIterator>
NotificationChannelsProviderAndroid::GetRuleIterator(
    ContentSettingsType content_type,
    const content_settings::ResourceIdentifier& resource_identifier,
    bool incognito) const {
  if (content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS || incognito ||
      !should_use_channels_) {
    return nullptr;
  }
  std::vector<NotificationChannel> channels = bridge_->GetChannels();
  std::sort(channels.begin(), channels.end());
  if (channels != cached_channels_) {
    // This const_cast is not ideal but tolerated because it doesn't change the
    // underlying state of NotificationChannelsProviderAndroid, and allows us to
    // notify observers as soon as we detect changes to channels.
    auto* provider = const_cast<NotificationChannelsProviderAndroid*>(this);
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(
                &NotificationChannelsProviderAndroid::NotifyObservers,
                provider->weak_factory_.GetWeakPtr(), ContentSettingsPattern(),
                ContentSettingsPattern(), content_type, std::string()));
    provider->cached_channels_ = channels;
  }
  return channels.empty()
             ? nullptr
             : base::MakeUnique<ChannelsRuleIterator>(std::move(channels));
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
  // TODO(awdf): Maybe update cached_channels before notifying here, to
  // avoid notifying observers unnecessarily from GetRuleIterator.
  NotifyObservers(primary_pattern, secondary_pattern, content_type,
                  resource_identifier);
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
  // TODO(awdf): Maybe check cached incognito status here to make sure
  // channels are never created in incognito mode.
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
