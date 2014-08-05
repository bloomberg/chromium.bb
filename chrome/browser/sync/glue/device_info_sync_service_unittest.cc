// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/sync/glue/device_info_sync_service.h"
#include "chrome/browser/sync/glue/local_device_info_provider_mock.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::AttachmentIdList;
using syncer::AttachmentServiceProxyForTest;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncChangeProcessorWrapperForTest;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncErrorFactoryMock;
using syncer::SyncMergeResult;

namespace browser_sync {

namespace {

class TestChangeProcessor : public SyncChangeProcessor {
 public:
  TestChangeProcessor() {}
  virtual ~TestChangeProcessor() {}

  // SyncChangeProcessor implementation.
  // Store a copy of all the changes passed in so we can examine them later.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE {
    change_list_ = change_list;
    return SyncError();
  }

  // This method isn't used in these tests.
  virtual SyncDataList GetAllSyncData(ModelType type) const OVERRIDE {
    return SyncDataList();
  }

  size_t change_list_size() const { return change_list_.size(); }

  SyncChange::SyncChangeType change_type_at(size_t index) const {
    CHECK_LT(index, change_list_size());
    return change_list_[index].change_type();
  }

  const sync_pb::DeviceInfoSpecifics& device_info_at(size_t index) const {
    CHECK_LT(index, change_list_size());
    return change_list_[index].sync_data().GetSpecifics().device_info();
  }

  const std::string& cache_guid_at(size_t index) const {
    return device_info_at(index).cache_guid();
  }

  const std::string& client_name_at(size_t index) const {
    return device_info_at(index).client_name();
  }

 private:
  SyncChangeList change_list_;
};

class DeviceInfoSyncServiceTest : public testing::Test,
                                  public DeviceInfoTracker::Observer {
 public:
  DeviceInfoSyncServiceTest() : num_device_info_changed_callbacks_(0) {}
  virtual ~DeviceInfoSyncServiceTest() {}

  virtual void SetUp() OVERRIDE {
    local_device_.reset(new LocalDeviceInfoProviderMock(
        "guid_1",
        "client_1",
        "Chromium 10k",
        "Chrome 10k",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
        "device_id"));
    sync_service_.reset(new DeviceInfoSyncService(local_device_.get()));
    sync_processor_.reset(new TestChangeProcessor());
    // Register observer
    sync_service_->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    sync_service_->RemoveObserver(this);
  }

  virtual void OnDeviceInfoChange() OVERRIDE {
    num_device_info_changed_callbacks_++;
  }

  scoped_ptr<SyncChangeProcessor> PassProcessor() {
    return scoped_ptr<SyncChangeProcessor>(
        new SyncChangeProcessorWrapperForTest(sync_processor_.get()));
  }

  scoped_ptr<SyncErrorFactory> CreateAndPassSyncErrorFactory() {
    return scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock());
  }

  SyncData CreateRemoteData(const std::string& client_id,
                            const std::string& client_name) {
    sync_pb::EntitySpecifics entity;
    sync_pb::DeviceInfoSpecifics& specifics = *entity.mutable_device_info();

    specifics.set_cache_guid(client_id);
    specifics.set_client_name(client_name);
    specifics.set_chrome_version("Chromium 10k");
    specifics.set_sync_user_agent("Chrome 10k");
    specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
    specifics.set_signin_scoped_device_id("device_id");

    return SyncData::CreateRemoteData(1,
                                      entity,
                                      base::Time(),
                                      AttachmentIdList(),
                                      AttachmentServiceProxyForTest::Create());
  }

  void AddInitialData(SyncDataList& sync_data_list,
                      const std::string& client_id,
                      const std::string& client_name) {
    SyncData sync_data = CreateRemoteData(client_id, client_name);
    sync_data_list.push_back(sync_data);
  }

  void AddChange(SyncChangeList& change_list,
                 SyncChange::SyncChangeType change_type,
                 const std::string& client_id,
                 const std::string& client_name) {
    SyncData sync_data = CreateRemoteData(client_id, client_name);
    SyncChange sync_change(FROM_HERE, change_type, sync_data);
    change_list.push_back(sync_change);
  }

 protected:
  int num_device_info_changed_callbacks_;
  scoped_ptr<LocalDeviceInfoProviderMock> local_device_;
  scoped_ptr<DeviceInfoSyncService> sync_service_;
  scoped_ptr<TestChangeProcessor> sync_processor_;
  content::TestBrowserThreadBundle thread_bundle_;
};

