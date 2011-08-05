// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_app_api.h"

#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/render_messages.h"
#include "content/common/notification_service.h"


const char kBodyTextKey[] = "bodyText";
const char kExtensionIdKey[] = "extensionId";
const char kIconDataKey[] = "iconData";
const char kLinkTextKey[] = "linkText";
const char kLinkUrlKey[] = "linkUrl";
const char kTitleKey[] = "title";

const char kInvalidExtensionIdError[] =
    "Invalid extension id";
const char kMissingLinkTextError[] =
    "You must specify linkText if you use linkUrl";

AppNotification::AppNotification() {}

AppNotification::~AppNotification() {}

AppNotificationManager::AppNotificationManager() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 NotificationService::AllSources());
}

AppNotificationManager::~AppNotificationManager() {}

void AppNotificationManager::Add(AppNotification* item) {
  CHECK(!item->extension_id.empty());
  NotificationMap::iterator found = notifications_.find(item->extension_id);
  if (found == notifications_.end()) {
    notifications_[item->extension_id] = AppNotificationList();
    found = notifications_.find(item->extension_id);
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

bool AppNotifyFunction::RunImpl() {
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  EXTENSION_FUNCTION_VALIDATE(details != NULL);

  scoped_ptr<AppNotification> item(new AppNotification());

  // TODO(asargent) remove this before the API leaves experimental.
  std::string id = extension_id();
  if (details->HasKey(kExtensionIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kExtensionIdKey, &id));
    if (!profile()->GetExtensionService()->GetExtensionById(id, true)) {
      error_ = kInvalidExtensionIdError;
      return false;
    }
  }

  item->extension_id = id;

  if (details->HasKey(kTitleKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kTitleKey, &item->title));

  if (details->HasKey(kBodyTextKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kBodyTextKey, &item->body));

  if (details->HasKey(kLinkUrlKey)) {
    std::string link_url;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kLinkUrlKey, &link_url));
    item->linkUrl = GURL(link_url);
    if (!item->linkUrl.is_valid()) {
      error_ = "Invalid url: " + link_url;
      return false;
    }
    if (!details->HasKey(kLinkTextKey)) {
      error_ = kMissingLinkTextError;
      return false;
    }
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kLinkTextKey,
                                                   &item->linkText));
  }

  if (details->HasKey(kIconDataKey)) {
    base::BinaryValue* binary = NULL;
    EXTENSION_FUNCTION_VALIDATE(details->GetBinary(kIconDataKey, &binary));
    IPC::Message bitmap_pickle(binary->GetBuffer(), binary->GetSize());
    void* iter = NULL;
    SkBitmap bitmap;
    EXTENSION_FUNCTION_VALIDATE(
        IPC::ReadParam(&bitmap_pickle, &iter, &bitmap));
    // TODO(asargent) - use the bitmap to set the NTP icon!
  }

  AppNotificationManager* manager =
      profile()->GetExtensionService()->app_notification_manager();

  manager->Add(item.release());

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
      Source<Profile>(profile_),
      Details<const std::string>(&id));

  return true;
}

bool AppClearAllNotificationsFunction::RunImpl() {
  std::string id = extension_id();
  DictionaryValue* details = NULL;
  if (args_->GetDictionary(0, &details) && details->HasKey(kExtensionIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kExtensionIdKey, &id));
    if (!profile()->GetExtensionService()->GetExtensionById(id, true)) {
      error_ = kInvalidExtensionIdError;
      return false;
    }
  }

  AppNotificationManager* manager =
      profile()->GetExtensionService()->app_notification_manager();
  manager->ClearAll(id);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
      Source<Profile>(profile_),
      Details<const std::string>(&id));
  return true;
}
