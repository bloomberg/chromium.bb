// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_service.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "sync/api/model_type_store.h"
#include "sync/internal_api/public/test/model_type_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver_v2 {

using syncer_v2::EntityData;
using syncer_v2::ModelTypeStore;
using syncer_v2::ModelTypeStoreTestUtil;
using sync_driver::DeviceInfo;
using sync_driver::DeviceInfoTracker;
using sync_driver::LocalDeviceInfoProviderMock;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;

using Result = ModelTypeStore::Result;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

void AssertResultIsSuccess(Result result) {
  ASSERT_EQ(Result::SUCCESS, result);
}

void AssertEqual(const DeviceInfoSpecifics& specifics,
                 const DeviceInfo& model) {
  ASSERT_EQ(specifics.cache_guid(), model.guid());
  ASSERT_EQ(specifics.client_name(), model.client_name());
  ASSERT_EQ(specifics.device_type(), model.device_type());
  ASSERT_EQ(specifics.sync_user_agent(), model.sync_user_agent());
  ASSERT_EQ(specifics.chrome_version(), model.chrome_version());
  ASSERT_EQ(specifics.signin_scoped_device_id(),
            model.signin_scoped_device_id());
}

class DeviceInfoServiceTest : public testing::Test,
                              public DeviceInfoTracker::Observer {
 protected:
  ~DeviceInfoServiceTest() override {
    // Some tests may never initialize the service.
    if (service_)
      service_->RemoveObserver(this);

    // Force all remaining (store) tasks to execute so we don't leak memory.
    base::RunLoop().RunUntilIdle();
  }

  void OnDeviceInfoChange() override { num_device_info_changed_callbacks_++; }

 protected:
  DeviceInfoServiceTest()
      : num_device_info_changed_callbacks_(0),
        store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        local_device_(new LocalDeviceInfoProviderMock(
            "guid_1",
            "client_1",
            "Chromium 10k",
            "Chrome 10k",
            sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
            "device_id")) {}

  // Initialized the service based on the current local device and store. Can
  // only be called once per run, as it passes |store_|.
  void InitializeService() {
    ASSERT_TRUE(store_);
    service_.reset(new DeviceInfoService(
        local_device_.get(),
        base::Bind(&ModelTypeStoreTestUtil::MoveStoreToCallback,
                   base::Passed(&store_))));
    service_->AddObserver(this);
  }

  // Creates the service and runs any outstanding tasks. This will typically
  // cause all initialization callbacks between the sevice and store to fire.
  void InitializeAndPump() {
    InitializeService();
    base::RunLoop().RunUntilIdle();
  }

  // Allows access to the store before that will ultimately be used to
  // initialize the service.
  ModelTypeStore* store() {
    EXPECT_TRUE(store_);
    return store_.get();
  }

  // Get the number of times the service notifies observers of changes.
  int num_device_info_changed_callbacks() {
    return num_device_info_changed_callbacks_;
  }

  // Allows overriding the provider before the service is initialized.
  void set_local_device(scoped_ptr<LocalDeviceInfoProviderMock> provider) {
    ASSERT_FALSE(service_);
    std::swap(local_device_, provider);
  }
  LocalDeviceInfoProviderMock* local_device() { return local_device_.get(); }

  // Allows access to the service after InitializeService() is called.
  DeviceInfoService* service() {
    EXPECT_TRUE(service_);
    return service_.get();
  }

 private:
  int num_device_info_changed_callbacks_;

  // Although we never use this in this class, the in memory model type store
  // grabs the current task runner from a static accessor which point at this
  // message loop. Must be declared/initilized before we call the synchronous
  // CreateInMemoryStoreForTest.
  base::MessageLoop message_loop_;

  // Temporarily holds the store before the service is initialized.
  scoped_ptr<ModelTypeStore> store_;

  scoped_ptr<LocalDeviceInfoProviderMock> local_device_;

  // Not initialized immediately (upon test's constructor). This allows each
  // test case to modify the dependencies the service will be constructed with.
  scoped_ptr<DeviceInfoService> service_;
};

TEST_F(DeviceInfoServiceTest, EmptyDataReconciliation) {
  InitializeService();
  ASSERT_EQ(0u, service()->GetAllDeviceInfo().size());
  base::RunLoop().RunUntilIdle();
  // TODO(skym): crbug.com/582460: Verify reconciliation has happened.
}

TEST_F(DeviceInfoServiceTest, LocalProviderSubscription) {
  set_local_device(make_scoped_ptr(new LocalDeviceInfoProviderMock()));
  InitializeAndPump();
  ASSERT_EQ(0u, service()->GetAllDeviceInfo().size());
  local_device()->Initialize(make_scoped_ptr(
      new DeviceInfo("guid_1", "client_1", "Chromium 10k", "Chrome 10k",
                     sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id")));
  // TODO(skym): crbug.com/582460: Verify reconciliation has happened.
}

TEST_F(DeviceInfoServiceTest, NonEmptyStoreLoad) {
  // Override the provider so that reconciliation never happens.
  set_local_device(make_scoped_ptr(new LocalDeviceInfoProviderMock()));

  scoped_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DeviceInfoSpecifics specifics;
  specifics.set_cache_guid("a");
  specifics.set_client_name("b");
  specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  specifics.set_sync_user_agent("d");
  specifics.set_chrome_version("e");
  specifics.set_backup_timestamp(6);
  store()->WriteData(batch.get(), "tag", specifics.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  ScopedVector<DeviceInfo> all_device_info(service()->GetAllDeviceInfo());
  ASSERT_EQ(1u, all_device_info.size());
  AssertEqual(specifics, *all_device_info[0]);
  AssertEqual(specifics, *service()->GetDeviceInfo("tag").get());
}

TEST_F(DeviceInfoServiceTest, GetClientTagNormal) {
  InitializeAndPump();
  const std::string guid = "abc";
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info()->set_cache_guid(guid);
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(guid, service()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoServiceTest, GetClientTagEmpty) {
  InitializeAndPump();
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info();
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ("", service()->GetClientTag(entity_data));
}

}  // namespace

}  // namespace sync_driver_v2
