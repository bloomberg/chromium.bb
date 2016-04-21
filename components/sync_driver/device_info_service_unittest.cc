// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_service.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "sync/api/data_batch.h"
#include "sync/api/entity_data.h"
#include "sync/api/fake_model_type_change_processor.h"
#include "sync/api/metadata_batch.h"
#include "sync/api/model_type_store.h"
#include "sync/internal_api/public/test/model_type_store_test_util.h"
#include "sync/protocol/data_type_state.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver_v2 {

using syncer::SyncError;
using syncer_v2::DataBatch;
using syncer_v2::EntityChange;
using syncer_v2::EntityChangeList;
using syncer_v2::EntityData;
using syncer_v2::EntityDataMap;
using syncer_v2::EntityDataPtr;
using syncer_v2::MetadataBatch;
using syncer_v2::MetadataChangeList;
using syncer_v2::ModelTypeChangeProcessor;
using syncer_v2::ModelTypeService;
using syncer_v2::ModelTypeStore;
using syncer_v2::ModelTypeStoreTestUtil;
using syncer_v2::TagAndData;
using sync_driver::DeviceInfo;
using sync_driver::DeviceInfoTracker;
using sync_driver::LocalDeviceInfoProviderMock;
using sync_pb::DataTypeState;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;

using ClientTagList = ModelTypeService::ClientTagList;
using RecordList = ModelTypeStore::RecordList;
using Result = ModelTypeStore::Result;
using StartCallback = ModelTypeChangeProcessor::StartCallback;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

void AssertResultIsSuccess(Result result) {
  ASSERT_EQ(Result::SUCCESS, result);
}

void AssertEqual(const DeviceInfoSpecifics& s1, const DeviceInfoSpecifics& s2) {
  ASSERT_EQ(s1.cache_guid(), s2.cache_guid());
  ASSERT_EQ(s1.client_name(), s2.client_name());
  ASSERT_EQ(s1.device_type(), s2.device_type());
  ASSERT_EQ(s1.sync_user_agent(), s2.sync_user_agent());
  ASSERT_EQ(s1.chrome_version(), s2.chrome_version());
  ASSERT_EQ(s1.signin_scoped_device_id(), s2.signin_scoped_device_id());
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

void AssertErrorFromDataBatch(SyncError error,
                              std::unique_ptr<DataBatch> batch) {
  ASSERT_TRUE(error.IsSet());
}

void AssertExpectedFromDataBatch(
    std::map<std::string, DeviceInfoSpecifics> expected,
    SyncError error,
    std::unique_ptr<DataBatch> batch) {
  ASSERT_FALSE(error.IsSet());
  while (batch->HasNext()) {
    const TagAndData& pair = batch->Next();
    std::map<std::string, DeviceInfoSpecifics>::iterator iter =
        expected.find(pair.first);
    ASSERT_NE(iter, expected.end());
    AssertEqual(iter->second, pair.second->specifics.device_info());
    // Removing allows us to verify we don't see the same item multiple times,
    // and that we saw everything we expected.
    expected.erase(iter);
  }
  ASSERT_TRUE(expected.empty());
}

// Creats an EntityData/EntityDataPtr around a copy of the given specifics.
EntityDataPtr SpecificsToEntity(const DeviceInfoSpecifics& specifics) {
  EntityData data;
  // These tests do not care about the tag hash, but EntityData and friends
  // cannot differentiate between the default EntityData object if the hash
  // is unset, which causes pass/copy operations to no-op and things start to
  // break, so we throw in a junk value and forget about it.
  data.client_tag_hash = "junk";
  *data.specifics.mutable_device_info() = specifics;
  return data.PassToPtr();
}

std::string CacheGuidToTag(const std::string& guid) {
  return "DeviceInfo_" + guid;
}

// Instead of actually processing anything, simply accumulates all instructions
// in members that can then be accessed. TODO(skym): If this ends up being
// useful for other model type unittests it should be moved out to a shared
// location.
class RecordingModelTypeChangeProcessor
    : public syncer_v2::FakeModelTypeChangeProcessor {
 public:
  RecordingModelTypeChangeProcessor() {}
  ~RecordingModelTypeChangeProcessor() override {}

  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_changes) override {
    put_map_.insert(std::make_pair(client_tag, std::move(entity_data)));
  }

  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_changes) override {
    delete_set_.insert(client_tag);
  }

  void OnMetadataLoaded(std::unique_ptr<MetadataBatch> batch) override {
    std::swap(metadata_, batch);
  }

  const std::map<std::string, std::unique_ptr<EntityData>>& put_map() const {
    return put_map_;
  }
  const std::set<std::string>& delete_set() const { return delete_set_; }
  const MetadataBatch* metadata() const { return metadata_.get(); }

 private:
  std::map<std::string, std::unique_ptr<EntityData>> put_map_;
  std::set<std::string> delete_set_;
  std::unique_ptr<MetadataBatch> metadata_;
};

}  // namespace

