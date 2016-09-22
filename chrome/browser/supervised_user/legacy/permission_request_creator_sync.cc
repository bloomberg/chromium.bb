// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/permission_request_creator_sync.h"

#include <utility>

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "components/browser_sync/profile_sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "url/gurl.h"

const char kSupervisedUserAccessRequestKeyPrefix[] =
    "X-ManagedUser-AccessRequests";
const char kSupervisedUserInstallRequestKeyPrefix[] =
    "X-ManagedUser-InstallRequests";
const char kSupervisedUserUpdateRequestKeyPrefix[] =
    "X-ManagedUser-UpdateRequests";
const char kSupervisedUserAccessRequestTime[] = "timestamp";
const char kSupervisedUserName[] = "name";

// Key for the notification setting of the custodian. This is a shared setting
// so we can include the setting in the access request data that is used to
// trigger notifications.
const char kNotificationSetting[] = "custodian-notification-setting";

PermissionRequestCreatorSync::PermissionRequestCreatorSync(
    SupervisedUserSettingsService* settings_service,
    SupervisedUserSharedSettingsService* shared_settings_service,
    browser_sync::ProfileSyncService* sync_service,
    const std::string& name,
    const std::string& supervised_user_id)
    : settings_service_(settings_service),
      shared_settings_service_(shared_settings_service),
      sync_service_(sync_service),
      name_(name),
      supervised_user_id_(supervised_user_id) {}

PermissionRequestCreatorSync::~PermissionRequestCreatorSync() {}

bool PermissionRequestCreatorSync::IsEnabled() const {
  GoogleServiceAuthError::State state = sync_service_->GetAuthError().state();
  // We allow requesting access if Sync is working or has a transient error.
  return (state == GoogleServiceAuthError::NONE ||
          state == GoogleServiceAuthError::CONNECTION_FAILED ||
          state == GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

void PermissionRequestCreatorSync::CreateURLAccessRequest(
    const GURL& url_requested,
    const SuccessCallback& callback) {
  CreateRequest(kSupervisedUserAccessRequestKeyPrefix,
                net::EscapeQueryParamValue(url_requested.spec(), true),
                callback);
}

void PermissionRequestCreatorSync::CreateExtensionInstallRequest(
    const std::string& id,
    const SuccessCallback& callback) {
  CreateRequest(kSupervisedUserInstallRequestKeyPrefix, id, callback);
}

void PermissionRequestCreatorSync::CreateExtensionUpdateRequest(
    const std::string& id,
    const SuccessCallback& callback) {
  CreateRequest(kSupervisedUserUpdateRequestKeyPrefix, id, callback);
}

void PermissionRequestCreatorSync::CreateRequest(
    const std::string& prefix,
    const std::string& data,
    const SuccessCallback& callback) {
  // Add the prefix.
  std::string key =
      SupervisedUserSettingsService::MakeSplitSettingKey(prefix, data);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
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

  settings_service_->UploadItem(key, std::move(dict));

  callback.Run(true);
}
