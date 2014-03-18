// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The SyncedNotificationAppInfoService brings down read only metadata from the
// sync server with information about the services sending synced notifications.

#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_app_info_specifics.pb.h"
#include "url/gurl.h"

namespace notifier {

SyncedNotificationAppInfoService::SyncedNotificationAppInfoService(
    Profile* profile)
    : profile_(profile) {}

SyncedNotificationAppInfoService::~SyncedNotificationAppInfoService() {}

// Methods from KeyedService.
void SyncedNotificationAppInfoService::Shutdown() {}

// syncer::SyncableService implementation.

// This is called at startup to sync with the sync data. This code is not thread
// safe, it should only be called by sync on the browser thread.
syncer::SyncMergeResult
SyncedNotificationAppInfoService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  thread_checker_.CalledOnValidThread();
  DCHECK_EQ(syncer::SYNCED_NOTIFICATION_APP_INFO, type);
  syncer::SyncMergeResult merge_result(syncer::SYNCED_NOTIFICATION_APP_INFO);

  // There should only be one sync data in the list for this data type.
  if (initial_sync_data.size() > 1) {
    LOG(ERROR) << "Too many app infos over sync";
  }

  // TODO(petewil): Today we can only handle a single object, so we simply check
  // for a non-empty list.  If in the future we can ever handle more, convert
  // this whole block to be a loop over all of |initial_sync_data|.
  if (!initial_sync_data.empty()) {
    const syncer::SyncData& sync_data = initial_sync_data.front();
    DCHECK_EQ(syncer::SYNCED_NOTIFICATION_APP_INFO, sync_data.GetDataType());

    const sync_pb::SyncedNotificationAppInfoSpecifics& specifics =
        sync_data.GetSpecifics().synced_notification_app_info();

    // Store our sync data, so GetAllSyncData can give it back later.
    sync_data_ = sync_data;

    size_t app_info_count = specifics.synced_notification_app_info_size();

    // The SyncedNotificationAppInfo is a repeated field, process each one.
    for (size_t app_info_index = 0;
         app_info_index < app_info_count;
         ++app_info_index) {
      const sync_pb::SyncedNotificationAppInfo app_info(
          specifics.synced_notification_app_info(app_info_index));

      ProcessIncomingAppInfoProtobuf(app_info);
    }
  }

  return merge_result;
}

void SyncedNotificationAppInfoService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(syncer::SYNCED_NOTIFICATION_APP_INFO, type);
  // Implementation is not required, since this is not a user selectable sync
  // type.
}

// This method is called when there is an incoming sync change from the server.
syncer::SyncError SyncedNotificationAppInfoService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  thread_checker_.CalledOnValidThread();
  syncer::SyncError error;

  for (syncer::SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end();
       ++it) {
    syncer::SyncData sync_data = it->sync_data();
    DCHECK_EQ(syncer::SYNCED_NOTIFICATION_APP_INFO, sync_data.GetDataType());
    syncer::SyncChange::SyncChangeType change_type = it->change_type();
    DCHECK(change_type == syncer::SyncChange::ACTION_UPDATE ||
           change_type == syncer::SyncChange::ACTION_ADD);

    sync_pb::SyncedNotificationAppInfoSpecifics specifics =
        sync_data.GetSpecifics().synced_notification_app_info();

    // Copy over the sync data with the new one.
    sync_data_ = sync_data;

    size_t app_info_count = specifics.synced_notification_app_info_size();
    if (app_info_count == 0) {
      NOTREACHED() << "Bad notification app info change from the server.";
      continue;
    }

    // The SyncedNotificationAppInfo is a repeated field, process each one.
    for (size_t app_info_index = 0;
         app_info_index < app_info_count;
         ++app_info_index) {
      const sync_pb::SyncedNotificationAppInfo app_info(
          specifics.synced_notification_app_info(app_info_index));
      ProcessIncomingAppInfoProtobuf(app_info);
    }
  }

  return error;
}

