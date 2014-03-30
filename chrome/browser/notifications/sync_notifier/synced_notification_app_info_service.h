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

// Information that the ChromeNotifierService needs from this app info to be
// able to properly enable and disable the sending services.
struct SyncedNotificationSendingServiceSettingsData {
  SyncedNotificationSendingServiceSettingsData(
      std::string settings_display_name,
      gfx::Image settings_icon,
      message_center::NotifierId notifier_id);
  std::string settings_display_name;
  gfx::Image settings_icon;
  message_center::NotifierId notifier_id;
};


class ChromeNotifierService;

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

  // When the bitmaps are all ready, tell ChromeNotifierService about changes.
  virtual void OnBitmapFetchesDone(std::vector<std::string> added_app_ids,
                                   std::vector<std::string> removed_app_ids);

  // Convert the protobuf to our internal format.
  scoped_ptr<SyncedNotificationAppInfo>
      CreateSyncedNotificationAppInfoFromProtobuf(
          const sync_pb::SyncedNotificationAppInfo& app_info);

  // Get the app info that contains this sending service name.
  SyncedNotificationAppInfo* FindSyncedNotificationAppInfoByName(
      const std::string& name);

  // Get the app info that contains this app id.
  SyncedNotificationAppInfo* FindSyncedNotificationAppInfoByAppId(
      const std::string& app_id);

  // Lookup the sending service name for a given app id.
  std::string FindSendingServiceNameFromAppId(const std::string app_id);

  // Return a list of all sending service names.
  std::vector<SyncedNotificationSendingServiceSettingsData>
  GetAllSendingServiceSettingsData();

  void set_chrome_notifier_service(ChromeNotifierService* notifier) {
    chrome_notifier_service_ = notifier;
  }

  // Functions for test.
  void AddForTest(
      scoped_ptr<notifier::SyncedNotificationAppInfo> sending_service_info) {
    Add(sending_service_info.Pass());
  }

  // If we allow the tests to do bitmap fetching, they will attempt to fetch
  // a URL from the web, which will fail.  We can already test the majority
  // of what we want without also trying to fetch bitmaps.  Other tests will
  // cover bitmap fetching.
  static void set_avoid_bitmap_fetching_for_test(bool avoid) {
    avoid_bitmap_fetching_for_test_ = avoid;
  }

 private:
  // Add an app_info object to our list.  This takes ownership of the pointer.
  void Add(
      scoped_ptr<notifier::SyncedNotificationAppInfo> sending_service_info);

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

  // Don't let unit tests try to hit the network.
  static bool avoid_bitmap_fetching_for_test_;

  // Pointer to the ChromeNotifierService.  Its lifetime is controlled by the
  // ChromeNotifierService itself, which will zero out the pointer when it is
  // destroyed.
  ChromeNotifierService* chrome_notifier_service_;

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
                           FindSyncedNotificationAppInfoByNameTestTest);
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
