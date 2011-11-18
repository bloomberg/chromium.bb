// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification_manager.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/protocol/app_notification_specifics.pb.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

typedef std::map<std::string, SyncData> SyncDataMap;

namespace {

class GuidComparator
    : public std::binary_function<linked_ptr<AppNotification>,
                                  std::string,
                                  bool> {
 public:
  bool operator() (linked_ptr<AppNotification> notif,
                   const std::string& guid) const {
    return notif->guid() == guid;
  }
};

void DeleteStorageOnFileThread(AppNotificationStorage* storage) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  delete storage;
}

const AppNotification* FindByGuid(const AppNotificationList& list,
                                  const std::string& guid) {
  AppNotificationList::const_iterator iter = std::find_if(
      list.begin(), list.end(), std::bind2nd(GuidComparator(), guid));
  return iter == list.end() ? NULL : iter->get();
}

void RemoveByGuid(AppNotificationList* list, const std::string& guid) {
  if (!list)
    return;

  AppNotificationList::iterator iter = std::find_if(
      list->begin(), list->end(), std::bind2nd(GuidComparator(), guid));
  if (iter != list->end())
    list->erase(iter);
}

void PopulateGuidToSyncDataMap(const SyncDataList& sync_data,
                               SyncDataMap* data_map) {
  for (SyncDataList::const_iterator iter = sync_data.begin();
       iter != sync_data.end(); ++iter) {
    (*data_map)[iter->GetSpecifics().GetExtension(
        sync_pb::app_notification).guid()] = *iter;
  }
}
}  // namespace

const unsigned int AppNotificationManager::kMaxNotificationPerApp = 5;

AppNotificationManager::AppNotificationManager(Profile* profile)
    : profile_(profile),
      sync_processor_(NULL),
      models_associated_(false),
      processing_syncer_changes_(false) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

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
      base::Bind(&AppNotificationManager::LoadOnFileThread,
          this, storage_path));
}

bool AppNotificationSortPredicate(const linked_ptr<AppNotification> a1,
                                  const linked_ptr<AppNotification> a2) {
  return a1.get()->creation_time() < a2.get()->creation_time();
}

bool AppNotificationManager::Add(AppNotification* item) {
  // Do this first since we own the incoming item and hence want to delete
  // it in error paths.
  linked_ptr<AppNotification> linked_item(item);
  if (!loaded())
    return false;
  const std::string& extension_id = item->extension_id();
  AppNotificationList& list = GetAllInternal(extension_id);
  list.push_back(linked_item);

  SyncAddChange(*linked_item);

  sort(list.begin(), list.end(), AppNotificationSortPredicate);

  if (list.size() > AppNotificationManager::kMaxNotificationPerApp) {
    AppNotification* removed = list.begin()->get();
    SyncRemoveChange(*removed);
    list.erase(list.begin());
  }

  if (storage_.get()) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&AppNotificationManager::SaveOnFileThread,
            this, extension_id, CopyAppNotificationList(list)));
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
      content::Source<Profile>(profile_),
      content::Details<const std::string>(&extension_id));

  return true;
}

const AppNotificationList* AppNotificationManager::GetAll(
    const std::string& extension_id) const {
  if (!loaded())
    return NULL;
  if (ContainsKey(*notifications_, extension_id))
    return &((*notifications_)[extension_id]);
  return NULL;
}

AppNotificationList& AppNotificationManager::GetAllInternal(
    const std::string& extension_id) {
  NotificationMap::iterator found = notifications_->find(extension_id);
  if (found == notifications_->end()) {
    (*notifications_)[extension_id] = AppNotificationList();
    found = notifications_->find(extension_id);
  }
  CHECK(found != notifications_->end());
  return found->second;
}

const AppNotification* AppNotificationManager::GetLast(
    const std::string& extension_id) {
  if (!loaded())
    return NULL;
  NotificationMap::iterator found = notifications_->find(extension_id);
  if (found == notifications_->end())
    return NULL;
  const AppNotificationList& list = found->second;
  if (list.empty())
    return NULL;
  return list.rbegin()->get();
}

