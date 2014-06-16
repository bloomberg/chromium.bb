// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/synced_notifications_private/synced_notifications_shim.h"

#include "base/json/json_writer.h"
#include "extensions/browser/event_router.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace extensions;
using namespace extensions::api;

namespace {

// Builds a SyncData for the specified |type| based on |key|.
syncer::SyncData BuildData(syncer::ModelType type, const std::string& key) {
  sync_pb::EntitySpecifics specifics;
  if (type == syncer::SYNCED_NOTIFICATIONS) {
    specifics.mutable_synced_notification()
        ->mutable_coalesced_notification()
        ->set_key(key);
  } else {
    specifics.mutable_synced_notification_app_info()
        ->add_synced_notification_app_info()
        ->add_app_id(key);
  }
  syncer::SyncData data = syncer::SyncData::CreateLocalData(
      key, key, specifics);
  return data;
}

// Builds a SyncChange with an update to the specified |type| based on |key|.
syncer::SyncChange BuildChange(syncer::ModelType type, const std::string& key) {
  syncer::SyncChangeList change_list;
  syncer::SyncData data = BuildData(type, key);
  return syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE, data);
}

// Verifies that the specifics within |data| match the serialized specifics
// within |serialized|.
testing::AssertionResult DataSpecificsMatch(const syncer::SyncData& data,
                                            const std::string& serialized) {
  if (data.GetDataType() == syncer::SYNCED_NOTIFICATIONS) {
    const sync_pb::SyncedNotificationSpecifics& proto =
        data.GetSpecifics().synced_notification();
    if (serialized != proto.SerializeAsString())
      return testing::AssertionFailure() << "Notification specifics mismatch";
  } else {
    const sync_pb::SyncedNotificationAppInfoSpecifics& proto =
        data.GetSpecifics().synced_notification_app_info();
    if (serialized != proto.SerializeAsString())
      return testing::AssertionFailure() << "App info specifics mismatch";
  }
  return testing::AssertionSuccess();
}

// Verifies that the update within |change| matchs the serialized specifics
// within |serialized|.
testing::AssertionResult ChangeSpecificsMatch(const syncer::SyncChange& change,
                                              const std::string& serialized) {
  if (change.change_type() != syncer::SyncChange::ACTION_UPDATE)
    return testing::AssertionFailure() << "Invalid change type";
  return DataSpecificsMatch(change.sync_data(), serialized);
}

class SyncedNotificationsShimTest : public testing::Test {
 public:
  SyncedNotificationsShimTest();
  virtual ~SyncedNotificationsShimTest();

  // Starts sync for both sync types.
  void StartSync();
  // Starts sync for the specified datatype |type|.
  void StartSync(syncer::ModelType type);

  // Transfers ownership of the last event received.
  scoped_ptr<Event> GetLastEvent();

  SyncedNotificationsShim* shim() { return &shim_; }
  syncer::FakeSyncChangeProcessor* notification_processor() {
    return notification_processor_;
  }
  syncer::FakeSyncChangeProcessor* app_info_processor() {
    return app_info_processor_;
  }

 private:
  void EventCallback(scoped_ptr<Event> event);

  // Shim being tested.
  SyncedNotificationsShim shim_;

  syncer::FakeSyncChangeProcessor* notification_processor_;
  syncer::FakeSyncChangeProcessor* app_info_processor_;

  // The last event fired by the shim.
  scoped_ptr<Event> last_event_fired_;
};

SyncedNotificationsShimTest::SyncedNotificationsShimTest()
    : shim_(base::Bind(&SyncedNotificationsShimTest::EventCallback,
                       base::Unretained(this))),
      notification_processor_(NULL),
      app_info_processor_(NULL) {}

SyncedNotificationsShimTest::~SyncedNotificationsShimTest() {}

void SyncedNotificationsShimTest::EventCallback(scoped_ptr<Event> event) {
  ASSERT_FALSE(last_event_fired_);
  last_event_fired_ = event.Pass();
}

scoped_ptr<Event> SyncedNotificationsShimTest::GetLastEvent() {
  return last_event_fired_.Pass();
}

void SyncedNotificationsShimTest::StartSync() {
  StartSync(syncer::SYNCED_NOTIFICATIONS);
  StartSync(syncer::SYNCED_NOTIFICATION_APP_INFO);
  GetLastEvent();
}

void SyncedNotificationsShimTest::StartSync(syncer::ModelType type) {
  scoped_ptr<syncer::FakeSyncChangeProcessor> change_processor(
      new syncer::FakeSyncChangeProcessor());
  if (type == syncer::SYNCED_NOTIFICATIONS)
    notification_processor_ = change_processor.get();
  else
    app_info_processor_ = change_processor.get();
  syncer::SyncDataList sync_data;
  shim_.MergeDataAndStartSyncing(
      type,
      sync_data,
      change_processor.PassAs<syncer::SyncChangeProcessor>(),
      scoped_ptr<syncer::SyncErrorFactory>());
}

