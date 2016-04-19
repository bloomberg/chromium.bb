// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_supervised_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"

namespace {

struct ContentSettingsFromSupervisedSettingsEntry {
  const char* setting_name;
  ContentSettingsType content_type;
};

const ContentSettingsFromSupervisedSettingsEntry
    kContentSettingsFromSupervisedSettingsMap[] = {
  {
    supervised_users::kGeolocationDisabled,
    CONTENT_SETTINGS_TYPE_GEOLOCATION,
  }, {
    supervised_users::kCameraMicDisabled,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
  }, {
    supervised_users::kCameraMicDisabled,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
  }
};

}  // namespace

namespace content_settings {

SupervisedProvider::SupervisedProvider(
    SupervisedUserSettingsService* supervised_user_settings_service) {

  // The SupervisedProvider is owned by the HostContentSettingsMap which
  // DependsOn the SupervisedUserSettingsService (through their factories).
  // This means this will get destroyed before the SUSS and will be
  // unsubscribed from it.
  user_settings_subscription_ = supervised_user_settings_service->Subscribe(
      base::Bind(
          &content_settings::SupervisedProvider::OnSupervisedSettingsAvailable,
          base::Unretained(this)));
}

SupervisedProvider::~SupervisedProvider() {
}

std::unique_ptr<RuleIterator> SupervisedProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  std::unique_ptr<base::AutoLock> auto_lock(new base::AutoLock(lock_));
  return value_map_.GetRuleIterator(content_type, resource_identifier,
                                    std::move(auto_lock));
}

void SupervisedProvider::OnSupervisedSettingsAvailable(
    const base::DictionaryValue* settings) {
  std::vector<ContentSettingsType> to_notify;
  // Entering locked scope to update content settings.
  {
    base::AutoLock auto_lock(lock_);
    for (const auto& entry : kContentSettingsFromSupervisedSettingsMap) {
      bool new_value = false;
      if (settings && settings->HasKey(entry.setting_name)) {
        bool is_bool = settings->GetBoolean(entry.setting_name, &new_value);
        DCHECK(is_bool);
      }
      bool old_value = !value_map_.IsContentSettingEnabled(entry.content_type);
      if (new_value != old_value) {
        to_notify.push_back(entry.content_type);
        value_map_.SetContentSettingDisabled(entry.content_type, new_value);
      }
    }
  }
  for (const auto& notification : to_notify) {
    NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                    notification, std::string());
  }
}

// Since the SupervisedProvider is a read only content settings provider, all
// methods of the ProviderInterface that set or delete any settings do nothing.
bool SupervisedProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* value) {
  return false;
}

void SupervisedProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
}

void SupervisedProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
  user_settings_subscription_.reset();
}

}  // namespace content_settings
