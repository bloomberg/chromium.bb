// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/synced_device_tracker.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/directory.h"
#include "sync/test/test_transaction_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class SyncedDeviceTrackerTest : public ::testing::Test {
 protected:
  SyncedDeviceTrackerTest() : transaction_count_baseline_(0) { }
  virtual ~SyncedDeviceTrackerTest() { }

  virtual void SetUp() {
    test_user_share_.SetUp();
    syncer::TestUserShare::CreateRoot(syncer::DEVICE_INFO, user_share());

    synced_device_tracker_.reset(
        new SyncedDeviceTracker(user_share(),
                                user_share()->directory->cache_guid()));

    // We don't actually touch the Profile, so we can get away with passing in a
    // NULL here.  Constructing a TestingProfile can take over a 100ms, so this
    // optimization can be the difference between 'tests run with a noticeable
    // delay' and 'tests run instantaneously'.
    synced_device_tracker_->Start(user_share());
  }

  virtual void TearDown() {
    synced_device_tracker_.reset();
    test_user_share_.TearDown();
  }

  syncer::UserShare* user_share() {
    return test_user_share_.user_share();
  }

  // Expose the private method to our tests.
  void WriteLocalDeviceInfo(const DeviceInfo& info) {
    synced_device_tracker_->WriteLocalDeviceInfo(info);
  }

  void WriteDeviceInfo(const DeviceInfo& device_info) {
    synced_device_tracker_->WriteDeviceInfo(device_info, device_info.guid());
  }

  void ResetObservedChangesCounter() {
    transaction_count_baseline_ = GetTotalTransactionsCount();
  }

  int GetObservedChangesCounter() {
    return GetTotalTransactionsCount() - transaction_count_baseline_;
  }

  scoped_ptr<SyncedDeviceTracker> synced_device_tracker_;

 private:
  // Count of how many closed WriteTransactions notified of meaningful changes.
  int GetTotalTransactionsCount() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    return test_user_share_.transaction_observer()->transactions_observed();
  }

  base::MessageLoop message_loop_;
  syncer::TestUserShare test_user_share_;
  int transaction_count_baseline_;
};

