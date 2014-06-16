// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/synced_notifications_private/synced_notifications_shim.h"

#include "extensions/browser/event_router.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

using namespace extensions;
using namespace extensions::api;

namespace {

synced_notifications_private::ChangeType SyncerChangeTypeToJS(
    syncer::SyncChange::SyncChangeType change_type) {
  switch (change_type) {
   case syncer::SyncChange::ACTION_UPDATE:
     return synced_notifications_private::CHANGE_TYPE_UPDATED;
   case syncer::SyncChange::ACTION_DELETE:
     return synced_notifications_private::CHANGE_TYPE_DELETED;
   case syncer::SyncChange::ACTION_ADD:
     return synced_notifications_private::CHANGE_TYPE_ADDED;
   case syncer::SyncChange::ACTION_INVALID:
     return synced_notifications_private::CHANGE_TYPE_NONE;
  }
  NOTREACHED();
  return synced_notifications_private::CHANGE_TYPE_NONE;
}

syncer::ModelType JSDataTypeToSyncer(
    synced_notifications_private::SyncDataType data_type) {
  switch (data_type) {
    case synced_notifications_private::SYNC_DATA_TYPE_APP_INFO:
      return syncer::SYNCED_NOTIFICATION_APP_INFO;
    case synced_notifications_private::SYNC_DATA_TYPE_SYNCED_NOTIFICATION:
      return syncer::SYNCED_NOTIFICATIONS;
    default:
      NOTREACHED();
      return syncer::UNSPECIFIED;
  }
}

synced_notifications_private::SyncDataType SyncerModelTypeToJS(
    syncer::ModelType model_type) {
  switch (model_type) {
    case syncer::SYNCED_NOTIFICATION_APP_INFO:
      return synced_notifications_private::SYNC_DATA_TYPE_APP_INFO;
    case syncer::SYNCED_NOTIFICATIONS:
      return synced_notifications_private::SYNC_DATA_TYPE_SYNCED_NOTIFICATION;
    default:
      NOTREACHED();
      return synced_notifications_private::SYNC_DATA_TYPE_NONE;
  }
}

bool BuildNewSyncUpdate(
    const tracked_objects::Location& from_here,
    const std::string& changed_notification,
    syncer::SyncChange* sync_change) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::SyncedNotificationSpecifics* notification_specifics =
      specifics.mutable_synced_notification();
  if (!notification_specifics->ParseFromArray(
          changed_notification.c_str(), changed_notification.size())) {
    return false;
  }

  // TODO(synced notifications): pass the tag via the JS API.
  const std::string& tag =
      notification_specifics->coalesced_notification().key();
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData(tag, tag, specifics);
  *sync_change = syncer::SyncChange(
      from_here, syncer::SyncChange::ACTION_UPDATE, sync_data);
  return true;
}

linked_ptr<synced_notifications_private::SyncChange> BuildNewJSSyncChange(
    const syncer::SyncChange& change) {
  linked_ptr<synced_notifications_private::SyncChange> js_change =
      make_linked_ptr<synced_notifications_private::SyncChange>(
          new synced_notifications_private::SyncChange());
  js_change->change_type = SyncerChangeTypeToJS(change.change_type());
  js_change->data.datatype =
      SyncerModelTypeToJS(change.sync_data().GetDataType());
  if (change.sync_data().GetDataType() == syncer::SYNCED_NOTIFICATIONS) {
    const sync_pb::SyncedNotificationSpecifics& specifics =
        change.sync_data().GetSpecifics().synced_notification();
    js_change->data.data_item = specifics.SerializeAsString();
  } else {
    DCHECK_EQ(change.sync_data().GetDataType(),
              syncer::SYNCED_NOTIFICATION_APP_INFO);
    const sync_pb::SyncedNotificationAppInfoSpecifics& specifics =
        change.sync_data().GetSpecifics().synced_notification_app_info();
    js_change->data.data_item = specifics.SerializeAsString();
  }
  return js_change;
}

bool PopulateJSDataListFromSync(
    const syncer::SyncDataList& sync_data_list,
    std::vector<linked_ptr<synced_notifications_private::SyncData> >*
        js_data_list) {
  for (size_t i = 0; i < sync_data_list.size(); ++i) {
    linked_ptr<synced_notifications_private::SyncData> js_data(
        new synced_notifications_private::SyncData());
    syncer::ModelType data_type = sync_data_list[i].GetDataType();
    js_data->datatype = SyncerModelTypeToJS(data_type);
    if (data_type == syncer::SYNCED_NOTIFICATIONS) {
      const sync_pb::SyncedNotificationSpecifics& specifics =
          sync_data_list[i].GetSpecifics().synced_notification();
      js_data->data_item = specifics.SerializeAsString();
    } else if (data_type == syncer::SYNCED_NOTIFICATION_APP_INFO) {
      const sync_pb::SyncedNotificationAppInfoSpecifics& specifics =
          sync_data_list[i].GetSpecifics().synced_notification_app_info();
      js_data->data_item = specifics.SerializeAsString();
    } else {
      return false;
    }
    js_data_list->push_back(js_data);
  }
  return true;
}

}  // namespace

