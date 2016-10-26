// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/device_info_service.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/time.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_error_handler_mock.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using base::Time;
using base::TimeDelta;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;
using sync_pb::ModelTypeState;

using DeviceInfoList = std::vector<std::unique_ptr<DeviceInfo>>;
using StorageKeyList = ModelTypeService::StorageKeyList;
using RecordList = ModelTypeStore::RecordList;
using Result = ModelTypeStore::Result;
using StartCallback = ModelTypeChangeProcessor::StartCallback;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

std::unique_ptr<DeviceInfo> CreateDeviceInfo() {
  return base::MakeUnique<DeviceInfo>(
      "guid_1", "client_1", "Chromium 10k", "Chrome 10k",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id");
}

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

void AssertExpectedFromDataBatch(
    std::map<std::string, DeviceInfoSpecifics> expected,
    SyncError error,
    std::unique_ptr<DataBatch> batch) {
  ASSERT_FALSE(error.IsSet());
  while (batch->HasNext()) {
    const KeyAndData& pair = batch->Next();
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

// Creates an EntityData/EntityDataPtr around a copy of the given specifics.
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

// Helper method to reduce duplicated code between tests. Wraps the given
// specifics object in an EntityData and EntityChange of type ACTION_ADD, and
// pushes them onto the given change list. The corresponding guid of the data
// is returned, which happens to be the storage key.
std::string PushBackEntityChangeAdd(const DeviceInfoSpecifics& specifics,
                                    EntityChangeList* changes) {
  EntityDataPtr ptr = SpecificsToEntity(specifics);
  changes->push_back(EntityChange::CreateAdd(specifics.cache_guid(), ptr));
  return specifics.cache_guid();
}

// Instead of actually processing anything, simply accumulates all instructions
// in members that can then be accessed. TODO(skym): If this ends up being
// useful for other model type unittests it should be moved out to a shared
// location.
class RecordingModelTypeChangeProcessor : public FakeModelTypeChangeProcessor {
 public:
  RecordingModelTypeChangeProcessor() {}
  ~RecordingModelTypeChangeProcessor() override {}

  void Put(const std::string& storage_key,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_changes) override {
    put_multimap_.insert(std::make_pair(storage_key, std::move(entity_data)));
  }

  void Delete(const std::string& storage_key,
              MetadataChangeList* metadata_changes) override {
    delete_set_.insert(storage_key);
  }

  void OnMetadataLoaded(SyncError error,
                        std::unique_ptr<MetadataBatch> batch) override {
    std::swap(metadata_, batch);
  }

  const std::multimap<std::string, std::unique_ptr<EntityData>>& put_multimap()
      const {
    return put_multimap_;
  }
  const std::set<std::string>& delete_set() const { return delete_set_; }
  const MetadataBatch* metadata() const { return metadata_.get(); }

 private:
  std::multimap<std::string, std::unique_ptr<EntityData>> put_multimap_;
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
        local_device_(new LocalDeviceInfoProviderMock()) {
    local_device_->Initialize(CreateDeviceInfo());
  }

  ~DeviceInfoServiceTest() override {
    // Some tests may never initialize the service.
    if (service_)
      service_->RemoveObserver(this);

    // Force all remaining (store) tasks to execute so we don't leak memory.
    base::RunLoop().RunUntilIdle();
  }

  void OnDeviceInfoChange() override { change_count_++; }

  std::unique_ptr<ModelTypeChangeProcessor> CreateModelTypeChangeProcessor(
      ModelType type,
      ModelTypeService* service) {
    auto processor = base::MakeUnique<RecordingModelTypeChangeProcessor>();
    processor_ = processor.get();
    return std::move(processor);
  }

  // Initialized the service based on the current local device and store. Can
  // only be called once per run, as it passes |store_|.
  void InitializeService() {
    ASSERT_TRUE(store_);
    service_ = base::MakeUnique<DeviceInfoService>(
        local_device_.get(),
        base::Bind(&ModelTypeStoreTestUtil::MoveStoreToCallback,
                   base::Passed(&store_)),
        base::Bind(&DeviceInfoServiceTest::CreateModelTypeChangeProcessor,
                   base::Unretained(this)));
    service_->AddObserver(this);
  }

  // Creates the service and runs any outstanding tasks. This will typically
  // cause all initialization callbacks between the sevice and store to fire.
  void InitializeAndPump() {
    InitializeService();
    base::RunLoop().RunUntilIdle();
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
    DeviceInfoSpecifics specifics = GenerateTestSpecifics();
    specifics.set_cache_guid(guid);
    return specifics;
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

  void RestartService() {
    PumpAndShutdown();
    InitializeAndPump();
  }

  void ForcePulse() { service()->SendLocalData(); }

  Time GetLastUpdateTime(const DeviceInfoSpecifics& specifics) {
    return DeviceInfoService::GetLastUpdateTime(specifics);
  }

 private:
  int change_count_;

  // In memory model type store needs a MessageLoop.
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
  DeviceInfoList devices = service()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));
}

TEST_F(DeviceInfoServiceTest, EmptyDataReconciliationSlowLoad) {
  InitializeService();
  ASSERT_EQ(0u, service()->GetAllDeviceInfo().size());
  base::RunLoop().RunUntilIdle();
  DeviceInfoList devices = service()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));
}