void AppNotificationManager::ClearAll(const std::string& extension_id) {
  if (!loaded())
    return;
  NotificationMap::iterator found = notifications_->find(extension_id);
  if (found != notifications_->end()) {
    SyncClearAllChange(found->second);
    notifications_->erase(found);
  }

  if (storage_.get()) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&AppNotificationManager::DeleteOnFileThread,
            this, extension_id));
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
      content::Source<Profile>(profile_),
      content::Details<const std::string>(&extension_id));
}

void AppNotificationManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  CHECK(type == chrome::NOTIFICATION_EXTENSION_UNINSTALLED);
  ClearAll(*content::Details<const std::string>(details).ptr());
}

void AppNotificationManager::LoadOnFileThread(const FilePath& storage_path) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!loaded());
  storage_.reset(AppNotificationStorage::Create(storage_path));
  if (!storage_.get())
    return;
  scoped_ptr<NotificationMap> result(new NotificationMap());
  std::set<std::string> ids;
  if (!storage_->GetExtensionIds(&ids))
    return;
  std::set<std::string>::const_iterator i;
  for (i = ids.begin(); i != ids.end(); ++i) {
    const std::string& id = *i;
    AppNotificationList& list = (*result)[id];
    if (!storage_->Get(id, &list))
      result->erase(id);
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AppNotificationManager::HandleLoadResults,
          this, result.release()));
}

void AppNotificationManager::HandleLoadResults(NotificationMap* map) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(map);
  DCHECK(!loaded());
  notifications_.reset(map);

  // Generate STATE_CHAGNED notifications for extensions that have at
  // least one notification loaded.
  NotificationMap::const_iterator i;
  for (i = map->begin(); i != map->end(); ++i) {
    const std::string& id = i->first;
    if (i->second.empty())
      continue;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
        content::Source<Profile>(profile_),
        content::Details<const std::string>(&id));
  }

  // Generate MANAGER_LOADED notification.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_NOTIFICATION_MANAGER_LOADED,
      content::Source<AppNotificationManager>(this),
      content::NotificationService::NoDetails());
}

void AppNotificationManager::SaveOnFileThread(const std::string& extension_id,
                                              AppNotificationList* list) {
  // Own the |list|.
  scoped_ptr<AppNotificationList> scoped_list(list);
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  storage_->Set(extension_id, *scoped_list);
}

void AppNotificationManager::DeleteOnFileThread(
    const std::string& extension_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  storage_->Delete(extension_id);
}

SyncDataList AppNotificationManager::GetAllSyncData(
    syncable::ModelType type) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(loaded());
  DCHECK_EQ(syncable::APP_NOTIFICATIONS, type);
  SyncDataList data;
  for (NotificationMap::const_iterator iter = notifications_->begin();
      iter != notifications_->end(); ++iter) {

    // Skip local notifications since they should not be synced.
    const AppNotificationList list = (*iter).second;
    for (AppNotificationList::const_iterator list_iter = list.begin();
        list_iter != list.end(); ++list_iter) {
      const AppNotification* notification = (*list_iter).get();
      if (notification->is_local()) {
        continue;
      }
      data.push_back(CreateSyncDataFromNotification(*notification));
    }
  }

  return data;
}