// Starting sync should fire the sync started event, but only after both types
// have started.
TEST_F(SyncedNotificationsShimTest, StartSync) {
  EXPECT_FALSE(shim()->IsSyncReady());
  StartSync(syncer::SYNCED_NOTIFICATIONS);
  EXPECT_FALSE(shim()->IsSyncReady());
  EXPECT_FALSE(GetLastEvent());

  StartSync(syncer::SYNCED_NOTIFICATION_APP_INFO);
  EXPECT_TRUE(shim()->IsSyncReady());

  scoped_ptr<Event> event = GetLastEvent();
  ASSERT_TRUE(event);
  EXPECT_EQ(synced_notifications_private::OnSyncStartup::kEventName,
            event->event_name);

  EXPECT_TRUE(notification_processor()->changes().empty());
  EXPECT_TRUE(app_info_processor()->changes().empty());
}

// A sync update should fire the OnDataChanges event with the updated
// notification.
TEST_F(SyncedNotificationsShimTest, ProcessSyncChangesSingleNotification) {
  StartSync();
  syncer::SyncChangeList change_list;
  change_list.push_back(BuildChange(syncer::SYNCED_NOTIFICATIONS, "key"));
  shim()->ProcessSyncChanges(FROM_HERE, change_list);
  scoped_ptr<Event> event = GetLastEvent();
  ASSERT_TRUE(event);
  EXPECT_EQ(synced_notifications_private::OnDataChanges::kEventName,
            event->event_name);
  ASSERT_TRUE(event->event_args);
  EXPECT_EQ(1U, event->event_args->GetSize());

  base::ListValue* args = NULL;
  ASSERT_TRUE(event->event_args->GetList(0, &args));
  EXPECT_EQ(1U, args->GetSize());
  base::DictionaryValue* sync_change_value = NULL;
  ASSERT_TRUE(args->GetDictionary(0, &sync_change_value));
  scoped_ptr<synced_notifications_private::SyncChange> sync_change =
      synced_notifications_private::SyncChange::FromValue(*sync_change_value);
  ASSERT_TRUE(sync_change);
  EXPECT_TRUE(ChangeSpecificsMatch(change_list[0],
                                   sync_change->data.data_item));
  EXPECT_EQ(synced_notifications_private::CHANGE_TYPE_UPDATED,
            sync_change->change_type);
}

// Verify that multiple notification updates can be sent in one event.
TEST_F(SyncedNotificationsShimTest, ProcessSyncChangesMultipleNotification) {
  StartSync();
  syncer::SyncChangeList change_list;
  change_list.push_back(BuildChange(syncer::SYNCED_NOTIFICATIONS, "key"));
  change_list.push_back(BuildChange(syncer::SYNCED_NOTIFICATIONS, "key2"));
  shim()->ProcessSyncChanges(FROM_HERE, change_list);
  scoped_ptr<Event> event = GetLastEvent();
  ASSERT_TRUE(event);
  EXPECT_EQ(synced_notifications_private::OnDataChanges::kEventName,
            event->event_name);
  ASSERT_TRUE(event->event_args);
  base::ListValue* args = NULL;
  ASSERT_TRUE(event->event_args->GetList(0, &args));
  EXPECT_EQ(2U, args->GetSize());

  base::DictionaryValue* sync_change_value = NULL;
  ASSERT_TRUE(args->GetDictionary(0, &sync_change_value));
  scoped_ptr<synced_notifications_private::SyncChange> sync_change =
      synced_notifications_private::SyncChange::FromValue(*sync_change_value);
  ASSERT_TRUE(sync_change);
  EXPECT_TRUE(ChangeSpecificsMatch(change_list[0],
                                   sync_change->data.data_item));
  EXPECT_EQ(synced_notifications_private::CHANGE_TYPE_UPDATED,
            sync_change->change_type);

  ASSERT_TRUE(args->GetDictionary(1, &sync_change_value));
  sync_change =
      synced_notifications_private::SyncChange::FromValue(*sync_change_value);
  ASSERT_TRUE(sync_change);
  EXPECT_TRUE(ChangeSpecificsMatch(change_list[1],
                                   sync_change->data.data_item));
  EXPECT_EQ(synced_notifications_private::CHANGE_TYPE_UPDATED,
            sync_change->change_type);
}