class DeviceInfoServiceTest : public testing::Test,
                              public DeviceInfoTracker::Observer {
 protected:
  DeviceInfoServiceTest()
      : change_count_(0),
        store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        local_device_(new LocalDeviceInfoProviderMock(
            "guid_1",
            "client_1",
            "Chromium 10k",
            "Chrome 10k",
            sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
            "device_id")) {}

  ~DeviceInfoServiceTest() override {
    // Some tests may never initialize the service.
    if (service_)
      service_->RemoveObserver(this);

    // Force all remaining (store) tasks to execute so we don't leak memory.
    base::RunLoop().RunUntilIdle();
  }

  void OnDeviceInfoChange() override { change_count_++; }

  std::unique_ptr<ModelTypeChangeProcessor> CreateModelTypeChangeProcessor(
      syncer::ModelType type,
      ModelTypeService* service) {
    processor_ = new RecordingModelTypeChangeProcessor();
    return base::WrapUnique(processor_);
  }

  // Initialized the service based on the current local device and store. Can
  // only be called once per run, as it passes |store_|.
  void InitializeService() {
    ASSERT_TRUE(store_);
    service_.reset(new DeviceInfoService(
        local_device_.get(),
        base::Bind(&ModelTypeStoreTestUtil::MoveStoreToCallback,
                   base::Passed(&store_)),
        base::Bind(&DeviceInfoServiceTest::CreateModelTypeChangeProcessor,
                   base::Unretained(this))));
    service_->AddObserver(this);
  }

  // Creates the service and runs any outstanding tasks. This will typically
  // cause all initialization callbacks between the sevice and store to fire.
  void InitializeAndPump() {
    InitializeService();
    base::RunLoop().RunUntilIdle();
  }

  // Creates the service, runs any outstanding tasks, and then indicates to the
  // service that sync wants to start and forces the processor to be created.
  void InitializeAndPumpAndStart() {
    InitializeAndPump();
    service()->OnSyncStarting(StartCallback());
    ASSERT_TRUE(processor_);
  }

  // Generates a specifics object with slightly differing values. Will generate
  // the same values on each run of a test because a simple counter is used to
  // vary field values.
  DeviceInfoSpecifics GenerateTestSpecifics() {
    int label = ++generated_count_;
    DeviceInfoSpecifics specifics;
    specifics.set_cache_guid(base::StringPrintf("cache guid %d", label));
    specifics.set_client_name(base::StringPrintf("client name %d", label));
    specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
    specifics.set_sync_user_agent(
        base::StringPrintf("sync user agent %d", label));
    specifics.set_chrome_version(
        base::StringPrintf("chrome version %d", label));
    specifics.set_signin_scoped_device_id(
        base::StringPrintf("signin scoped device id %d", label));
    return specifics;
  }

  std::unique_ptr<DeviceInfoSpecifics> CopyToSpecifics(const DeviceInfo& info) {
    return DeviceInfoService::CopyToSpecifics(info);
  }

  // Override to allow specific cache guids.
  DeviceInfoSpecifics GenerateTestSpecifics(const std::string& guid) {
    DeviceInfoSpecifics specifics(GenerateTestSpecifics());
    specifics.set_cache_guid(guid);
    return specifics;
  }

  // Helper method to reduce duplicated code between tests. Wraps the given
  // specifics object in an EntityData and EntityChange of type ACTION_ADD, and
  // pushes them onto the given change list. The corresponding client tag the
  // service determines is returned. Instance method because we need access to
  // service to generate the tag.
  std::string PushBackEntityChangeAdd(const DeviceInfoSpecifics& specifics,
                                      EntityChangeList* changes) {
    EntityDataPtr ptr = SpecificsToEntity(specifics);
    const std::string tag = service()->GetClientTag(ptr.value());
    changes->push_back(EntityChange::CreateAdd(tag, ptr));
    return tag;
  }

  // Allows access to the store before that will ultimately be used to
  // initialize the service.
  ModelTypeStore* store() {
    EXPECT_TRUE(store_);
    return store_.get();
  }

  // Get the number of times the service notifies observers of changes.
  int change_count() { return change_count_; }

  // Allows overriding the provider before the service is initialized.
  void set_local_device(std::unique_ptr<LocalDeviceInfoProviderMock> provider) {
    ASSERT_FALSE(service_);
    std::swap(local_device_, provider);
  }
  LocalDeviceInfoProviderMock* local_device() { return local_device_.get(); }

  // Allows access to the service after InitializeService() is called.
  DeviceInfoService* service() {
    EXPECT_TRUE(service_);
    return service_.get();
  }

  RecordingModelTypeChangeProcessor* processor() {
    EXPECT_TRUE(processor_);
    return processor_;
  }

  // Should only be called after the service has been initialized. Will first
  // recover the service's store, so another can be initialized later, and then
  // deletes the service.
  void PumpAndShutdown() {
    ASSERT_TRUE(service_);
    base::RunLoop().RunUntilIdle();
    std::swap(store_, service_->store_);
    service_->RemoveObserver(this);
    service_.reset();
  }

 private:
  int change_count_;

  // Although we never use this in this class, the in memory model type store
  // grabs the current task runner from a static accessor which point at this
  // message loop. Must be declared/initilized before we call the synchronous
  // CreateInMemoryStoreForTest.
  base::MessageLoop message_loop_;

  // Holds the store while the service is not initialized.
  std::unique_ptr<ModelTypeStore> store_;

  std::unique_ptr<LocalDeviceInfoProviderMock> local_device_;

  // Not initialized immediately (upon test's constructor). This allows each
  // test case to modify the dependencies the service will be constructed with.
  std::unique_ptr<DeviceInfoService> service_;

  // A non-owning pointer to the processor given to the service. Will be nullptr
  // before being given to the service, to make ownership easier.
  RecordingModelTypeChangeProcessor* processor_ = nullptr;

  // A monotonically increasing label for generated specifics objects with data
  // that is slightly different from eachother.
  int generated_count_ = 0;
};