SyncedNotificationsShim::SyncedNotificationsShim(
    const EventLauncher& event_launcher)
    : event_launcher_(event_launcher) {
}

SyncedNotificationsShim::~SyncedNotificationsShim() {
}

syncer::SyncMergeResult SyncedNotificationsShim::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  if (type == syncer::SYNCED_NOTIFICATIONS)
    notifications_change_processor_ = sync_processor.Pass();
  else if (type == syncer::SYNCED_NOTIFICATION_APP_INFO)
    app_info_change_processor_ = sync_processor.Pass();
  else
    NOTREACHED();

  // Only wake up the extension if both sync data types are ready.
  if (notifications_change_processor_ && app_info_change_processor_) {
    scoped_ptr<Event> event(new Event(
        synced_notifications_private::OnSyncStartup::kEventName,
        synced_notifications_private::OnSyncStartup::Create()));
    event_launcher_.Run(event.Pass());
  }

  return syncer::SyncMergeResult(type);
}

void SyncedNotificationsShim::StopSyncing(syncer::ModelType type) {
  if (type == syncer::SYNCED_NOTIFICATIONS)
    notifications_change_processor_.reset();
  else if (type == syncer::SYNCED_NOTIFICATION_APP_INFO)
    app_info_change_processor_.reset();
  else
    NOTREACHED();
}

syncer::SyncError SyncedNotificationsShim::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& changes) {
  std::vector<linked_ptr<synced_notifications_private::SyncChange> > js_changes;
  for (size_t i = 0; i < changes.size(); ++i)
    js_changes.push_back(BuildNewJSSyncChange(changes[i]));

  scoped_ptr<base::ListValue> args(
      synced_notifications_private::OnDataChanges::Create(js_changes));
  scoped_ptr<Event> event(new Event(
      synced_notifications_private::OnDataChanges::kEventName, args.Pass()));
  event_launcher_.Run(event.Pass());
  return syncer::SyncError();
}

syncer::SyncDataList SyncedNotificationsShim::GetAllSyncData(
      syncer::ModelType type) const {
  NOTIMPLEMENTED();
  return syncer::SyncDataList();
}

bool SyncedNotificationsShim::GetInitialData(
    synced_notifications_private::SyncDataType data_type,
    std::vector<linked_ptr<synced_notifications_private::SyncData> >*
        js_data_list) const {
  if (!IsSyncReady())
    return false;

  syncer::SyncDataList sync_data_list;
  if (JSDataTypeToSyncer(data_type) == syncer::SYNCED_NOTIFICATIONS) {
    sync_data_list = notifications_change_processor_->GetAllSyncData(
        syncer::SYNCED_NOTIFICATIONS);
    if (PopulateJSDataListFromSync(sync_data_list, js_data_list))
      return true;
  } else if (JSDataTypeToSyncer(data_type) ==
                 syncer::SYNCED_NOTIFICATION_APP_INFO) {
    sync_data_list = app_info_change_processor_->GetAllSyncData(
        syncer::SYNCED_NOTIFICATION_APP_INFO);
    if (PopulateJSDataListFromSync(sync_data_list, js_data_list))
      return true;
  }
  return false;
}

bool SyncedNotificationsShim::UpdateNotification(
    const std::string& changed_notification) {
  if (!IsSyncReady())
    return false;

  syncer::SyncChange sync_change;
  if (!BuildNewSyncUpdate(FROM_HERE, changed_notification, &sync_change))
    return false;
  syncer::SyncError error = notifications_change_processor_->ProcessSyncChanges(
      FROM_HERE,
      syncer::SyncChangeList(1, sync_change));
  return !error.IsSet();
}

bool SyncedNotificationsShim::SetRenderContext(
    synced_notifications_private::RefreshRequest refresh_request,
    const std::string& new_context) {
  if (!IsSyncReady())
    return false;

  syncer::SyncChangeProcessor::ContextRefreshStatus sync_refresh_status =
      refresh_request ==
              synced_notifications_private::REFRESH_REQUEST_REFRESH_NEEDED
          ? syncer::SyncChangeProcessor::REFRESH_NEEDED
          : syncer::SyncChangeProcessor::NO_REFRESH;
  syncer::SyncError error =
      notifications_change_processor_->UpdateDataTypeContext(
          syncer::SYNCED_NOTIFICATIONS, sync_refresh_status, new_context);
  return !error.IsSet();
}

bool SyncedNotificationsShim::IsSyncReady() const {
  return notifications_change_processor_ && app_info_change_processor_;
}
