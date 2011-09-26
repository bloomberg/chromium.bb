// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification_manager.h"

#include "base/stl_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/common/notification_service.h"

AppNotificationManager::AppNotificationManager() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 NotificationService::AllSources());
}

AppNotificationManager::~AppNotificationManager() {}

void AppNotificationManager::Add(const std::string& extension_id,
                                 AppNotification* item) {
  NotificationMap::iterator found = notifications_.find(extension_id);
  if (found == notifications_.end()) {
    notifications_[extension_id] = AppNotificationList();
    found = notifications_.find(extension_id);
  }
  CHECK(found != notifications_.end());
  AppNotificationList& list = (*found).second;
  list.push_back(linked_ptr<AppNotification>(item));
}

const AppNotificationList* AppNotificationManager::GetAll(
    const std::string& extension_id) {
  if (ContainsKey(notifications_, extension_id))
    return &notifications_[extension_id];
  return NULL;
}

const AppNotification* AppNotificationManager::GetLast(
    const std::string& extension_id) {
  NotificationMap::iterator found = notifications_.find(extension_id);
  if (found == notifications_.end())
    return NULL;
  const AppNotificationList& list = found->second;
  return list.rbegin()->get();
}

void AppNotificationManager::ClearAll(const std::string& extension_id) {
  NotificationMap::iterator found = notifications_.find(extension_id);
  if (found != notifications_.end())
    notifications_.erase(found);
}

void AppNotificationManager::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  CHECK(type == chrome::NOTIFICATION_EXTENSION_UNINSTALLED);
  const std::string& id =
      Details<UninstalledExtensionInfo>(details)->extension_id;
  ClearAll(id);
}