// Verify AppInfo updates trigger OnDataChanges events.
TEST_F(SyncedNotificationsShimTest, ProcessSyncChangeAppInfo) {
  StartSync();
  syncer::SyncChangeList change_list;
  change_list.push_back(
      BuildChange(syncer::SYNCED_NOTIFICATION_APP_INFO, "key"));
  shim()->ProcessSyncChanges(FROM_HERE, change_list);
  scoped_ptr<Event> event = GetLastEvent();
  ASSERT_TRUE(event);
  EXPECT_EQ(synced_notifications_private::OnDataChanges::kEventName,
            event->event_name);
  ASSERT_TRUE(event->event_args);
  EXPECT_EQ(1U, event->event_args->GetSize());

  base::ListValue* args = NULL;
  ASSERT_TRUE(event->event_args->GetList(0, &args));
  EXPECT_EQ(1U, args->GetSize());
  base::DictionaryValue* sync_change_value = NULL;
  ASSERT_TRUE(args->GetDictionary(0, &sync_change_value));
  scoped_ptr<synced_notifications_private::SyncChange> sync_change =
      synced_notifications_private::SyncChange::FromValue(*sync_change_value);
  ASSERT_TRUE(sync_change);
  EXPECT_TRUE(ChangeSpecificsMatch(change_list[0],
                                   sync_change->data.data_item));
  EXPECT_EQ(synced_notifications_private::CHANGE_TYPE_UPDATED,
            sync_change->change_type);
}

// Attempt to get the initial sync data both before and after sync starts.
TEST_F(SyncedNotificationsShimTest, GetInitialData) {
  std::vector<linked_ptr<synced_notifications_private::SyncData> > data;
  EXPECT_FALSE(shim()->GetInitialData(
      synced_notifications_private::SYNC_DATA_TYPE_SYNCED_NOTIFICATION, &data));
  EXPECT_FALSE(shim()->GetInitialData(
      synced_notifications_private::SYNC_DATA_TYPE_APP_INFO, &data));

  StartSync();

  EXPECT_TRUE(shim()->GetInitialData(
      synced_notifications_private::SYNC_DATA_TYPE_SYNCED_NOTIFICATION, &data));
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(shim()->GetInitialData(
      synced_notifications_private::SYNC_DATA_TYPE_APP_INFO, &data));
  EXPECT_TRUE(data.empty());

  notification_processor()->data().push_back(BuildData(
      syncer::SYNCED_NOTIFICATIONS, "notif_key"));
  EXPECT_TRUE(shim()->GetInitialData(
      synced_notifications_private::SYNC_DATA_TYPE_SYNCED_NOTIFICATION, &data));
  EXPECT_EQ(1U, data.size());
  EXPECT_TRUE(DataSpecificsMatch(notification_processor()->data()[0],
                                 data[0]->data_item));

  data.clear();
  app_info_processor()->data().push_back(BuildData(
      syncer::SYNCED_NOTIFICATION_APP_INFO, "app_key"));
  EXPECT_TRUE(shim()->GetInitialData(
      synced_notifications_private::SYNC_DATA_TYPE_APP_INFO, &data));
  EXPECT_EQ(1U, data.size());
  EXPECT_TRUE(DataSpecificsMatch(app_info_processor()->data()[0],
                                 data[0]->data_item));
}

// Verify that notification updates are properly handled.
TEST_F(SyncedNotificationsShimTest, UpdateNotification) {
  syncer::SyncData data =
      BuildData(syncer::SYNCED_NOTIFICATIONS, "notif_key");
  std::string serialized =
      data.GetSpecifics().synced_notification().SerializeAsString();
  EXPECT_FALSE(shim()->UpdateNotification(serialized));

  StartSync();

  EXPECT_FALSE(shim()->UpdateNotification("gibberish"));
  EXPECT_TRUE(notification_processor()->changes().empty());

  EXPECT_TRUE(shim()->UpdateNotification(serialized));
  EXPECT_EQ(1U, notification_processor()->changes().size());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            notification_processor()->changes()[0].change_type());
  EXPECT_EQ(syncer::SYNCED_NOTIFICATIONS,
            notification_processor()->changes()[0].sync_data().GetDataType());
  EXPECT_EQ(serialized,
            notification_processor()
                ->changes()[0]
                .sync_data()
                .GetSpecifics()
                .synced_notification()
                .SerializeAsString());
}

// Verify that SetRenderContext updates the datatype context properly.
TEST_F(SyncedNotificationsShimTest, SetRenderContext) {
  const std::string kContext = "context";
  EXPECT_FALSE(shim()->SetRenderContext(
      synced_notifications_private::REFRESH_REQUEST_REFRESH_NEEDED, kContext));

  StartSync();

  EXPECT_TRUE(shim()->SetRenderContext(
      synced_notifications_private::REFRESH_REQUEST_REFRESH_NEEDED, kContext));
  EXPECT_EQ(kContext, notification_processor()->context());
}

}  // namespace
