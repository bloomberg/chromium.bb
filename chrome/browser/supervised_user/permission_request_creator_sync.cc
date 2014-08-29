// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/permission_request_creator_sync.h"

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "net/base/escape.h"
#include "url/gurl.h"

using base::Time;

const char kSupervisedUserAccessRequestKeyPrefix[] =
    "X-ManagedUser-AccessRequests";
const char kSupervisedUserAccessRequestTime[] = "timestamp";
const char kSupervisedUserName[] = "name";

// Key for the notification setting of the custodian. This is a shared setting
// so we can include the setting in the access request data that is used to
// trigger notifications.
const char kNotificationSetting[] = "custodian-notification-setting";

PermissionRequestCreatorSync::PermissionRequestCreatorSync(
    SupervisedUserSettingsService* settings_service,
    SupervisedUserSharedSettingsService* shared_settings_service,
    const std::string& name,
    const std::string& supervised_user_id)
    : settings_service_(settings_service),
      shared_settings_service_(shared_settings_service),
      name_(name),
      supervised_user_id_(supervised_user_id) {
}

PermissionRequestCreatorSync::~PermissionRequestCreatorSync() {}

void PermissionRequestCreatorSync::CreatePermissionRequest(
    const GURL& url_requested,
    const base::Closure& callback) {
  // Escape the URL and add the prefix.
  std::string key = SupervisedUserSettingsService::MakeSplitSettingKey(
      kSupervisedUserAccessRequestKeyPrefix,
      net::EscapeQueryParamValue(url_requested.spec(), true));

  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);

  // TODO(sergiu): Use sane time here when it's ready.
  dict->SetDouble(kSupervisedUserAccessRequestTime,
                  base::Time::Now().ToJsTime());

  dict->SetString(kSupervisedUserName, name_);

  // Copy the notification setting of the custodian.
  const base::Value* value = shared_settings_service_->GetValue(
      supervised_user_id_, kNotificationSetting);
  bool notifications_enabled = false;
  if (value) {
    bool success = value->GetAsBoolean(&notifications_enabled);
    DCHECK(success);
  }
  dict->SetBoolean(kNotificationSetting, notifications_enabled);

  settings_service_->UploadItem(key, dict.PassAs<base::Value>());

  callback.Run();
}