// Sync with empty initial data.
TEST_F(DeviceInfoSyncServiceTest, StartSyncEmptyInitialData) {
  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              SyncDataList(),
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());

  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(1, merge_result.num_items_before_association());
  EXPECT_EQ(1, merge_result.num_items_after_association());
  EXPECT_EQ(SyncChange::ACTION_ADD, sync_processor_->change_type_at(0));

  EXPECT_EQ(1U, sync_processor_->change_list_size());
  EXPECT_EQ("guid_1", sync_processor_->cache_guid_at(0));

  // Should have one device info corresponding to local device info.
  EXPECT_EQ(1U, sync_service_->GetAllSyncData(syncer::DEVICE_INFO).size());
  EXPECT_EQ(1U, sync_service_->GetAllDeviceInfo().size());
  EXPECT_TRUE(sync_service_->GetDeviceInfo("guid_1"));
  EXPECT_FALSE(sync_service_->GetDeviceInfo("guid_0"));
}

// Sync with initial data matching the local device data.
TEST_F(DeviceInfoSyncServiceTest, StartSyncMatchingInitialData) {
  SyncDataList sync_data;
  AddInitialData(sync_data, "guid_1", "client_1");

  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              sync_data,
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(1, merge_result.num_items_before_association());
  EXPECT_EQ(1, merge_result.num_items_after_association());

  // No changes expected because the device info matches.
  EXPECT_EQ(0U, sync_processor_->change_list_size());

  EXPECT_EQ(1U, sync_service_->GetAllSyncData(syncer::DEVICE_INFO).size());
  EXPECT_EQ(1U, sync_service_->GetAllDeviceInfo().size());
  EXPECT_TRUE(sync_service_->GetDeviceInfo("guid_1"));
  EXPECT_FALSE(sync_service_->GetDeviceInfo("guid_0"));
}

// Sync with misc initial data.
TEST_F(DeviceInfoSyncServiceTest, StartSync) {
  SyncDataList sync_data;
  AddInitialData(sync_data, "guid_2", "foo");
  AddInitialData(sync_data, "guid_3", "bar");
  // This guid matches the local device but the client name is different.
  AddInitialData(sync_data, "guid_1", "baz");

  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              sync_data,
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());

  EXPECT_EQ(2, merge_result.num_items_added());
  EXPECT_EQ(1, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(1, merge_result.num_items_before_association());
  EXPECT_EQ(3, merge_result.num_items_after_association());

  EXPECT_EQ(1U, sync_processor_->change_list_size());
  EXPECT_EQ(SyncChange::ACTION_UPDATE, sync_processor_->change_type_at(0));
  EXPECT_EQ("client_1", sync_processor_->client_name_at(0));

  EXPECT_EQ(3U, sync_service_->GetAllSyncData(syncer::DEVICE_INFO).size());
  EXPECT_EQ(3U, sync_service_->GetAllDeviceInfo().size());
  EXPECT_TRUE(sync_service_->GetDeviceInfo("guid_1"));
  EXPECT_TRUE(sync_service_->GetDeviceInfo("guid_2"));
  EXPECT_TRUE(sync_service_->GetDeviceInfo("guid_3"));
  EXPECT_FALSE(sync_service_->GetDeviceInfo("guid_0"));
}

// Process sync change with ACTION_ADD.
// Verify callback.
TEST_F(DeviceInfoSyncServiceTest, ProcessAddChange) {
  EXPECT_EQ(0, num_device_info_changed_callbacks_);

  // Start with an empty initial data.
  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              SyncDataList(),
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());
  // There should be only one item corresponding to the local device
  EXPECT_EQ(1, merge_result.num_items_after_association());
  EXPECT_EQ(1, num_device_info_changed_callbacks_);

  // Add a new device info with a non-matching guid.
  SyncChangeList change_list;
  AddChange(change_list, SyncChange::ACTION_ADD, "guid_2", "foo");

  SyncError error = sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(2, num_device_info_changed_callbacks_);

  EXPECT_EQ(2U, sync_service_->GetAllDeviceInfo().size());

  EXPECT_TRUE(sync_service_->GetDeviceInfo("guid_1"));
  EXPECT_TRUE(sync_service_->GetDeviceInfo("guid_2"));
  EXPECT_FALSE(sync_service_->GetDeviceInfo("guid_0"));
}