syncer::SyncDataList SyncedNotificationAppInfoService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(syncer::SYNCED_NOTIFICATION_APP_INFO, type);
  syncer::SyncDataList sync_data_list;

  // TODO(petewil): Recreate this from internal data instead,
  // it improves code maintainability.

  // make a copy for the list we give back to the caller.
  sync_data_list.push_back(sync_data_);

  return sync_data_list;
}

void SyncedNotificationAppInfoService::ProcessIncomingAppInfoProtobuf(
    const sync_pb::SyncedNotificationAppInfo& app_info) {
  // Build a local app_info object from the sync data.
  scoped_ptr<SyncedNotificationAppInfo> incoming(
      CreateSyncedNotificationAppInfoFromProtobuf(app_info));
  DCHECK(incoming.get());

  // Process each incoming app_info protobuf.
  const std::string& name = incoming->settings_display_name();
  DCHECK_GT(name.length(), 0U);
  if (name.length() == 0) {
    // If there is no unique id (name), there is nothing we can do.
    return;
  }

  SyncedNotificationAppInfo* found = FindSyncedNotificationAppInfoByName(name);

  if (NULL != found) {
    // When we have an update, some app id types may be added or removed.
    // Append to lists of added and removed types.
    FreeSyncedNotificationAppInfoByName(name);
  }

  sending_service_infos_.push_back(incoming.release());

  // Tell the Chrome Notifier Service so it can show any notifications that were
  // waiting for the app id to arrive, and to remave any notifications that are
  // no longer supported.
  // TODO(petewil): Notify CNS of added ids
  // TODO(petewil): Notify CNS of deleted ids.
}

// Static Method.  Convert from a server protobuf to our internal format.
scoped_ptr<SyncedNotificationAppInfo>
SyncedNotificationAppInfoService::CreateSyncedNotificationAppInfoFromProtobuf(
    const sync_pb::SyncedNotificationAppInfo& server_app_info) {

  // Check for mandatory fields in the sync_data object.
  std::string display_name;
  if (server_app_info.has_settings_display_name()) {
    display_name = server_app_info.settings_display_name();
  }
  scoped_ptr<SyncedNotificationAppInfo> app_info;
  if (display_name.length() == 0)
    return app_info.Pass();

  // Create a new app info object based on the supplied protobuf.
  app_info.reset(new SyncedNotificationAppInfo(display_name));

  // TODO(petewil): Eventually we will add the monochrome icon here, and we may
  // need to fetch the correct url for the current DPI.
  // Add the icon URL, if any.
  if (server_app_info.has_icon()) {
    std::string icon_url = server_app_info.icon().url();
    app_info->SetSettingsIcon(GURL(icon_url));
  }

  // Add all the AppIds from the protobuf.
  size_t app_id_count = server_app_info.app_id_size();
  for (size_t ii = 0; ii < app_id_count; ++ii) {
    app_info->AddAppId(server_app_info.app_id(ii));
  }

  return app_info.Pass();
}

// This returns a pointer into a vector that we own.  Caller must not free it.
// Returns NULL if no match is found.
notifier::SyncedNotificationAppInfo*
SyncedNotificationAppInfoService::FindSyncedNotificationAppInfoByName(
    const std::string& name) {
  for (ScopedVector<SyncedNotificationAppInfo>::const_iterator it =
           sending_service_infos_.begin();
       it != sending_service_infos_.end();
       ++it) {
    SyncedNotificationAppInfo* app_info = *it;
    if (name == app_info->settings_display_name())
      return *it;
  }

  return NULL;
}

void SyncedNotificationAppInfoService::FreeSyncedNotificationAppInfoByName(
    const std::string& name) {
  ScopedVector<SyncedNotificationAppInfo>::iterator it =
      sending_service_infos_.begin();
  for (; it != sending_service_infos_.end(); ++it) {
    SyncedNotificationAppInfo* app_info = *it;
    if (name == app_info->settings_display_name()) {
      sending_service_infos_.erase(it);
      return;
    }
  }
}

// Add a new app info to our data structure.  This takes ownership
// of the passed in pointer.
void SyncedNotificationAppInfoService::Add(
    scoped_ptr<SyncedNotificationAppInfo> app_info) {
  sending_service_infos_.push_back(app_info.release());
}

}  // namespace notifier