TEST_F(DeviceInfoServiceTest, LocalProviderSubscription) {
  set_local_device(base::MakeUnique<LocalDeviceInfoProviderMock>());
  InitializeAndPump();

  ASSERT_EQ(0u, service()->GetAllDeviceInfo().size());
  local_device()->Initialize(CreateDeviceInfo());
  base::RunLoop().RunUntilIdle();

  DeviceInfoList devices = service()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));
}

// Metadata shouldn't be loaded before the provider is initialized.
TEST_F(DeviceInfoServiceTest, LocalProviderInitRace) {
  set_local_device(base::MakeUnique<LocalDeviceInfoProviderMock>());
  InitializeAndPump();
  EXPECT_FALSE(processor()->metadata());

  ASSERT_EQ(0u, service()->GetAllDeviceInfo().size());
  local_device()->Initialize(CreateDeviceInfo());
  base::RunLoop().RunUntilIdle();

  DeviceInfoList devices = service()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));

  EXPECT_TRUE(processor()->metadata());
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
  DeviceInfoSpecifics specifics = GenerateTestSpecifics();
  store()->WriteData(batch.get(), specifics.cache_guid(),
                     specifics.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  ASSERT_EQ(2u, service()->GetAllDeviceInfo().size());
  AssertEqual(specifics,
              *service()->GetDeviceInfo(specifics.cache_guid()).get());
}

TEST_F(DeviceInfoServiceTest, TestWithLocalMetadata) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  ModelTypeState state;
  state.set_encryption_key_name("ekn");
  store()->WriteGlobalMetadata(batch.get(), state.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));
  InitializeAndPump();
  DeviceInfoList devices = service()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  ASSERT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));
  EXPECT_EQ(1u, processor()->put_multimap().size());
}

TEST_F(DeviceInfoServiceTest, TestWithLocalDataAndMetadata) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DeviceInfoSpecifics specifics = GenerateTestSpecifics();
  store()->WriteData(batch.get(), specifics.cache_guid(),
                     specifics.SerializeAsString());
  ModelTypeState state;
  state.set_encryption_key_name("ekn");
  store()->WriteGlobalMetadata(batch.get(), state.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  ASSERT_EQ(2u, service()->GetAllDeviceInfo().size());
  AssertEqual(specifics,
              *service()->GetDeviceInfo(specifics.cache_guid()).get());
  ASSERT_TRUE(processor()->metadata());
  ASSERT_EQ(state.encryption_key_name(),
            processor()->metadata()->GetModelTypeState().encryption_key_name());
}

TEST_F(DeviceInfoServiceTest, GetData) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DeviceInfoSpecifics specifics1 = GenerateTestSpecifics();
  DeviceInfoSpecifics specifics2 = GenerateTestSpecifics();
  DeviceInfoSpecifics specifics3 = GenerateTestSpecifics();
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
  expected[specifics1.cache_guid()] = specifics1;
  expected[specifics3.cache_guid()] = specifics3;
  StorageKeyList storage_keys;
  storage_keys.push_back(specifics1.cache_guid());
  storage_keys.push_back(specifics3.cache_guid());
  service()->GetData(storage_keys,
                     base::Bind(&AssertExpectedFromDataBatch, expected));
}