SyncError AppNotificationManager::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(loaded());
  if (!models_associated_)
    return SyncError(FROM_HERE, "Models not yet associated.",
                     syncable::APP_NOTIFICATIONS);

  AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  SyncError error;
  for (SyncChangeList::const_iterator iter = change_list.begin();
       iter != change_list.end(); ++iter) {
    SyncData sync_data = iter->sync_data();
    DCHECK_EQ(syncable::APP_NOTIFICATIONS, sync_data.GetDataType());
    SyncChange::SyncChangeType change_type = iter->change_type();

    scoped_ptr<AppNotification> new_notif(CreateNotificationFromSyncData(
        sync_data));
    if (!new_notif.get()) {
      NOTREACHED() << "Failed to read notification.";
      continue;
    }
    const AppNotification* existing_notif = GetNotification(
        new_notif->extension_id(), new_notif->guid());
    if (existing_notif && existing_notif->is_local()) {
      NOTREACHED() << "Matched with notification marked as local";
      error = SyncError(FROM_HERE,
          "ProcessSyncChanges received a local only notification" +
          SyncChange::ChangeTypeToString(change_type),
          syncable::APP_NOTIFICATIONS);
      continue;
    }

    if (change_type == SyncChange::ACTION_ADD && !existing_notif) {
      Add(new_notif.release());
    } else if (change_type == SyncChange::ACTION_DELETE && existing_notif) {
      Remove(new_notif->extension_id(), new_notif->guid());
    } else {
      // Something really unexpected happened. Either we received an
      // ACTION_INVALID, or Sync is in a crazy state:
      // - Trying to UPDATE: notifications are immutable.
      // . Trying to DELETE a non-existent item.
      // - Trying to ADD an item that already exists.
      NOTREACHED() << "Unexpected sync change state.";
      error = SyncError(FROM_HERE, "ProcessSyncChanges failed on ChangeType " +
          SyncChange::ChangeTypeToString(change_type),
          syncable::APP_NOTIFICATIONS);
      continue;
    }
  }

  return error;
}

SyncError AppNotificationManager::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    SyncChangeProcessor* sync_processor) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // AppNotificationDataTypeController ensures that modei is fully should before
  // this method is called by waiting until the load notification is received
  // from AppNotificationManager.
  DCHECK(loaded());
  DCHECK_EQ(type, syncable::APP_NOTIFICATIONS);
  DCHECK(!sync_processor_);
  sync_processor_ = sync_processor;

  // We may add, or remove notifications here, so ensure we don't step on
  // our own toes.
  AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  SyncDataMap local_data_map;
  PopulateGuidToSyncDataMap(GetAllSyncData(syncable::APP_NOTIFICATIONS),
                            &local_data_map);

  for (SyncDataList::const_iterator iter = initial_sync_data.begin();
       iter != initial_sync_data.end(); ++iter) {
    const SyncData& sync_data = *iter;
    DCHECK_EQ(syncable::APP_NOTIFICATIONS, sync_data.GetDataType());
    scoped_ptr<AppNotification> sync_notif(CreateNotificationFromSyncData(
        sync_data));
    CHECK(sync_notif.get());
    const AppNotification* local_notif = GetNotification(
        sync_notif->extension_id(), sync_notif->guid());
    if (local_notif) {
      local_data_map.erase(sync_notif->guid());
      // Local notification should always match with sync notification as
      // notifications are immutable.
      if (local_notif->is_local() || !sync_notif->Equals(*local_notif)) {
        return SyncError(FROM_HERE,
            "MergeDataAndStartSyncing failed: local notification and sync "
            "notification have same guid but different data.",
            syncable::APP_NOTIFICATIONS);
      }
    } else {
      // Sync model has a notification that local model does not, add it.
      Add(sync_notif.release());
    }
  }

  // TODO(munjal): crbug.com/10059. Work with Lingesh/Antony to resolve.
  SyncChangeList new_changes;
  for (SyncDataMap::const_iterator iter = local_data_map.begin();
      iter != local_data_map.end(); ++iter) {
    new_changes.push_back(SyncChange(SyncChange::ACTION_ADD, iter->second));
  }

  SyncError error;
  if (new_changes.size() > 0)
    error = sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  models_associated_ = !error.IsSet();
  return error;
}

void AppNotificationManager::StopSyncing(syncable::ModelType type) {
  DCHECK_EQ(type, syncable::APP_NOTIFICATIONS);
  models_associated_ = false;
  sync_processor_ = NULL;
}

