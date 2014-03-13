// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info.h"
#include "components/keyed_service/core/keyed_service.h"
#include "sync/api/syncable_service.h"

class NotificationUIManager;
class Profile;

namespace sync_pb {
class SyncedNotificationAppInfo;
}  // namespace sync_pb

namespace notifier {

// The SyncedNotificationAppInfoService contains and syncs AppInfo protobufs
// from the server with metadata about the services sending synced
// notifications.
class SyncedNotificationAppInfoService : public syncer::SyncableService,
                                         public KeyedService {
 public:
  explicit SyncedNotificationAppInfoService(Profile* profile);
  virtual ~SyncedNotificationAppInfoService();

  // Methods from KeyedService.
  virtual void Shutdown() OVERRIDE;

  // syncer::SyncableService implementation.
  // This is called at chrome startup with the data from sync.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;

  // This does not normally get called, since this is not a user selectable
  // data type, but it could get called if an error occurs at shutdown.
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;

  // Process incoming changes from the server.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // Get all data currently on this clients.
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE;

  // Handles the Add and Update cases.
  void ProcessIncomingAppInfoProtobuf(
      const sync_pb::SyncedNotificationAppInfo& app_info);

  // Handles the Removed case.
  void ProcessRemovedAppInfoProtobuf(
      const sync_pb::SyncedNotificationAppInfo& app_info);

  // Convert the protobuf to our internal format.
  static scoped_ptr<SyncedNotificationAppInfo>
      CreateSyncedNotificationAppInfoFromProtobuf(
          const sync_pb::SyncedNotificationAppInfo& app_info);

  // Functions for test.
  void AddForTest(
      scoped_ptr<notifier::SyncedNotificationAppInfo> sending_service_info) {
    Add(sending_service_info.Pass());
  }

 private:
  // Add an app_info object to our list.  This takes ownership of the pointer.
  void Add(
      scoped_ptr<notifier::SyncedNotificationAppInfo> sending_service_info);

  // Get the app info that contains this ID.
  SyncedNotificationAppInfo* FindSyncedNotificationAppInfoByName(
      const std::string& name);

  // Remove this app info.
  void FreeSyncedNotificationAppInfoByName(const std::string& name);

  size_t sending_service_infos_size() { return sending_service_infos_.size(); }

  // Back pointer to the owning profile.
  Profile* const profile_;

  // Debug testing member for checking thread is as expected.
  base::ThreadChecker thread_checker_;

  // Member list of AppInfo objects.
  ScopedVector<notifier::SyncedNotificationAppInfo> sending_service_infos_;

  // Cache of the sync info.
  syncer::SyncData sync_data_;

  friend class SyncedNotificationAppInfoServiceTest;
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           MergeDataAndStartSyncingTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           ProcessSyncChangesEmptyModel);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           ProcessSyncChangesNonEmptyModel);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           FindSyncedNotificationAppInfoByNameTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           FreeSyncedNotificationAppInfoByNameTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           CreateSyncedNotificationAppInfoFromProtobufTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           ProcessIncomingAppInfoProtobufAddTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           ProcessIncomingAppInfoProtobufUpdateTest);
  FRIEND_TEST_ALL_PREFIXES(SyncedNotificationAppInfoServiceTest,
                           ProcessRemovedAppInfoProtobufTest);
  DISALLOW_COPY_AND_ASSIGN(SyncedNotificationAppInfoService);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNCED_NOTIFICATION_APP_INFO_SERVICE_H_