TEST_F(DeviceInfoServiceTest, GetDataMissing) {
  InitializeAndPump();
  std::map<std::string, DeviceInfoSpecifics> expected;
  StorageKeyList storage_keys;
  storage_keys.push_back("does_not_exist");
  service()->GetData(storage_keys,
                     base::Bind(&AssertExpectedFromDataBatch, expected));
}

TEST_F(DeviceInfoServiceTest, GetAllData) {
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  DeviceInfoSpecifics specifics1 = GenerateTestSpecifics();
  DeviceInfoSpecifics specifics2 = GenerateTestSpecifics();
  const std::string& guid1 = specifics1.cache_guid();
  const std::string& guid2 = specifics2.cache_guid();
  store()->WriteData(batch.get(), specifics1.cache_guid(),
                     specifics1.SerializeAsString());
  store()->WriteData(batch.get(), specifics2.cache_guid(),
                     specifics2.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  std::map<std::string, DeviceInfoSpecifics> expected;
  expected[guid1] = specifics1;
  expected[guid2] = specifics2;
  StorageKeyList storage_keys;
  storage_keys.push_back(guid1);
  storage_keys.push_back(guid2);
  service()->GetData(storage_keys,
                     base::Bind(&AssertExpectedFromDataBatch, expected));
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesEmpty) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());
  const SyncError error = service()->ApplySyncChanges(
      service()->CreateMetadataChangeList(), EntityChangeList());
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesInMemory) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());

  DeviceInfoSpecifics specifics = GenerateTestSpecifics();
  EntityChangeList add_changes;
  PushBackEntityChangeAdd(specifics, &add_changes);
  SyncError error = service()->ApplySyncChanges(
      service()->CreateMetadataChangeList(), add_changes);

  EXPECT_FALSE(error.IsSet());
  std::unique_ptr<DeviceInfo> info =
      service()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  AssertEqual(specifics, *info.get());
  EXPECT_EQ(2, change_count());

  EntityChangeList delete_changes;
  delete_changes.push_back(EntityChange::CreateDelete(specifics.cache_guid()));
  error = service()->ApplySyncChanges(service()->CreateMetadataChangeList(),
                                      delete_changes);

  EXPECT_FALSE(error.IsSet());
  EXPECT_FALSE(service()->GetDeviceInfo(specifics.cache_guid()));
  EXPECT_EQ(3, change_count());
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesStore) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());

  DeviceInfoSpecifics specifics = GenerateTestSpecifics();
  EntityChangeList data_changes;
  PushBackEntityChangeAdd(specifics, &data_changes);
  ModelTypeState state;
  state.set_encryption_key_name("ekn");
  std::unique_ptr<MetadataChangeList> metadata_changes =
      service()->CreateMetadataChangeList();
  metadata_changes->UpdateModelTypeState(state);

  const SyncError error =
      service()->ApplySyncChanges(std::move(metadata_changes), data_changes);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(2, change_count());

  RestartService();

  std::unique_ptr<DeviceInfo> info =
      service()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  AssertEqual(specifics, *info.get());

  EXPECT_TRUE(processor()->metadata());
  EXPECT_EQ(state.encryption_key_name(),
            processor()->metadata()->GetModelTypeState().encryption_key_name());
}