namespace {

// New client scenario: set device info when no previous info existed.
TEST_F(SyncedDeviceTrackerTest, CreateNewDeviceInfo) {
  ASSERT_FALSE(synced_device_tracker_->ReadLocalDeviceInfo());

  ResetObservedChangesCounter();

  // Include the non-ASCII character "’" (typographic apostrophe) in the client
  // name to ensure that SyncedDeviceTracker can properly handle non-ASCII
  // characters, which client names can include on some platforms (e.g., Mac
  // and iOS).
  DeviceInfo write_device_info(user_share()->directory->cache_guid(),
                               "John’s Device",
                               "Chromium 3000",
                               "ChromeSyncAgent 3000",
                               sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                               "device_id");
  WriteLocalDeviceInfo(write_device_info);

  scoped_ptr<DeviceInfo> read_device_info(
      synced_device_tracker_->ReadLocalDeviceInfo());
  ASSERT_TRUE(read_device_info);
  EXPECT_TRUE(write_device_info.Equals(*read_device_info.get()));

  EXPECT_EQ(1, GetObservedChangesCounter());
}

// Restart scenario: update existing device info with identical data.
TEST_F(SyncedDeviceTrackerTest, DontModifyExistingDeviceInfo) {
  // For writing.
  DeviceInfo device_info(user_share()->directory->cache_guid(),
                         "John’s Device",
                         "XYZ v1",
                         "XYZ SyncAgent v1",
                         sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                         "device_id");
  WriteLocalDeviceInfo(device_info);

  // First read.
  scoped_ptr<DeviceInfo> old_device_info(
      synced_device_tracker_->ReadLocalDeviceInfo());
  ASSERT_TRUE(old_device_info);

  ResetObservedChangesCounter();

  // Overwrite the device info with the same data as before.
  WriteLocalDeviceInfo(device_info);

  // Ensure that this didn't count as a change worth syncing.
  EXPECT_EQ(0, GetObservedChangesCounter());

  // Second read.
  scoped_ptr<DeviceInfo> new_device_info(
      synced_device_tracker_->ReadLocalDeviceInfo());
  ASSERT_TRUE(new_device_info);
  EXPECT_TRUE(old_device_info->Equals(*new_device_info.get()));
}

// Upgrade scenario: update existing device info with new version.
TEST_F(SyncedDeviceTrackerTest, UpdateExistingDeviceInfo) {
  // Write v1 device info.
  DeviceInfo device_info_v1(user_share()->directory->cache_guid(),
                            "John’s Device",
                            "XYZ v1",
                            "XYZ SyncAgent v1",
                            sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                            "device_id1");
  WriteLocalDeviceInfo(device_info_v1);

  ResetObservedChangesCounter();

  // Write upgraded device info.
  DeviceInfo device_info_v2(user_share()->directory->cache_guid(),
                            "John’s Device",
                            "XYZ v2",
                            "XYZ SyncAgent v2",
                            sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                            "device_id2");
  WriteLocalDeviceInfo(device_info_v2);

  // Verify result.
  scoped_ptr<DeviceInfo> result_device_info(
      synced_device_tracker_->ReadLocalDeviceInfo());
  ASSERT_TRUE(result_device_info);

  EXPECT_TRUE(result_device_info->Equals(device_info_v2));

  // The update write should have sent a nudge.
  EXPECT_EQ(1, GetObservedChangesCounter());
}

// Test retrieving DeviceInfos for all the syncing devices.
TEST_F(SyncedDeviceTrackerTest, GetAllDeviceInfo) {
  DeviceInfo device_info1(base::GenerateGUID(),
                          "abc Device",
                          "XYZ v1",
                          "XYZ SyncAgent v1",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                          "device_id1");

  std::string guid1 = base::GenerateGUID();

  DeviceInfo device_info2(base::GenerateGUID(),
                          "def Device",
                          "XYZ v2",
                          "XYZ SyncAgent v2",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                          "device_id2");

  std::string guid2 = base::GenerateGUID();

  WriteDeviceInfo(device_info1);
  WriteDeviceInfo(device_info2);

  ScopedVector<DeviceInfo> device_info;
  synced_device_tracker_->GetAllSyncedDeviceInfo(&device_info);

  EXPECT_EQ(device_info.size(), 2U);
  EXPECT_TRUE(device_info[0]->Equals(device_info1));
  EXPECT_TRUE(device_info[1]->Equals(device_info2));
}

TEST_F(SyncedDeviceTrackerTest, DeviceBackupTime) {
  DeviceInfo device_info(user_share()->directory->cache_guid(),
                         "John’s Device",
                         "XYZ v1",
                         "XYZ SyncAgent v1",
                         sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                         "device_id");
  const base::Time test_backup_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(10000);

  WriteLocalDeviceInfo(device_info);
  synced_device_tracker_->UpdateLocalDeviceBackupTime(test_backup_time);

  // Verify read of device info and backup time.
  EXPECT_EQ(test_backup_time,
            synced_device_tracker_->GetLocalDeviceBackupTime());
  scoped_ptr<DeviceInfo> device_info_out(
      synced_device_tracker_->ReadLocalDeviceInfo());
  ASSERT_TRUE(device_info_out);
  EXPECT_TRUE(device_info.Equals(*device_info_out.get()));

  // Verify backup time is not lost after updating device info.
  DeviceInfo device_info2(user_share()->directory->cache_guid(),
                          "def Device",
                          "XYZ v2",
                          "XYZ SyncAgent v2",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                          "device_id");
  WriteLocalDeviceInfo(device_info2);
  EXPECT_EQ(test_backup_time,
            synced_device_tracker_->GetLocalDeviceBackupTime());
}

}  // namespace

}  // namespace browser_sync