// Process multiple sync change with ACTION_UPDATE and ACTION_ADD.
// Verify that callback is called multiple times.
TEST_F(DeviceInfoSyncServiceTest, ProcessMultipleChanges) {
  SyncDataList sync_data;
  AddInitialData(sync_data, "guid_2", "foo");
  AddInitialData(sync_data, "guid_3", "bar");

  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              sync_data,
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());
  EXPECT_EQ(3, merge_result.num_items_after_association());
  // reset callbacks counter
  num_device_info_changed_callbacks_ = 0;

  SyncChangeList change_list;
  AddChange(change_list, SyncChange::ACTION_UPDATE, "guid_2", "foo_2");

  SyncError error = sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());

  EXPECT_EQ(1, num_device_info_changed_callbacks_);
  EXPECT_EQ(3U, sync_service_->GetAllDeviceInfo().size());
  EXPECT_EQ("foo_2", sync_service_->GetDeviceInfo("guid_2")->client_name());

  change_list.clear();
  AddChange(change_list, SyncChange::ACTION_UPDATE, "guid_3", "bar_3");
  AddChange(change_list, SyncChange::ACTION_ADD, "guid_4", "baz_4");

  error = sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());

  EXPECT_EQ(2, num_device_info_changed_callbacks_);
  EXPECT_EQ(4U, sync_service_->GetAllDeviceInfo().size());
  EXPECT_EQ("bar_3", sync_service_->GetDeviceInfo("guid_3")->client_name());
  EXPECT_EQ("baz_4", sync_service_->GetDeviceInfo("guid_4")->client_name());
}

// Process update to the local device info and verify that it is ignored.
TEST_F(DeviceInfoSyncServiceTest, ProcessUpdateChangeMatchingLocalDevice) {
  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              SyncDataList(),
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());
  EXPECT_EQ(1, merge_result.num_items_after_association());
  // reset callbacks counter
  num_device_info_changed_callbacks_ = 0;

  SyncChangeList change_list;
  AddChange(change_list, SyncChange::ACTION_UPDATE, "guid_1", "foo_1");

  SyncError error = sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());
  // Callback shouldn't be sent in this case.
  EXPECT_EQ(0, num_device_info_changed_callbacks_);
  // Should still have the old local device Info.
  EXPECT_EQ(1U, sync_service_->GetAllDeviceInfo().size());
  EXPECT_EQ("client_1", sync_service_->GetDeviceInfo("guid_1")->client_name());
}

// Process sync change with ACTION_DELETE.
TEST_F(DeviceInfoSyncServiceTest, ProcessDeleteChange) {
  SyncDataList sync_data;
  AddInitialData(sync_data, "guid_2", "foo");
  AddInitialData(sync_data, "guid_3", "bar");

  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              sync_data,
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());
  EXPECT_EQ(3, merge_result.num_items_after_association());
  // reset callbacks counter
  num_device_info_changed_callbacks_ = 0;

  SyncChangeList change_list;
  AddChange(change_list, SyncChange::ACTION_DELETE, "guid_2", "foo_2");

  SyncError error = sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());

  EXPECT_EQ(1, num_device_info_changed_callbacks_);
  EXPECT_EQ(2U, sync_service_->GetAllDeviceInfo().size());
  EXPECT_FALSE(sync_service_->GetDeviceInfo("guid_2"));
}

// Process sync change with unexpected action.
TEST_F(DeviceInfoSyncServiceTest, ProcessInvalidChange) {
  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              SyncDataList(),
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());
  EXPECT_EQ(1, merge_result.num_items_after_association());
  // reset callbacks counter
  num_device_info_changed_callbacks_ = 0;

  SyncChangeList change_list;
  AddChange(change_list, (SyncChange::SyncChangeType)100, "guid_2", "foo_2");

  SyncError error = sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_TRUE(error.IsSet());

  // The number of callback should still be zero.
  EXPECT_EQ(0, num_device_info_changed_callbacks_);
  EXPECT_EQ(1U, sync_service_->GetAllDeviceInfo().size());
}

// Process sync change after unsubscribing from notifications.
TEST_F(DeviceInfoSyncServiceTest, ProcessChangesAfterUnsubscribing) {
  SyncMergeResult merge_result =
      sync_service_->MergeDataAndStartSyncing(syncer::DEVICE_INFO,
                                              SyncDataList(),
                                              PassProcessor(),
                                              CreateAndPassSyncErrorFactory());
  EXPECT_EQ(1, merge_result.num_items_after_association());
  // reset callbacks counter
  num_device_info_changed_callbacks_ = 0;

  SyncChangeList change_list;
  AddChange(change_list, SyncChange::ACTION_ADD, "guid_2", "foo_2");

  // Unsubscribe observer before processing changes.
  sync_service_->RemoveObserver(this);

  SyncError error = sync_service_->ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet());

  // The number of callback should still be zero.
  EXPECT_EQ(0, num_device_info_changed_callbacks_);
}

}  // namespace

}  // namespace browser_sync
