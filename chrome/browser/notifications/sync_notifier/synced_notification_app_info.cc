// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info.h"

#include "sync/api/sync_data.h"
#include "sync/protocol/synced_notification_app_info_specifics.pb.h"

namespace notifier {

SyncedNotificationAppInfo::SyncedNotificationAppInfo(
    const std::string& settings_display_name)
    : settings_display_name_(settings_display_name) {}

SyncedNotificationAppInfo::~SyncedNotificationAppInfo() {}

bool SyncedNotificationAppInfo::HasAppId(const std::string& app_id) {
  std::vector<std::string>::iterator it;

  for (it = app_ids_.begin(); it != app_ids_.end(); ++it) {
    if (app_id == *it)
      return true;
  }

  return false;
}

void SyncedNotificationAppInfo::AddAppId(const std::string& app_id) {
  if (HasAppId(app_id))
    return;

  app_ids_.push_back(app_id);
}

void SyncedNotificationAppInfo::RemoveAppId(const std::string& app_id) {
  std::vector<std::string>::iterator it;

  for (it = app_ids_.begin(); it != app_ids_.end(); ++it) {
    if (app_id == *it) {
      app_ids_.erase(it);
      return;
    }
  }
}

void SyncedNotificationAppInfo::GetAppIdList(
    std::vector<std::string>* app_id_list) {
  if (app_id_list == NULL)
    return;

  std::vector<std::string>::iterator it;
  for (it = app_ids_.begin(); it != app_ids_.end(); ++it) {
    app_id_list->push_back(*it);
  }
}

}  // namespace notifier
