// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::EntitySpecifics;
using syncer::SyncData;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncDataList;
using syncer::SYNCED_NOTIFICATION_APP_INFO;
using notifier::SyncedNotificationAppInfo;
using notifier::SyncedNotificationAppInfoService;
using sync_pb::SyncedNotificationAppInfoSpecifics;

namespace {
const char kSendingService1Name[] = "TestService1";
const char kSendingService2Name[] = "TestService2";
const char kSendingService3Name[] = "TestService3";
const char kAppId1[] = "service1_appid1";
const char kAppId2[] = "service1_appid2";
const char kAppId3[] = "service1_appid3";
const char kAppId4[] = "service2_appid1";
const char kAppId5[] = "service2_appid2";
const char kTestIconUrl[] = "https://www.google.com/someicon.png";
}  // namespace

namespace notifier {

// Extract app_info id from syncer::SyncData.
std::string GetAppInfoId(const SyncData& sync_data) {
  SyncedNotificationAppInfoSpecifics specifics =
      sync_data.GetSpecifics().synced_notification_app_info();

  // TODO(petewil): It would be better if we had a designated unique identifier
  // instead of relying on the display name to be unique.
  return specifics.synced_notification_app_info(0).settings_display_name();
}

// Dummy SyncChangeProcessor used to help review what SyncChanges are pushed
// back up to Sync.
class TestChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestChangeProcessor() {}
  virtual ~TestChangeProcessor() {}

  // Store a copy of all the changes passed in so we can examine them later.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE {
    change_map_.clear();
    for (SyncChangeList::const_iterator iter = change_list.begin();
         iter != change_list.end();
         ++iter) {
      // Put the data into the change tracking map.
      change_map_[GetAppInfoId(iter->sync_data())] = *iter;
    }

    return syncer::SyncError();
  }

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE {
    return syncer::SyncDataList();
  }

  size_t change_list_size() { return change_map_.size(); }

  bool ContainsId(const std::string& id) {
    return change_map_.find(id) != change_map_.end();
  }

  SyncChange GetChangeById(const std::string& id) {
    EXPECT_TRUE(ContainsId(id));
    return change_map_[id];
  }

 private:
  // Track the changes received in ProcessSyncChanges.
  std::map<std::string, SyncChange> change_map_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeProcessor);
};

class SyncedNotificationAppInfoServiceTest : public testing::Test {

 public:
  SyncedNotificationAppInfoServiceTest()
      : sync_processor_(new TestChangeProcessor),
        sync_processor_delegate_(
            new syncer::FakeSyncChangeProcessor()) {}

  virtual ~SyncedNotificationAppInfoServiceTest() {}

  // Overrides from testing::Test.
  virtual void SetUp() { profile_.reset(new TestingProfile()); }

  // Introduce some sample test data into the system.
  void AddTestingAppInfosToList(
      SyncedNotificationAppInfoService* app_info_service) {
    // Create the app_info struct, setting the settings display name.

    // The sending_service_infos_ list will take ownership of this pointer.
    SyncedNotificationAppInfo* test_item1 =
        new SyncedNotificationAppInfo(kSendingService1Name);

    // Add some App IDs.
    test_item1->AddAppId(kAppId1);
    test_item1->AddAppId(kAppId2);

    // Set this icon GURL.
    test_item1->SetSettingsIcon(GURL(kTestIconUrl));

    // Add to the list.
    app_info_service->sending_service_infos_.push_back(test_item1);

    // Add a second test item for another service.
    SyncedNotificationAppInfo* test_item2 =
        new SyncedNotificationAppInfo(kSendingService2Name);

    // Add some App IDs.
    test_item2->AddAppId(kAppId4);
    test_item2->AddAppId(kAppId5);

    // Set thi icon GURL.
    test_item2->SetSettingsIcon(GURL(kTestIconUrl));

    // Add to the list.
    app_info_service->sending_service_infos_.push_back(test_item2);
  }

  // Put some representative test data into the AppInfo protobuf.
  static void FillProtobufWithTestData1(
      sync_pb::SyncedNotificationAppInfo& protobuf) {
    protobuf.add_app_id(std::string(kAppId1));
    protobuf.add_app_id(std::string(kAppId2));
    protobuf.set_settings_display_name(kSendingService1Name);
    protobuf.mutable_icon()->set_url(kTestIconUrl);
  }

  static void FillProtobufWithTestData2(
      sync_pb::SyncedNotificationAppInfo& protobuf) {
    protobuf.add_app_id(std::string(kAppId3));
    protobuf.set_settings_display_name(kSendingService1Name);
    protobuf.mutable_icon()->set_url(kTestIconUrl);
  }

  // Helper to create syncer::SyncChange.
  static SyncChange CreateSyncChange(SyncChange::SyncChangeType type,
                                     const std::string& settings_display_name,
                                     const std::string& icon_url,
                                     const std::string& app_id1,
                                     const std::string& app_id2) {

    return SyncChange(
        FROM_HERE,
        type,
        CreateSyncData(settings_display_name, icon_url, app_id1, app_id2));
  }

