// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification_manager.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/perftimer.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/app_notification_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

using content::BrowserThread;

typedef std::map<std::string, syncer::SyncData> SyncDataMap;

namespace extensions {

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

void PopulateGuidToSyncDataMap(const syncer::SyncDataList& sync_data,
                               SyncDataMap* data_map) {
  for (syncer::SyncDataList::const_iterator iter = sync_data.begin();
       iter != sync_data.end(); ++iter) {
    (*data_map)[iter->GetSpecifics().app_notification().guid()] = *iter;
  }
}
}  // namespace

const unsigned int AppNotificationManager::kMaxNotificationPerApp = 5;

AppNotificationManager::AppNotificationManager(Profile* profile)
    : profile_(profile),
      models_associated_(false),
      processing_syncer_changes_(false) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

void AppNotificationManager::Init() {
  base::FilePath storage_path =
      profile_->GetPath().AppendASCII("App Notifications");
  load_timer_.reset(new PerfTimer());
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

  std::sort(list.begin(), list.end(), AppNotificationSortPredicate);

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
  ClearAll(content::Details<const Extension>(details).ptr()->id());
}

syncer::SyncDataList AppNotificationManager::GetAllSyncData(
    syncer::ModelType type) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(loaded());
  DCHECK_EQ(syncer::APP_NOTIFICATIONS, type);
  syncer::SyncDataList data;
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

syncer::SyncError AppNotificationManager::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(loaded());
  if (!models_associated_) {
    return sync_error_factory_->CreateAndUploadError(
        FROM_HERE,
        "Models not yet associated.");
  }

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  syncer::SyncError error;
  for (syncer::SyncChangeList::const_iterator iter = change_list.begin();
       iter != change_list.end(); ++iter) {
    syncer::SyncData sync_data = iter->sync_data();
    DCHECK_EQ(syncer::APP_NOTIFICATIONS, sync_data.GetDataType());
    syncer::SyncChange::SyncChangeType change_type = iter->change_type();

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
      error = sync_error_factory_->CreateAndUploadError(
          FROM_HERE,
          "ProcessSyncChanges received a local only notification" +
              syncer::SyncChange::ChangeTypeToString(change_type));
      continue;
    }

    switch (change_type) {
      case syncer::SyncChange::ACTION_ADD:
        if (!existing_notif) {
          Add(new_notif.release());
        } else {
          DLOG(ERROR) << "Got ADD change for an existing item.\n"
                      << "Existing item: " << existing_notif->ToString()
                      << "\nItem in ADD change: " << new_notif->ToString();
        }
        break;
      case syncer::SyncChange::ACTION_DELETE:
        if (existing_notif) {
          Remove(new_notif->extension_id(), new_notif->guid());
        } else {
          // This should never happen. But we are seeting this sometimes, and
          // it stops all of sync. See bug http://crbug.com/108088
          // So until we figure out the root cause, log an error and ignore.
          DLOG(ERROR) << "Got DELETE change for non-existing item.\n"
                      << "Item in DELETE change: " << new_notif->ToString();
        }
        break;
      case syncer::SyncChange::ACTION_UPDATE:
        // Although app notifications are immutable from the model perspective,
        // sync can send UPDATE changes due to encryption / meta-data changes.
        // So ignore UPDATE changes when the exitsing and new notification
        // objects are the same. Log an error otherwise.
        if (!existing_notif) {
          DLOG(ERROR) << "Got UPDATE change for non-existing item."
                      << "Item in UPDATE change: " << new_notif->ToString();
        } else if (!existing_notif->Equals(*new_notif)) {
          DLOG(ERROR) << "Got invalid UPDATE change:"
                      << "New and existing notifications should be the same.\n"
                      << "Existing item: " << existing_notif->ToString() << "\n"
                      << "Item in UPDATE change: " << new_notif->ToString();
        }
        break;
      default:
        break;
    }
  }

  return error;
}

syncer::SyncMergeResult AppNotificationManager::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  syncer::SyncMergeResult merge_result(type);
  // AppNotificationDataTypeController ensures that modei is fully should before
  // this method is called by waiting until the load notification is received
  // from AppNotificationManager.
  DCHECK(loaded());
  DCHECK_EQ(type, syncer::APP_NOTIFICATIONS);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());
  sync_processor_ = sync_processor.Pass();
  sync_error_factory_ = sync_error_factory.Pass();

  // We may add, or remove notifications here, so ensure we don't step on
  // our own toes.
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  SyncDataMap local_data_map;
  PopulateGuidToSyncDataMap(GetAllSyncData(syncer::APP_NOTIFICATIONS),
                            &local_data_map);

  for (syncer::SyncDataList::const_iterator iter = initial_sync_data.begin();
       iter != initial_sync_data.end(); ++iter) {
    const syncer::SyncData& sync_data = *iter;
    DCHECK_EQ(syncer::APP_NOTIFICATIONS, sync_data.GetDataType());
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
        merge_result.set_error(sync_error_factory_->CreateAndUploadError(
             FROM_HERE,
            "MergeDataAndStartSyncing failed: local notification and sync "
            "notification have same guid but different data."));
        return merge_result;
      }
    } else {
      // Sync model has a notification that local model does not, add it.
      Add(sync_notif.release());
    }
  }

  // TODO(munjal): crbug.com/10059. Work with Lingesh/Antony to resolve.
  syncer::SyncChangeList new_changes;
  for (SyncDataMap::const_iterator iter = local_data_map.begin();
      iter != local_data_map.end(); ++iter) {
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           iter->second));
  }

  if (new_changes.size() > 0) {
    merge_result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  }
  models_associated_ = !merge_result.error().IsSet();
  return merge_result;
}

