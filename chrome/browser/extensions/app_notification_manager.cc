// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification_manager.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

AppNotificationManager::AppNotificationManager(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 Source<Profile>(profile_));
}

namespace {

void DeleteStorageOnFileThread(AppNotificationStorage* storage) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  delete storage;
}

}  // namespace

AppNotificationManager::~AppNotificationManager() {
  // Post a task to delete our storage on the file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&DeleteStorageOnFileThread, storage_.release()));
}

void AppNotificationManager::Init() {
  FilePath storage_path = profile_->GetPath().AppendASCII("App Notifications");
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &AppNotificationManager::LoadOnFileThread, this, storage_path));
}

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

  if (!storage_.get())
    return;

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &AppNotificationManager::SaveOnFileThread, this, extension_id, list));
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

  if (!storage_.get())
    return;
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &AppNotificationManager::DeleteOnFileThread, this, extension_id));
}

void AppNotificationManager::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  CHECK(type == chrome::NOTIFICATION_EXTENSION_UNINSTALLED);
  const std::string& id =
      Details<UninstalledExtensionInfo>(details)->extension_id;
  ClearAll(id);
}

void AppNotificationManager::LoadOnFileThread(const FilePath& storage_path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  storage_.reset(AppNotificationStorage::Create(storage_path));
  if (!storage_.get())
    return;
  NotificationMap result;
  std::set<std::string> ids;
  if (!storage_->GetExtensionIds(&ids))
    return;
  std::set<std::string>::const_iterator i;
  for (i = ids.begin(); i != ids.end(); ++i) {
    const std::string& id = *i;
    AppNotificationList& list = result[id];
    if (!storage_->Get(id, &list))
      result.erase(id);
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AppNotificationManager::HandleLoadResults, this, result));
}

void AppNotificationManager::HandleLoadResults(const NotificationMap& map) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NotificationMap::const_iterator i;
  for (i = map.begin(); i != map.end(); ++i) {
    const std::string& id = i->first;
    const AppNotificationList& list = i->second;
    if (list.empty())
      continue;
    notifications_[id].insert(notifications_[id].begin(),
                              list.begin(),
                              list.end());
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
        Source<Profile>(profile_),
        Details<const std::string>(&id));
  }
}

void AppNotificationManager::SaveOnFileThread(const std::string& extension_id,
                                              const AppNotificationList& list) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  storage_->Set(extension_id, list);
}

void AppNotificationManager::DeleteOnFileThread(
    const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  storage_->Delete(extension_id);
}