TEST_F(DeviceInfoServiceTest, ApplySyncChangesWithLocalGuid) {
  InitializeAndPump();

  // The point of this test is to try to apply remote changes that have the same
  // cache guid as the local device. The service should ignore these changes
  // since only it should be performing writes on its data.
  DeviceInfoSpecifics specifics =
      GenerateTestSpecifics(local_device()->GetLocalDeviceInfo()->guid());
  EntityChangeList change_list;
  PushBackEntityChangeAdd(specifics, &change_list);

  // Should have a single change from reconciliation.
  EXPECT_TRUE(
      service()->GetDeviceInfo(local_device()->GetLocalDeviceInfo()->guid()));
  EXPECT_EQ(1, change_count());
  // Ensure |last_updated| is about now, plus or minus a little bit.
  Time last_updated(ProtoTimeToTime(processor()
                                        ->put_multimap()
                                        .begin()
                                        ->second->specifics.device_info()
                                        .last_updated_timestamp()));
  EXPECT_LT(Time::Now() - TimeDelta::FromMinutes(1), last_updated);
  EXPECT_GT(Time::Now() + TimeDelta::FromMinutes(1), last_updated);

  EXPECT_FALSE(
      service()
          ->ApplySyncChanges(service()->CreateMetadataChangeList(), change_list)
          .IsSet());
  EXPECT_EQ(1, change_count());

  change_list.clear();
  change_list.push_back(EntityChange::CreateDelete(specifics.cache_guid()));
  EXPECT_FALSE(
      service()
          ->ApplySyncChanges(service()->CreateMetadataChangeList(), change_list)
          .IsSet());
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoServiceTest, ApplyDeleteNonexistent) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());
  EntityChangeList delete_changes;
  delete_changes.push_back(EntityChange::CreateDelete("guid"));
  const SyncError error = service()->ApplySyncChanges(
      service()->CreateMetadataChangeList(), delete_changes);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoServiceTest, MergeEmpty) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());
  const SyncError error = service()->MergeSyncData(
      service()->CreateMetadataChangeList(), EntityDataMap());
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(1, change_count());
  // TODO(skym): Stop sending local twice. The first of the two puts will
  // probably happen before the processor is tracking metadata yet, and so there
  // should not be much overhead.
  EXPECT_EQ(2u, processor()->put_multimap().size());
  EXPECT_EQ(2u, processor()->put_multimap().count(
                    local_device()->GetLocalDeviceInfo()->guid()));
  EXPECT_EQ(0u, processor()->delete_set().size());
}