void AppNotificationManager::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, syncer::APP_NOTIFICATIONS);
  models_associated_ = false;
  sync_processor_.reset();
  sync_error_factory_.reset();
}

AppNotificationManager::~AppNotificationManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Post a task to delete our storage on the file thread.
  BrowserThread::DeleteSoon(BrowserThread::FILE,
                            FROM_HERE,
                            storage_.release());
}

void AppNotificationManager::LoadOnFileThread(
    const base::FilePath& storage_path) {
  PerfTimer timer;
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

  UMA_HISTOGRAM_LONG_TIMES("AppNotification.MgrFileThreadLoadTime",
                           timer.Elapsed());
}

void AppNotificationManager::HandleLoadResults(NotificationMap* map) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(map);
  DCHECK(!loaded());
  notifications_.reset(map);
  UMA_HISTOGRAM_LONG_TIMES("AppNotification.MgrLoadDelay",
                           load_timer_->Elapsed());
  load_timer_.reset();

  // Generate STATE_CHANGED notifications for extensions that have at
  // least one notification loaded.
  int app_count = 0;
  int notification_count = 0;
  NotificationMap::const_iterator i;
  for (i = map->begin(); i != map->end(); ++i) {
    const std::string& id = i->first;
    if (i->second.empty())
      continue;
    app_count++;
    notification_count += i->second.size();
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_APP_NOTIFICATION_STATE_CHANGED,
        content::Source<Profile>(profile_),
        content::Details<const std::string>(&id));
  }
  UMA_HISTOGRAM_COUNTS("AppNotification.MgrLoadAppCount", app_count);
  UMA_HISTOGRAM_COUNTS("AppNotification.MgrLoadTotalCount",
                       notification_count);

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

const AppNotification* AppNotificationManager::GetNotification(
    const std::string& extension_id, const std::string& guid) {
  DCHECK(loaded());
  const AppNotificationList& list = GetAllInternal(extension_id);
  return FindByGuid(list, guid);
}

void AppNotificationManager::SyncAddChange(const AppNotification& notif) {
  // Skip if either:
  // - Notification is marked as local.
  // - Sync is not enabled by user.
  // - Change is generated from within the manager.
  if (notif.is_local() || !models_associated_ || processing_syncer_changes_)
    return;

  // TODO(munjal): crbug.com/10059. Work with Lingesh/Antony to resolve.

  syncer::SyncChangeList changes;
  syncer::SyncData sync_data = CreateSyncDataFromNotification(notif);
  changes.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         sync_data));
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void AppNotificationManager::SyncRemoveChange(const AppNotification& notif) {
  // Skip if either:
  // - Sync is not enabled by user.
  // - Change is generated from within the manager.
  if (notif.is_local() || !models_associated_) {
    return;
  }

  syncer::SyncChangeList changes;
  syncer::SyncData sync_data = CreateSyncDataFromNotification(notif);
  changes.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_DELETE,
                         sync_data));
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void AppNotificationManager::SyncClearAllChange(
  const AppNotificationList& list) {
  // Skip if either:
  // - Sync is not enabled by user.
  // - Change is generated from within the manager.
  if (!models_associated_ || processing_syncer_changes_)
    return;

  syncer::SyncChangeList changes;
  for (AppNotificationList::const_iterator iter = list.begin();
      iter != list.end(); ++iter) {
    const AppNotification& notif = *iter->get();
    // Skip notifications marked as local.
    if (notif.is_local())
      continue;
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_DELETE,
        CreateSyncDataFromNotification(notif)));
  }
  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

// static
syncer::SyncData AppNotificationManager::CreateSyncDataFromNotification(
    const AppNotification& notification) {
  DCHECK(!notification.is_local());
  sync_pb::EntitySpecifics specifics;
  sync_pb::AppNotification* notif_specifics =
      specifics.mutable_app_notification();
  notif_specifics->set_app_id(notification.extension_id());
  notif_specifics->set_creation_timestamp_ms(
      notification.creation_time().ToInternalValue());
  notif_specifics->set_body_text(notification.body());
  notif_specifics->set_guid(notification.guid());
  notif_specifics->set_link_text(notification.link_text());
  notif_specifics->set_link_url(notification.link_url().spec());
  notif_specifics->set_title(notification.title());
  return syncer::SyncData::CreateLocalData(
      notif_specifics->guid(), notif_specifics->app_id(), specifics);
}

// static
AppNotification* AppNotificationManager::CreateNotificationFromSyncData(
    const syncer::SyncData& sync_data) {
  sync_pb::AppNotification specifics =
      sync_data.GetSpecifics().app_notification();

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

}  // namespace extensions
