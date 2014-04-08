// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNC_NOTIFIER_TEST_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNC_NOTIFIER_TEST_UTILS_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"

// Test data for App Info structures.
extern const char kSendingService1Name[];
extern const char kSendingService2Name[];
extern const char kSendingService3Name[];
extern const char kTestInfoUrl[];
extern const char kTestIconUrl[];

// Fake data for creating a SyncedNotification.
extern const char kAppId1[];
extern const char kAppId2[];
extern const char kAppId3[];
extern const char kAppId4[];
extern const char kAppId5[];
extern const char kAppId6[];
extern const char kAppId7[];
extern const char kKey1[];
extern const char kKey2[];
extern const char kKey3[];
extern const char kKey4[];
extern const char kKey5[];
extern const char kKey6[];
extern const char kKey7[];
extern const char kIconUrl1[];
extern const char kIconUrl2[];
extern const char kIconUrl3[];
extern const char kIconUrl4[];
extern const char kIconUrl5[];
extern const char kIconUrl6[];
extern const char kIconUrl7[];
extern const char kTitle1[];
extern const char kTitle2[];
extern const char kTitle3[];
extern const char kTitle4[];
extern const char kTitle5[];
extern const char kTitle6[];
extern const char kTitle7[];
extern const char kText1[];
extern const char kText2[];
extern const char kText3[];
extern const char kText4[];
extern const char kText5[];
extern const char kText6[];
extern const char kText7[];
extern const char kText1And1[];
extern const char kImageUrl1[];
extern const char kImageUrl2[];
extern const char kImageUrl3[];
extern const char kImageUrl4[];
extern const char kImageUrl5[];
extern const char kImageUrl6[];
extern const char kImageUrl7[];
extern const char kExpectedOriginUrl[];
extern const char kDefaultDestinationTitle[];
extern const char kDefaultDestinationIconUrl[];
extern const char kDefaultDestinationUrl[];
extern const char kButtonOneTitle[];
extern const char kButtonOneIconUrl[];
extern const char kButtonOneUrl[];
extern const char kButtonTwoTitle[];
extern const char kButtonTwoIconUrl[];
extern const char kButtonTwoUrl[];
extern const char kContainedTitle1[];
extern const char kContainedTitle2[];
extern const char kContainedTitle3[];
extern const char kContainedMessage1[];
extern const char kContainedMessage2[];
extern const char kContainedMessage3[];
const uint64 kFakeCreationTime = 42;
const int kProtobufPriority = static_cast<int>(
    sync_pb::CoalescedSyncedNotification_Priority_INVISIBLE);

const sync_pb::CoalescedSyncedNotification_ReadState kRead =
    sync_pb::CoalescedSyncedNotification_ReadState_READ;
const sync_pb::CoalescedSyncedNotification_ReadState kDismissed =
    sync_pb::CoalescedSyncedNotification_ReadState_DISMISSED;
const sync_pb::CoalescedSyncedNotification_ReadState kUnread =
    sync_pb::CoalescedSyncedNotification_ReadState_UNREAD;

// This function builds the sync data object we use to create a testing
// notification.
syncer::SyncData CreateSyncData(
    const std::string& title,
    const std::string& text,
    const std::string& app_icon_url,
    const std::string& image_url,
    const std::string& app_id,
    const std::string& key,
      const sync_pb::CoalescedSyncedNotification_ReadState read_state);

namespace notifier {

// Stub out the SyncedNotificationAppInfoService.  This is used to
// 1. Verify that when bitmaps are fetched, the OnFetchComplete causes a call to
// OnBitmapFetchesDone.
// 2. Provide the app info for our first sending service.
class StubSyncedNotificationAppInfoService
    : public SyncedNotificationAppInfoService {
 public:
  // Interface functions from SyncedNotificationAppInfoService
    explicit StubSyncedNotificationAppInfoService(Profile* profile);
  virtual ~StubSyncedNotificationAppInfoService();
  virtual void Shutdown() OVERRIDE{}
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE{}
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE;
  void ProcessIncomingAppInfoProtobuf(
      const sync_pb::SyncedNotificationAppInfo& app_info) {}
  void ProcessRemovedAppInfoProtobuf(
      const sync_pb::SyncedNotificationAppInfo& app_info) {}
  // Remember the arguments we saw in the most recent call.
  virtual void OnBitmapFetchesDone(std::vector<std::string> added_app_ids,
                                   std::vector<std::string> removed_app_ids)
      OVERRIDE;
  scoped_ptr<SyncedNotificationAppInfo>
  CreateSyncedNotificationAppInfoFromProtobuf(
      const sync_pb::SyncedNotificationAppInfo& app_info);
  SyncedNotificationAppInfo* FindSyncedNotificationAppInfoByAppId(
      const std::string& app_id);
  std::string FindSendingServiceNameFromAppId(const std::string app_id);
  std::vector<SyncedNotificationSendingServiceSettingsData>
  GetAllSendingServiceSettingsData();
  void AddForTest(
      scoped_ptr<notifier::SyncedNotificationAppInfo> sending_service_info) {}
  static void set_avoid_bitmap_fetching_for_test(bool avoid) {}

  // Probe functions to return data.
  std::vector<std::string> added_app_ids();
  std::vector<std::string> removed_app_ids();
  bool on_bitmap_fetches_done_called();

 private:
  SyncedNotificationAppInfo* FindSyncedNotificationAppInfoByName(
      const std::string& name);
  std::vector<std::string> added_app_ids_;
  std::vector<std::string> removed_app_ids_;
  bool on_bitmap_fetches_done_called_;
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_SYNC_NOTIFIER_TEST_UTILS_H_