namespace {

TEST_F(DeviceInfoServiceTest, EmptyDataReconciliation) {
  InitializeAndPump();
  ASSERT_EQ(0u, service()->GetAllDeviceInfo().size());
  service()->OnSyncStarting(StartCallback());
  ScopedVector<DeviceInfo> all_device_info(service()->GetAllDeviceInfo());
  ASSERT_EQ(1u, all_device_info.size());
  ASSERT_TRUE(
      local_device()->GetLocalDeviceInfo()->Equals(*all_device_info[0]));
}

TEST_F(DeviceInfoServiceTest, EmptyDataReconciliationSlowLoad) {
  InitializeService();
  service()->OnSyncStarting(StartCallback());
  ASSERT_EQ(0u, service()->GetAllDeviceInfo().size());
  base::RunLoop().RunUntilIdle();
  ScopedVector<DeviceInfo> all_device_info(service()->GetAllDeviceInfo());
  ASSERT_EQ(1u, all_device_info.size());
  ASSERT_TRUE(
      local_device()->GetLocalDeviceInfo()->Equals(*all_device_info[0]));
}

TEST_F(DeviceInfoServiceTest, LocalProviderSubscription) {
  set_local_device(base::WrapUnique(new LocalDeviceInfoProviderMock()));
  InitializeAndPumpAndStart();
  ASSERT_EQ(0u, service()->GetAllDeviceInfo().size());
  local_device()->Initialize(base::WrapUnique(
      new DeviceInfo("guid_1", "client_1", "Chromium 10k", "Chrome 10k",
                     sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id")));
  ScopedVector<DeviceInfo> all_device_info(service()->GetAllDeviceInfo());
  ASSERT_EQ(1u, all_device_info.size());
  ASSERT_TRUE(
      local_device()->GetLocalDeviceInfo()->Equals(*all_device_info[0]));
}

TEST_F(DeviceInfoServiceTest, GetClientTagNormal) {
  InitializeService();
  const std::string guid = "abc";
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info()->set_cache_guid(guid);
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(CacheGuidToTag(guid), service()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoServiceTest, GetClientTagEmpty) {
  InitializeService();
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info();
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(CacheGuidToTag(""), service()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoServiceTest, TestWithLocalData) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DeviceInfoSpecifics specifics(GenerateTestSpecifics());
  store()->WriteData(batch.get(), specifics.cache_guid(),
                     specifics.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  ScopedVector<DeviceInfo> all_device_info(service()->GetAllDeviceInfo());
  ASSERT_EQ(1u, all_device_info.size());
  AssertEqual(specifics, *all_device_info[0]);
  AssertEqual(specifics,
              *service()->GetDeviceInfo(specifics.cache_guid()).get());
}

TEST_F(DeviceInfoServiceTest, TestWithLocalMetadata) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DataTypeState state;
  state.set_encryption_key_name("ekn");
  store()->WriteGlobalMetadata(batch.get(), state.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));
  InitializeAndPump();
  ScopedVector<DeviceInfo> all_device_info(service()->GetAllDeviceInfo());
  ASSERT_EQ(1u, all_device_info.size());
  ASSERT_TRUE(
      local_device()->GetLocalDeviceInfo()->Equals(*all_device_info[0]));
  EXPECT_EQ(1u, processor()->put_map().size());
}

TEST_F(DeviceInfoServiceTest, TestWithLocalDataAndMetadata) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DeviceInfoSpecifics specifics(GenerateTestSpecifics());
  store()->WriteData(batch.get(), specifics.cache_guid(),
                     specifics.SerializeAsString());
  DataTypeState state;
  state.set_encryption_key_name("ekn");
  store()->WriteGlobalMetadata(batch.get(), state.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  ScopedVector<DeviceInfo> all_device_info(service()->GetAllDeviceInfo());
  ASSERT_EQ(2u, all_device_info.size());
  AssertEqual(specifics,
              *service()->GetDeviceInfo(specifics.cache_guid()).get());
  ASSERT_TRUE(processor()->metadata());
  ASSERT_EQ(state.encryption_key_name(),
            processor()->metadata()->GetDataTypeState().encryption_key_name());
}

TEST_F(DeviceInfoServiceTest, GetData) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DeviceInfoSpecifics specifics1(GenerateTestSpecifics());
  DeviceInfoSpecifics specifics2(GenerateTestSpecifics());
  DeviceInfoSpecifics specifics3(GenerateTestSpecifics());
  store()->WriteData(batch.get(), specifics1.cache_guid(),
                     specifics1.SerializeAsString());
  store()->WriteData(batch.get(), specifics2.cache_guid(),
                     specifics2.SerializeAsString());
  store()->WriteData(batch.get(), specifics3.cache_guid(),
                     specifics3.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  std::map<std::string, DeviceInfoSpecifics> expected;
  expected[CacheGuidToTag(specifics1.cache_guid())] = specifics1;
  expected[CacheGuidToTag(specifics3.cache_guid())] = specifics3;
  ClientTagList client_tags;
  client_tags.push_back(CacheGuidToTag(specifics1.cache_guid()));
  client_tags.push_back(CacheGuidToTag(specifics3.cache_guid()));
  service()->GetData(client_tags,
                     base::Bind(&AssertExpectedFromDataBatch, expected));
}

TEST_F(DeviceInfoServiceTest, GetDataMissing) {
  InitializeAndPump();
  std::map<std::string, DeviceInfoSpecifics> expected;
  ClientTagList client_tags;
  client_tags.push_back(CacheGuidToTag("tag1"));
  service()->GetData(client_tags,
                     base::Bind(&AssertExpectedFromDataBatch, expected));
}

TEST_F(DeviceInfoServiceTest, GetDataNotInitialized) {
  InitializeService();
  ClientTagList client_tags;
  service()->GetData(client_tags, base::Bind(&AssertErrorFromDataBatch));
}

TEST_F(DeviceInfoServiceTest, GetAllData) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DeviceInfoSpecifics specifics1(GenerateTestSpecifics());
  DeviceInfoSpecifics specifics2(GenerateTestSpecifics());
  const std::string& tag1 = CacheGuidToTag(specifics1.cache_guid());
  const std::string& tag2 = CacheGuidToTag(specifics2.cache_guid());
  store()->WriteData(batch.get(), specifics1.cache_guid(),
                     specifics1.SerializeAsString());
  store()->WriteData(batch.get(), specifics2.cache_guid(),
                     specifics2.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  std::map<std::string, DeviceInfoSpecifics> expected;
  expected[tag1] = specifics1;
  expected[tag2] = specifics2;
  ClientTagList client_tags;
  client_tags.push_back(tag1);
  client_tags.push_back(tag2);
  service()->GetData(client_tags,
                     base::Bind(&AssertExpectedFromDataBatch, expected));
}

TEST_F(DeviceInfoServiceTest, GetAllDataNotInitialized) {
  InitializeService();
  service()->GetAllData(base::Bind(&AssertErrorFromDataBatch));
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesBeforeInit) {
  InitializeService();
  const SyncError error = service()->ApplySyncChanges(
      service()->CreateMetadataChangeList(), EntityChangeList());
  EXPECT_TRUE(error.IsSet());
  EXPECT_EQ(0, change_count());
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesEmpty) {
  InitializeAndPump();
  const SyncError error = service()->ApplySyncChanges(
      service()->CreateMetadataChangeList(), EntityChangeList());
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(0, change_count());
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesInMemory) {
  InitializeAndPump();

  DeviceInfoSpecifics specifics = GenerateTestSpecifics();
  EntityChangeList add_changes;
  const std::string tag = PushBackEntityChangeAdd(specifics, &add_changes);
  SyncError error = service()->ApplySyncChanges(
      service()->CreateMetadataChangeList(), add_changes);

  EXPECT_FALSE(error.IsSet());
  std::unique_ptr<DeviceInfo> info =
      service()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  AssertEqual(specifics, *info.get());
  EXPECT_EQ(1, change_count());

  EntityChangeList delete_changes;
  delete_changes.push_back(EntityChange::CreateDelete(tag));
  error = service()->ApplySyncChanges(service()->CreateMetadataChangeList(),
                                      delete_changes);

  EXPECT_FALSE(error.IsSet());
  EXPECT_FALSE(service()->GetDeviceInfo(specifics.cache_guid()));
  EXPECT_EQ(2, change_count());
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesStore) {
  InitializeAndPump();

  DeviceInfoSpecifics specifics = GenerateTestSpecifics();
  EntityChangeList data_changes;
  const std::string tag = PushBackEntityChangeAdd(specifics, &data_changes);
  DataTypeState state;
  state.set_encryption_key_name("ekn");
  std::unique_ptr<MetadataChangeList> metadata_changes(
      service()->CreateMetadataChangeList());
  metadata_changes->UpdateDataTypeState(state);

  const SyncError error =
      service()->ApplySyncChanges(std::move(metadata_changes), data_changes);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(1, change_count());

  PumpAndShutdown();
  InitializeAndPump();

  std::unique_ptr<DeviceInfo> info =
      service()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  AssertEqual(specifics, *info.get());
  // TODO(skym): Uncomment once SimpleMetadataChangeList::TransferChanges is
  // implemented.
  // EXPECT_TRUE(processor()->metadata());
  // EXPECT_EQ(state.encryption_key_name(),
  // processor()->metadata()->GetDataTypeState().encryption_key_name());
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesWithLocalGuid) {
  InitializeAndPumpAndStart();

  // The point of this test is to try to apply remote changes that have the same
  // cache guid as the local device. The service should ignore these changes
  // since only it should be performing writes on its data.
  DeviceInfoSpecifics specifics =
      GenerateTestSpecifics(local_device()->GetLocalDeviceInfo()->guid());
  EntityChangeList change_list;
  const std::string tag = PushBackEntityChangeAdd(specifics, &change_list);

  // Should have a single change from reconciliation.
  EXPECT_TRUE(
      service()->GetDeviceInfo(local_device()->GetLocalDeviceInfo()->guid()));
  EXPECT_EQ(1, change_count());

  EXPECT_FALSE(
      service()
          ->ApplySyncChanges(service()->CreateMetadataChangeList(), change_list)
          .IsSet());
  EXPECT_EQ(1, change_count());

  change_list.clear();
  change_list.push_back(EntityChange::CreateDelete(tag));
  EXPECT_FALSE(
      service()
          ->ApplySyncChanges(service()->CreateMetadataChangeList(), change_list)
          .IsSet());
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoServiceTest, ApplyDeleteNonexistent) {
  InitializeAndPumpAndStart();
  EXPECT_EQ(1, change_count());
  EntityChangeList delete_changes;
  delete_changes.push_back(EntityChange::CreateDelete(CacheGuidToTag("tag")));
  const SyncError error = service()->ApplySyncChanges(
      service()->CreateMetadataChangeList(), delete_changes);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoServiceTest, MergeWithoutProcessor) {
  InitializeService();
  const SyncError error = service()->MergeSyncData(
      service()->CreateMetadataChangeList(), EntityDataMap());
  EXPECT_TRUE(error.IsSet());
  EXPECT_EQ(0, change_count());
}

TEST_F(DeviceInfoServiceTest, MergeEmpty) {
  InitializeAndPumpAndStart();
  EXPECT_EQ(1, change_count());
  const SyncError error = service()->MergeSyncData(
      service()->CreateMetadataChangeList(), EntityDataMap());
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(1, change_count());
  EXPECT_EQ(1u, processor()->put_map().size());
  EXPECT_EQ(0u, processor()->delete_set().size());
}

TEST_F(DeviceInfoServiceTest, MergeWithData) {
  const DeviceInfoSpecifics unique_local(GenerateTestSpecifics("unique_local"));
  const DeviceInfoSpecifics conflict_local(GenerateTestSpecifics("conflict"));
  DeviceInfoSpecifics conflict_remote(GenerateTestSpecifics("conflict"));
  DeviceInfoSpecifics unique_remote(GenerateTestSpecifics("unique_remote"));

  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  store()->WriteData(batch.get(), unique_local.cache_guid(),
                     unique_local.SerializeAsString());
  store()->WriteData(batch.get(), conflict_local.cache_guid(),
                     conflict_local.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPumpAndStart();
  EXPECT_EQ(1, change_count());

  EntityDataMap remote_input;
  remote_input[CacheGuidToTag(conflict_remote.cache_guid())] =
      SpecificsToEntity(conflict_remote);
  remote_input[CacheGuidToTag(unique_remote.cache_guid())] =
      SpecificsToEntity(unique_remote);

  DataTypeState state;
  state.set_encryption_key_name("ekn");
  std::unique_ptr<MetadataChangeList> metadata_changes(
      service()->CreateMetadataChangeList());
  metadata_changes->UpdateDataTypeState(state);

  const SyncError error =
      service()->MergeSyncData(std::move(metadata_changes), remote_input);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(2, change_count());

  // The remote should beat the local in conflict.
  EXPECT_EQ(4u, service()->GetAllDeviceInfo().size());
  AssertEqual(unique_local, *service()->GetDeviceInfo("unique_local").get());
  AssertEqual(unique_remote, *service()->GetDeviceInfo("unique_remote").get());
  AssertEqual(conflict_remote, *service()->GetDeviceInfo("conflict").get());

  // Service should have told the processor about the existance of unique_local.
  EXPECT_TRUE(processor()->delete_set().empty());
  EXPECT_EQ(2u, processor()->put_map().size());
  const auto& it = processor()->put_map().find(CacheGuidToTag("unique_local"));
  ASSERT_NE(processor()->put_map().end(), it);
  AssertEqual(unique_local, it->second->specifics.device_info());

  // TODO(skym): Uncomment once SimpleMetadataChangeList::TransferChanges is
  // implemented.
  // ASSERT_EQ(state.encryption_key_name(),
  // processor()->metadata()->GetDataTypeState().encryption_key_name());
}

TEST_F(DeviceInfoServiceTest, MergeLocalGuid) {
  const DeviceInfo* local_device_info = local_device()->GetLocalDeviceInfo();
  std::unique_ptr<DeviceInfoSpecifics> specifics(
      CopyToSpecifics(*local_device_info));
  const std::string guid = local_device_info->guid();

  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  store()->WriteData(batch.get(), guid, specifics->SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPumpAndStart();

  EntityDataMap remote_input;
  remote_input[CacheGuidToTag(guid)] = SpecificsToEntity(*specifics);

  const SyncError error = service()->MergeSyncData(
      service()->CreateMetadataChangeList(), remote_input);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(0, change_count());
  EXPECT_EQ(1u, service()->GetAllDeviceInfo().size());
  EXPECT_TRUE(processor()->delete_set().empty());
  EXPECT_TRUE(processor()->put_map().empty());
}

}  // namespace

}  // namespace sync_driver_v2