  // Build a SyncData object to look like what Sync would deliver.
  static SyncData CreateSyncData(const std::string& settings_display_name,
                                 const std::string& icon_url,
                                 const std::string& app_id1,
                                 const std::string& app_id2) {
    // CreateLocalData makes a copy of this, so it can safely live on the stack.
    EntitySpecifics entity_specifics;
    EXPECT_FALSE(app_id1.empty());

    sync_pb::SyncedNotificationAppInfoSpecifics* specifics =
        entity_specifics.mutable_synced_notification_app_info();

    // Add a synced_notification_app_info object.
    specifics->add_synced_notification_app_info();
    sync_pb::SyncedNotificationAppInfo* app_info =
        specifics->mutable_synced_notification_app_info(0);

    // Add the key, the settings display name.
    app_info->set_settings_display_name(settings_display_name);

    // Add the icon URL.
    app_info->mutable_icon()->set_url(icon_url);

    // Add the app IDs.
    app_info->add_app_id(app_id1);

    // Only add the second if it is non-empty
    if (!app_id2.empty()) {
      app_info->add_app_id(app_id2);
    }

    // Create the sync data.
    SyncData sync_data =
        SyncData::CreateLocalData("syncer::SYNCED_NOTIFICATION_APP_INFO",
                                  "SyncedNotificationAppInfoServiceUnitTest",
                                  entity_specifics);

    return sync_data;
  }

  TestChangeProcessor* processor() {
    return static_cast<TestChangeProcessor*>(sync_processor_.get());
  }

  scoped_ptr<syncer::SyncChangeProcessor> PassProcessor() {
    return sync_processor_delegate_.Pass();
  }

 protected:
  scoped_ptr<TestingProfile> profile_;

 private:
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_delegate_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(SyncedNotificationAppInfoServiceTest);
};