void AppNotificationManager::SyncAddChange(const AppNotification& notif) {
  // Skip if either:
  // - Notification is marked as local.
  // - Sync is not enabled by user.
  // - Change is generated from within the manager.
  if (notif.is_local() || !models_associated_ || processing_syncer_changes_)
    return;

  // TODO(munjal): crbug.com/10059. Work with Lingesh/Antony to resolve.

  SyncChangeList changes;
  SyncData sync_data = CreateSyncDataFromNotification(notif);
  changes.push_back(SyncChange(SyncChange::ACTION_ADD, sync_data));
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void AppNotificationManager::SyncRemoveChange(const AppNotification& notif) {
  // Skip if either:
  // - Sync is not enabled by user.
  // - Change is generated from within the manager.
  if (notif.is_local() || !models_associated_) {
    return;
  }

  SyncChangeList changes;
  SyncData sync_data = CreateSyncDataFromNotification(notif);
  changes.push_back(SyncChange(SyncChange::ACTION_DELETE, sync_data));
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void AppNotificationManager::SyncClearAllChange(
  const AppNotificationList& list) {
  // Skip if either:
  // - Sync is not enabled by user.
  // - Change is generated from within the manager.
  if (!models_associated_ || processing_syncer_changes_)
    return;

  SyncChangeList changes;
  for (AppNotificationList::const_iterator iter = list.begin();
      iter != list.end(); ++iter) {
    const AppNotification& notif = *iter->get();
    // Skip notifications marked as local.
    if (notif.is_local())
      continue;
    changes.push_back(SyncChange(
        SyncChange::ACTION_DELETE,
        CreateSyncDataFromNotification(notif)));
  }
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

const AppNotification* AppNotificationManager::GetNotification(
    const std::string& extension_id, const std::string& guid) {
  DCHECK(loaded());
  const AppNotificationList& list = GetAllInternal(extension_id);
  return FindByGuid(list, guid);
}

void AppNotificationManager::Remove(const std::string& extension_id,
                                    const std::string& guid) {
  DCHECK(loaded());
  AppNotificationList& list = GetAllInternal(extension_id);
  RemoveByGuid(&list, guid);

  if (storage_.get()) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&AppNotificationManager::SaveOnFileThread,
            this, extension_id, CopyAppNotificationList(list)));
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
      content::Source<Profile>(profile_),
      content::Details<const std::string>(&extension_id));
}

// static
SyncData AppNotificationManager::CreateSyncDataFromNotification(
    const AppNotification& notification) {
  DCHECK(!notification.is_local());
  sync_pb::EntitySpecifics specifics;
  sync_pb::AppNotificationSpecifics* notif_specifics =
      specifics.MutableExtension(sync_pb::app_notification);
  notif_specifics->set_app_id(notification.extension_id());
  notif_specifics->set_creation_timestamp_ms(
      notification.creation_time().ToInternalValue());
  notif_specifics->set_body_text(notification.body());
  notif_specifics->set_guid(notification.guid());
  notif_specifics->set_link_text(notification.link_text());
  notif_specifics->set_link_url(notification.link_url().spec());
  notif_specifics->set_title(notification.title());
  return SyncData::CreateLocalData(
      notif_specifics->guid(), notif_specifics->app_id(), specifics);
}

// static
AppNotification* AppNotificationManager::CreateNotificationFromSyncData(
    const SyncData& sync_data) {
  sync_pb::AppNotificationSpecifics specifics =
      sync_data.GetSpecifics().GetExtension(sync_pb::app_notification);

  // Check for mandatory fields.
  if (!specifics.has_app_id() || !specifics.has_guid() ||
      !specifics.has_title() || !specifics.has_body_text() ||
      !specifics.has_creation_timestamp_ms()) {
    return NULL;
  }

  AppNotification* notification = new AppNotification(
      false, base::Time::FromInternalValue(specifics.creation_timestamp_ms()),
      specifics.guid(), specifics.app_id(),
      specifics.title(), specifics.body_text());
  if (specifics.has_link_text())
    notification->set_link_text(specifics.link_text());
  if (specifics.has_link_url())
    notification->set_link_url(GURL(specifics.link_url()));
  return notification;
}