TEST_F(DeviceInfoServiceTest, MergeWithData) {
  const std::string conflict_guid = "conflict_guid";
  const DeviceInfoSpecifics unique_local =
      GenerateTestSpecifics("unique_local_guid");
  const DeviceInfoSpecifics conflict_local =
      GenerateTestSpecifics(conflict_guid);
  const DeviceInfoSpecifics conflict_remote =
      GenerateTestSpecifics(conflict_guid);
  const DeviceInfoSpecifics unique_remote =
      GenerateTestSpecifics("unique_remote_guid");

  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  store()->WriteData(batch.get(), unique_local.cache_guid(),
                     unique_local.SerializeAsString());
  store()->WriteData(batch.get(), conflict_local.cache_guid(),
                     conflict_local.SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();
  EXPECT_EQ(1, change_count());

  EntityDataMap remote_input;
  remote_input[conflict_remote.cache_guid()] =
      SpecificsToEntity(conflict_remote);
  remote_input[unique_remote.cache_guid()] = SpecificsToEntity(unique_remote);

  ModelTypeState state;
  state.set_encryption_key_name("ekn");
  std::unique_ptr<MetadataChangeList> metadata_changes(
      service()->CreateMetadataChangeList());
  metadata_changes->UpdateModelTypeState(state);

  const SyncError error =
      service()->MergeSyncData(std::move(metadata_changes), remote_input);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(2, change_count());

  // The remote should beat the local in conflict.
  EXPECT_EQ(4u, service()->GetAllDeviceInfo().size());
  AssertEqual(unique_local,
              *service()->GetDeviceInfo(unique_local.cache_guid()).get());
  AssertEqual(unique_remote,
              *service()->GetDeviceInfo(unique_remote.cache_guid()).get());
  AssertEqual(conflict_remote, *service()->GetDeviceInfo(conflict_guid).get());

  // Service should have told the processor about the existance of unique_local.
  EXPECT_TRUE(processor()->delete_set().empty());
  EXPECT_EQ(3u, processor()->put_multimap().size());
  EXPECT_EQ(1u, processor()->put_multimap().count(unique_local.cache_guid()));
  const auto& it = processor()->put_multimap().find(unique_local.cache_guid());
  ASSERT_NE(processor()->put_multimap().end(), it);
  AssertEqual(unique_local, it->second->specifics.device_info());

  RestartService();
  ASSERT_EQ(state.encryption_key_name(),
            processor()->metadata()->GetModelTypeState().encryption_key_name());
}

TEST_F(DeviceInfoServiceTest, MergeLocalGuid) {
  const DeviceInfo* local_device_info = local_device()->GetLocalDeviceInfo();
  std::unique_ptr<DeviceInfoSpecifics> specifics =
      CopyToSpecifics(*local_device_info);
  specifics->set_last_updated_timestamp(TimeToProtoTime(Time::Now()));
  const std::string guid = local_device_info->guid();

  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  store()->WriteData(batch.get(), guid, specifics->SerializeAsString());
  store()->CommitWriteBatch(std::move(batch),
                            base::Bind(&AssertResultIsSuccess));

  InitializeAndPump();

  EntityDataMap remote_input;
  remote_input[guid] = SpecificsToEntity(*specifics);

  const SyncError error = service()->MergeSyncData(
      service()->CreateMetadataChangeList(), remote_input);
  EXPECT_FALSE(error.IsSet());
  EXPECT_EQ(0, change_count());
  EXPECT_EQ(1u, service()->GetAllDeviceInfo().size());
  EXPECT_TRUE(processor()->delete_set().empty());
  EXPECT_TRUE(processor()->put_multimap().empty());
}

TEST_F(DeviceInfoServiceTest, GetLastUpdateTime) {
  Time time1(Time() + TimeDelta::FromDays(1));

  DeviceInfoSpecifics specifics1 = GenerateTestSpecifics();
  DeviceInfoSpecifics specifics2 = GenerateTestSpecifics();
  specifics2.set_last_updated_timestamp(TimeToProtoTime(time1));

  EXPECT_EQ(Time(), GetLastUpdateTime(specifics1));
  EXPECT_EQ(time1, GetLastUpdateTime(specifics2));
}

TEST_F(DeviceInfoServiceTest, CountActiveDevices) {
  InitializeAndPump();
  EXPECT_EQ(1, service()->CountActiveDevices());

  DeviceInfoSpecifics specifics =
      GenerateTestSpecifics(local_device()->GetLocalDeviceInfo()->guid());
  EntityChangeList change_list;
  PushBackEntityChangeAdd(specifics, &change_list);
  service()->ApplySyncChanges(service()->CreateMetadataChangeList(),
                              change_list);
  EXPECT_EQ(1, service()->CountActiveDevices());

  change_list.clear();
  specifics.set_last_updated_timestamp(TimeToProtoTime(Time::Now()));
  PushBackEntityChangeAdd(specifics, &change_list);
  service()->ApplySyncChanges(service()->CreateMetadataChangeList(),
                              change_list);
  EXPECT_EQ(1, service()->CountActiveDevices());

  change_list.clear();
  specifics.set_cache_guid("non-local");
  PushBackEntityChangeAdd(specifics, &change_list);
  service()->ApplySyncChanges(service()->CreateMetadataChangeList(),
                              change_list);
  EXPECT_EQ(2, service()->CountActiveDevices());
}

TEST_F(DeviceInfoServiceTest, MultipleOnProviderInitialized) {
  set_local_device(base::MakeUnique<LocalDeviceInfoProviderMock>());
  InitializeAndPump();
  EXPECT_EQ(nullptr, processor()->metadata());

  // Verify the processor was given metadata.
  local_device()->Initialize(CreateDeviceInfo());
  base::RunLoop().RunUntilIdle();
  const MetadataBatch* metadata = processor()->metadata();
  EXPECT_NE(nullptr, metadata);

  // Pointer address of metadata should remain constant because the processor
  // should not have been given new metadata.
  local_device()->Initialize(CreateDeviceInfo());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(metadata, processor()->metadata());
}

TEST_F(DeviceInfoServiceTest, SendLocalData) {
  InitializeAndPump();
  EXPECT_EQ(1, service()->CountActiveDevices());
  EXPECT_EQ(1, change_count());
  EXPECT_EQ(1u, processor()->put_multimap().size());

  ForcePulse();
  EXPECT_EQ(1, service()->CountActiveDevices());
  EXPECT_EQ(2, change_count());
  EXPECT_EQ(2u, processor()->put_multimap().size());

  local_device()->Clear();
  ForcePulse();
  EXPECT_EQ(1, service()->CountActiveDevices());
  EXPECT_EQ(2, change_count());
  EXPECT_EQ(2u, processor()->put_multimap().size());
}

}  // namespace

}  // namespace syncer