// Null data case - we have no data, and sync has no data when we start up.
TEST_F(SyncedNotificationAppInfoServiceTest, MergeDataAndStartSyncingTest) {
    SyncedNotificationAppInfoService app_info_service(profile_.get());

  app_info_service.MergeDataAndStartSyncing(
      SYNCED_NOTIFICATION_APP_INFO,
      SyncDataList(),  // Empty.
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  EXPECT_EQ(static_cast<size_t>(0),
            app_info_service.sending_service_infos_size());
}

// Process sync changes when there is no local data.
TEST_F(SyncedNotificationAppInfoServiceTest, ProcessSyncChangesEmptyModel) {
  // We initially have no data.
  SyncedNotificationAppInfoService app_info_service(profile_.get());

  // Set up an ADD.
  SyncChangeList changes;
  changes.push_back(CreateSyncChange(SyncChange::ACTION_ADD,
                                     kSendingService1Name,
                                     kTestIconUrl,
                                     kAppId1,
                                     kAppId2));

  // Process the changes we built.
  app_info_service.ProcessSyncChanges(FROM_HERE, changes);

  // Verify sync change made it to the SyncedNotificationAppInfo list.
  SyncedNotificationAppInfo* app_info1 =
      app_info_service.FindSyncedNotificationAppInfoByName(
          kSendingService1Name);
  EXPECT_NE(static_cast<SyncedNotificationAppInfo*>(NULL), app_info1);
  EXPECT_TRUE(app_info1->HasAppId(kAppId1));
  EXPECT_TRUE(app_info1->HasAppId(kAppId2));
  EXPECT_FALSE(app_info1->HasAppId(kAppId3));
  EXPECT_EQ(app_info1->settings_icon_url(), GURL(kTestIconUrl));
}

// Process sync changes when there is local data.
TEST_F(SyncedNotificationAppInfoServiceTest, ProcessSyncChangesNonEmptyModel) {
    SyncedNotificationAppInfoService app_info_service(profile_.get());

  // Create some local fake data. We rely on the specific ids set up here.
  AddTestingAppInfosToList(&app_info_service);

  // Set up an UPDATE.
  SyncChangeList changes;

  changes.push_back(CreateSyncChange(SyncChange::ACTION_UPDATE,
                                     kSendingService1Name,
                                     kTestIconUrl,
                                     kAppId1,
                                     kAppId3));

  // Simulate incoming changed sync data at runtime.
  app_info_service.ProcessSyncChanges(FROM_HERE, changes);

  // We should find that the first item now has a different set of app ids.
  SyncedNotificationAppInfo* app_info1 =
      app_info_service.FindSyncedNotificationAppInfoByName(
          kSendingService1Name);
  EXPECT_NE(static_cast<SyncedNotificationAppInfo*>(NULL), app_info1);
  EXPECT_TRUE(app_info1->HasAppId(kAppId1));
  EXPECT_FALSE(app_info1->HasAppId(kAppId2));
  EXPECT_TRUE(app_info1->HasAppId(kAppId3));
  EXPECT_EQ(app_info1->settings_icon_url(), GURL(kTestIconUrl));
}

// Test ProcessIncomingAppInfoProtobuf with an add.
TEST_F(SyncedNotificationAppInfoServiceTest,
       ProcessIncomingAppInfoProtobufAddTest) {
  // Get an app info service object.
  SyncedNotificationAppInfoService app_info_service(profile_.get());

  // Get an app info protobuf.
  sync_pb::SyncedNotificationAppInfo protobuf;
  FillProtobufWithTestData1(protobuf);

  // Call the function we are testing.
  app_info_service.ProcessIncomingAppInfoProtobuf(protobuf);

  // Ensure that we now have an app_info in our list, and it looks like we
  // expect.
  notifier::SyncedNotificationAppInfo* found_app_info;
  found_app_info = app_info_service.FindSyncedNotificationAppInfoByName(
      kSendingService1Name);
  EXPECT_NE(static_cast<notifier::SyncedNotificationAppInfo*>(NULL),
            found_app_info);
  EXPECT_TRUE(found_app_info->HasAppId(kAppId1));
  EXPECT_TRUE(found_app_info->HasAppId(kAppId2));
}

// Test ProcessIncomingAppInfoProtobuf with an update
TEST_F(SyncedNotificationAppInfoServiceTest,
       ProcessIncomingAppInfoProtobufUpdateTest) {
  // Get an app info service object.
  SyncedNotificationAppInfoService app_info_service(profile_.get());

  // Make an app info with the same display name as the first one in the test
  // data.
  sync_pb::SyncedNotificationAppInfo protobuf1;
  FillProtobufWithTestData1(protobuf1);
  app_info_service.ProcessIncomingAppInfoProtobuf(protobuf1);

  // Ensure that we now have an app_info in our list, and it looks like we
  // expect.
  notifier::SyncedNotificationAppInfo* found_app_info1;
  found_app_info1 = app_info_service.FindSyncedNotificationAppInfoByName(
      kSendingService1Name);
  EXPECT_NE(static_cast<notifier::SyncedNotificationAppInfo*>(NULL),
            found_app_info1);
  EXPECT_TRUE(found_app_info1->HasAppId(kAppId1));
  EXPECT_TRUE(found_app_info1->HasAppId(kAppId2));

  // Make an update to the protobuf that has already been sent.
  app_info_service.FreeSyncedNotificationAppInfoByName(kSendingService1Name);
  // Change appid1 to appid3
  sync_pb::SyncedNotificationAppInfo protobuf2;
  FillProtobufWithTestData2(protobuf2);
  app_info_service.ProcessIncomingAppInfoProtobuf(protobuf2);

  // Ensure we have the same named app info as before, but it has the new
  // contents.
  notifier::SyncedNotificationAppInfo* found_app_info2;
  found_app_info2 = app_info_service.FindSyncedNotificationAppInfoByName(
      kSendingService1Name);
  EXPECT_NE(static_cast<notifier::SyncedNotificationAppInfo*>(NULL),
            found_app_info2);
  EXPECT_FALSE(found_app_info2->HasAppId(kAppId1));
  EXPECT_TRUE(found_app_info2->HasAppId(kAppId3));
}

// Test our function that creates a synced notification from a protobuf.
TEST_F(SyncedNotificationAppInfoServiceTest,
       CreateSyncedNotificationAppInfoFromProtobufTest) {
  // Build a protobuf and fill it with data.
  sync_pb::SyncedNotificationAppInfo protobuf;
  FillProtobufWithTestData1(protobuf);

  scoped_ptr<SyncedNotificationAppInfo> app_info;
  app_info = SyncedNotificationAppInfoService::
      CreateSyncedNotificationAppInfoFromProtobuf(protobuf);

  // Ensure the app info class has the fields we expect.
  EXPECT_EQ(std::string(kSendingService1Name),
            app_info->settings_display_name());
  EXPECT_TRUE(app_info->HasAppId(kAppId1));
  EXPECT_TRUE(app_info->HasAppId(kAppId2));
  EXPECT_EQ(GURL(std::string(kTestIconUrl)), app_info->settings_icon_url());
}

// Test our find function.
TEST_F(SyncedNotificationAppInfoServiceTest,
       FindSyncedNotificationAppInfoByNameTest) {
  SyncedNotificationAppInfoService app_info_service(profile_.get());

  AddTestingAppInfosToList(&app_info_service);

  SyncedNotificationAppInfo* found;

  found = app_info_service.FindSyncedNotificationAppInfoByName(
      kSendingService1Name);

  EXPECT_NE(static_cast<SyncedNotificationAppInfo*>(NULL), found);
  EXPECT_EQ(std::string(kSendingService1Name), found->settings_display_name());

  found = app_info_service.FindSyncedNotificationAppInfoByName(
      kSendingService3Name);
  EXPECT_EQ(NULL, found);
}

// Test our delete function.
TEST_F(SyncedNotificationAppInfoServiceTest,
       FreeSyncedNotificationAppInfoByNameTest) {
  SyncedNotificationAppInfoService app_info_service(profile_.get());

  AddTestingAppInfosToList(&app_info_service);

  SyncedNotificationAppInfo* found;

  app_info_service.FreeSyncedNotificationAppInfoByName(kSendingService1Name);
  found = app_info_service.FindSyncedNotificationAppInfoByName(
      kSendingService1Name);
  EXPECT_EQ(NULL, found);
}

}  // namespace notifier
