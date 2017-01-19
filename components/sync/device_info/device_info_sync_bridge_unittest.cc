// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/device_info_sync_bridge.h"

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
#include "components/sync/model/recording_model_type_change_processor.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using base::OneShotTimer;
using base::Time;
using base::TimeDelta;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;
using sync_pb::ModelTypeState;

using DeviceInfoList = std::vector<std::unique_ptr<DeviceInfo>>;
using StorageKeyList = ModelTypeSyncBridge::StorageKeyList;
using RecordList = ModelTypeStore::RecordList;
using Result = ModelTypeStore::Result;
using StartCallback = ModelTypeChangeProcessor::StartCallback;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

const char kGuidFormat[] = "cache guid %d";
const char kClientNameFormat[] = "client name %d";
const char kChromeVersionFormat[] = "chrome version %d";
const char kSyncUserAgentFormat[] = "sync user agent %d";
const char kSigninScopedDeviceIdFormat[] = "signin scoped device id %d";
const sync_pb::SyncEnums::DeviceType kDeviceType =
    sync_pb::SyncEnums_DeviceType_TYPE_LINUX;

// The |provider_| is first initialized with a model object created with this
// suffix. Local suffix can be changed by setting the provider and then
// initializing. Remote data should use other suffixes.
const int kDefaultLocalSuffix = 0;

DeviceInfoSpecifics CreateSpecifics(int suffix) {
  DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(base::StringPrintf(kGuidFormat, suffix));
  specifics.set_client_name(base::StringPrintf(kClientNameFormat, suffix));
  specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  specifics.set_sync_user_agent(
      base::StringPrintf(kSyncUserAgentFormat, suffix));
  specifics.set_chrome_version(
      base::StringPrintf(kChromeVersionFormat, suffix));
  specifics.set_signin_scoped_device_id(
      base::StringPrintf(kSigninScopedDeviceIdFormat, suffix));
  return specifics;
}

DeviceInfoSpecifics CreateSpecifics(int suffix, Time last_updated_timestamp) {
  DeviceInfoSpecifics specifics = CreateSpecifics(suffix);
  specifics.set_last_updated_timestamp(TimeToProtoTime(last_updated_timestamp));
  return specifics;
}

std::unique_ptr<DeviceInfo> CreateModel(int suffix) {
  return base::MakeUnique<DeviceInfo>(
      base::StringPrintf(kGuidFormat, suffix),
      base::StringPrintf(kClientNameFormat, suffix),
      base::StringPrintf(kChromeVersionFormat, suffix),
      base::StringPrintf(kSyncUserAgentFormat, suffix), kDeviceType,
      base::StringPrintf(kSigninScopedDeviceIdFormat, suffix));
}

ModelTypeState StateWithEncryption(const std::string& encryption_key_name) {
  ModelTypeState state;
  state.set_encryption_key_name(encryption_key_name);
  return state;
}

void VerifyResultIsSuccess(Result result) {
  EXPECT_EQ(Result::SUCCESS, result);
}

void VerifyEqual(const DeviceInfoSpecifics& s1, const DeviceInfoSpecifics& s2) {
  EXPECT_EQ(s1.cache_guid(), s2.cache_guid());
  EXPECT_EQ(s1.client_name(), s2.client_name());
  EXPECT_EQ(s1.device_type(), s2.device_type());
  EXPECT_EQ(s1.sync_user_agent(), s2.sync_user_agent());
  EXPECT_EQ(s1.chrome_version(), s2.chrome_version());
  EXPECT_EQ(s1.signin_scoped_device_id(), s2.signin_scoped_device_id());
}

void VerifyEqual(const DeviceInfoSpecifics& specifics,
                 const DeviceInfo& model) {
  EXPECT_EQ(specifics.cache_guid(), model.guid());
  EXPECT_EQ(specifics.client_name(), model.client_name());
  EXPECT_EQ(specifics.device_type(), model.device_type());
  EXPECT_EQ(specifics.sync_user_agent(), model.sync_user_agent());
  EXPECT_EQ(specifics.chrome_version(), model.chrome_version());
  EXPECT_EQ(specifics.signin_scoped_device_id(),
            model.signin_scoped_device_id());
}

void VerifyDataBatch(std::map<std::string, DeviceInfoSpecifics> expected,
                     std::unique_ptr<DataBatch> batch) {
  while (batch->HasNext()) {
    const KeyAndData& pair = batch->Next();
    auto iter = expected.find(pair.first);
    ASSERT_NE(iter, expected.end());
    VerifyEqual(iter->second, pair.second->specifics.device_info());
    // Removing allows us to verify we don't see the same item multiple times,
    // and that we saw everything we expected.
    expected.erase(iter);
  }
  EXPECT_TRUE(expected.empty());
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
// specifics objects in an EntityData and EntityChange of type ACTION_ADD, and
// returns an EntityChangeList containing them all. Order is maintained.
EntityChangeList EntityAddList(
    const std::vector<DeviceInfoSpecifics>& specifics_list) {
  EntityChangeList changes;
  for (const auto& specifics : specifics_list) {
    changes.push_back(EntityChange::CreateAdd(specifics.cache_guid(),
                                              SpecificsToEntity(specifics)));
  }
  return changes;
}

// Similar helper to EntityAddList(...), only wraps in a EntityDataMap for a
// merge call. Order is irrelevant, since the map sorts by key. Should not
// contain multiple specifics with the same guid.
EntityDataMap InlineEntityDataMap(
    const std::vector<DeviceInfoSpecifics>& specifics_list) {
  EntityDataMap map;
  for (const auto& specifics : specifics_list) {
    EXPECT_EQ(map.end(), map.find(specifics.cache_guid()));
    map[specifics.cache_guid()] = SpecificsToEntity(specifics);
  }
  return map;
}

}  // namespace

class DeviceInfoSyncBridgeTest : public testing::Test,
                                 public DeviceInfoTracker::Observer {
 protected:
  DeviceInfoSyncBridgeTest()
      : store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        provider_(new LocalDeviceInfoProviderMock()) {
    provider_->Initialize(CreateModel(kDefaultLocalSuffix));
  }

  ~DeviceInfoSyncBridgeTest() override {
    // Some tests may never initialize the bridge.
    if (bridge_)
      bridge_->RemoveObserver(this);

    // Force all remaining (store) tasks to execute so we don't leak memory.
    base::RunLoop().RunUntilIdle();
  }

  void OnDeviceInfoChange() override { change_count_++; }

  std::unique_ptr<ModelTypeChangeProcessor> CreateModelTypeChangeProcessor(
      ModelType type,
      ModelTypeSyncBridge* bridge) {
    auto processor = base::MakeUnique<RecordingModelTypeChangeProcessor>();
    processor_ = processor.get();
    return std::move(processor);
  }

  // Initialized the bridge based on the current local device and store. Can
  // only be called once per run, as it passes |store_|.
  void InitializeBridge() {
    ASSERT_TRUE(store_);
    bridge_ = base::MakeUnique<DeviceInfoSyncBridge>(
        provider_.get(),
        base::Bind(&ModelTypeStoreTestUtil::MoveStoreToCallback,
                   base::Passed(&store_)),
        base::Bind(&DeviceInfoSyncBridgeTest::CreateModelTypeChangeProcessor,
                   base::Unretained(this)));
    bridge_->AddObserver(this);
  }

  // Creates the bridge and runs any outstanding tasks. This will typically
  // cause all initialization callbacks between the sevice and store to fire.
  void InitializeAndPump() {
    InitializeBridge();
    base::RunLoop().RunUntilIdle();
  }

  // Allows access to the store before that will ultimately be used to
  // initialize the bridge.
  ModelTypeStore* store() {
    EXPECT_TRUE(store_);
    return store_.get();
  }

  // Get the number of times the bridge notifies observers of changes.
  int change_count() { return change_count_; }

  // Allows overriding the provider before the bridge is initialized.
  void set_provider(std::unique_ptr<LocalDeviceInfoProviderMock> provider) {
    ASSERT_FALSE(bridge_);
    std::swap(provider_, provider);
  }
  LocalDeviceInfoProviderMock* local_device() { return provider_.get(); }

  // Allows access to the bridge after InitializeBridge() is called.
  DeviceInfoSyncBridge* bridge() {
    EXPECT_TRUE(bridge_);
    return bridge_.get();
  }

  RecordingModelTypeChangeProcessor* processor() {
    EXPECT_TRUE(processor_);
    return processor_;
  }

  const OneShotTimer& pulse_timer() { return bridge()->pulse_timer_; }

  // Should only be called after the bridge has been initialized. Will first
  // recover the bridge's store, so another can be initialized later, and then
  // deletes the bridge.
  void PumpAndShutdown() {
    ASSERT_TRUE(bridge_);
    base::RunLoop().RunUntilIdle();
    std::swap(store_, bridge_->store_);
    bridge_->RemoveObserver(this);
    bridge_.reset();
  }

  void RestartBridge() {
    PumpAndShutdown();
    InitializeAndPump();
  }

  void ForcePulse() { bridge()->SendLocalData(); }

  void WriteToStore(const std::vector<DeviceInfoSpecifics>& specifics_list,
                    std::unique_ptr<WriteBatch> batch) {
    // The bridge only reads from the store during initialization, so if the
    // |bridge_| has already been initialized, then it may not have an effect.
    EXPECT_FALSE(bridge_);
    for (auto& specifics : specifics_list) {
      batch->WriteData(specifics.cache_guid(), specifics.SerializeAsString());
    }
    store()->CommitWriteBatch(std::move(batch),
                              base::Bind(&VerifyResultIsSuccess));
  }

  void WriteToStore(const std::vector<DeviceInfoSpecifics>& specifics_list,
                    ModelTypeState state) {
    std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
    batch->GetMetadataChangeList()->UpdateModelTypeState(state);
    WriteToStore(specifics_list, std::move(batch));
  }

  void WriteToStore(const std::vector<DeviceInfoSpecifics>& specifics_list) {
    WriteToStore(specifics_list, store()->CreateWriteBatch());
  }

 private:
  int change_count_ = 0;

  // In memory model type store needs a MessageLoop.
  base::MessageLoop message_loop_;

  // Holds the store while the bridge is not initialized.
  std::unique_ptr<ModelTypeStore> store_;

  // Provides information about the local device. Is initialized in each case's
  // constructor with a model object created from |kDefaultLocalSuffix|.
  std::unique_ptr<LocalDeviceInfoProviderMock> provider_;

  // Not initialized immediately (upon test's constructor). This allows each
  // test case to modify the dependencies the bridge will be constructed with.
  std::unique_ptr<DeviceInfoSyncBridge> bridge_;

  // A non-owning pointer to the processor given to the bridge. Will be null
  // before being given to the bridge, to make ownership easier.
  RecordingModelTypeChangeProcessor* processor_ = nullptr;
};

namespace {

TEST_F(DeviceInfoSyncBridgeTest, EmptyDataReconciliation) {
  InitializeAndPump();
  const DeviceInfoList devices = bridge()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  EXPECT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));
}

TEST_F(DeviceInfoSyncBridgeTest, EmptyDataReconciliationSlowLoad) {
  InitializeBridge();
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
  base::RunLoop().RunUntilIdle();
  const DeviceInfoList devices = bridge()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  EXPECT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));
}

TEST_F(DeviceInfoSyncBridgeTest, LocalProviderSubscription) {
  set_provider(base::MakeUnique<LocalDeviceInfoProviderMock>());
  InitializeAndPump();

  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
  local_device()->Initialize(CreateModel(1));
  base::RunLoop().RunUntilIdle();

  const DeviceInfoList devices = bridge()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  EXPECT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));
}

// Metadata shouldn't be loaded before the provider is initialized.
TEST_F(DeviceInfoSyncBridgeTest, LocalProviderInitRace) {
  set_provider(base::MakeUnique<LocalDeviceInfoProviderMock>());
  InitializeAndPump();
  EXPECT_FALSE(processor()->metadata());

  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
  local_device()->Initialize(CreateModel(1));
  base::RunLoop().RunUntilIdle();

  DeviceInfoList devices = bridge()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  EXPECT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));

  EXPECT_TRUE(processor()->metadata());
}

// Simulate shutting down sync during the ModelTypeStore callbacks. The pulse
// timer should still be initialized, even though reconcile never occurs.
TEST_F(DeviceInfoSyncBridgeTest, ClearProviderDuringInit) {
  InitializeBridge();
  local_device()->Clear();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
  EXPECT_TRUE(pulse_timer().IsRunning());
}

TEST_F(DeviceInfoSyncBridgeTest, GetClientTagNormal) {
  InitializeBridge();
  const std::string guid = "abc";
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info()->set_cache_guid(guid);
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(CacheGuidToTag(guid), bridge()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoSyncBridgeTest, GetClientTagEmpty) {
  InitializeBridge();
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info();
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(CacheGuidToTag(""), bridge()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalData) {
  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  WriteToStore({specifics});
  InitializeAndPump();

  ASSERT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  VerifyEqual(specifics,
              *bridge()->GetDeviceInfo(specifics.cache_guid()).get());
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalMetadata) {
  WriteToStore(std::vector<DeviceInfoSpecifics>(), StateWithEncryption("ekn"));
  InitializeAndPump();

  const DeviceInfoList devices = bridge()->GetAllDeviceInfo();
  ASSERT_EQ(1u, devices.size());
  EXPECT_TRUE(local_device()->GetLocalDeviceInfo()->Equals(*devices[0]));
  EXPECT_EQ(1u, processor()->put_multimap().size());
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalDataAndMetadata) {
  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  ModelTypeState state = StateWithEncryption("ekn");
  WriteToStore({specifics}, state);
  InitializeAndPump();

  ASSERT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  VerifyEqual(specifics,
              *bridge()->GetDeviceInfo(specifics.cache_guid()).get());
  EXPECT_TRUE(processor()->metadata());
  EXPECT_EQ(state.encryption_key_name(),
            processor()->metadata()->GetModelTypeState().encryption_key_name());
}

TEST_F(DeviceInfoSyncBridgeTest, GetData) {
  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  const DeviceInfoSpecifics specifics3 = CreateSpecifics(3);
  WriteToStore({specifics1, specifics2, specifics3});
  InitializeAndPump();

  const std::map<std::string, DeviceInfoSpecifics> expected{
      {specifics1.cache_guid(), specifics1},
      {specifics3.cache_guid(), specifics3}};
  bridge()->GetData({specifics1.cache_guid(), specifics3.cache_guid()},
                    base::Bind(&VerifyDataBatch, expected));
}

TEST_F(DeviceInfoSyncBridgeTest, GetDataMissing) {
  InitializeAndPump();
  bridge()->GetData({"does_not_exist"},
                    base::Bind(&VerifyDataBatch,
                               std::map<std::string, DeviceInfoSpecifics>()));
}

TEST_F(DeviceInfoSyncBridgeTest, GetAllData) {
  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  WriteToStore({specifics1, specifics2});
  InitializeAndPump();

  const std::map<std::string, DeviceInfoSpecifics> expected{
      {specifics1.cache_guid(), specifics1},
      {specifics2.cache_guid(), specifics2}};
  bridge()->GetData({specifics1.cache_guid(), specifics2.cache_guid()},
                    base::Bind(&VerifyDataBatch, expected));
}

TEST_F(DeviceInfoSyncBridgeTest, ApplySyncChangesEmpty) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());
  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityChangeList());
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplySyncChangesInMemory) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  auto error_on_add = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));

  EXPECT_FALSE(error_on_add);
  std::unique_ptr<DeviceInfo> info =
      bridge()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  VerifyEqual(specifics, *info.get());
  EXPECT_EQ(2, change_count());

  auto error_on_delete = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      {EntityChange::CreateDelete(specifics.cache_guid())});

  EXPECT_FALSE(error_on_delete);
  EXPECT_FALSE(bridge()->GetDeviceInfo(specifics.cache_guid()));
  EXPECT_EQ(3, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplySyncChangesStore) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  ModelTypeState state = StateWithEncryption("ekn");
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  metadata_changes->UpdateModelTypeState(state);

  auto error = bridge()->ApplySyncChanges(std::move(metadata_changes),
                                          EntityAddList({specifics}));
  EXPECT_FALSE(error);
  EXPECT_EQ(2, change_count());

  RestartBridge();

  std::unique_ptr<DeviceInfo> info =
      bridge()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  VerifyEqual(specifics, *info.get());

  EXPECT_TRUE(processor()->metadata());
  EXPECT_EQ(state.encryption_key_name(),
            processor()->metadata()->GetModelTypeState().encryption_key_name());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplySyncChangesWithLocalGuid) {
  InitializeAndPump();

  // The bridge should ignore these changes using this specifics because its
  // guid will match the local device.
  const DeviceInfoSpecifics specifics = CreateSpecifics(kDefaultLocalSuffix);

  // Should have a single change from reconciliation.
  EXPECT_TRUE(
      bridge()->GetDeviceInfo(local_device()->GetLocalDeviceInfo()->guid()));
  EXPECT_EQ(1, change_count());
  // Ensure |last_updated| is about now, plus or minus a little bit.
  const Time last_updated(ProtoTimeToTime(processor()
                                              ->put_multimap()
                                              .begin()
                                              ->second->specifics.device_info()
                                              .last_updated_timestamp()));
  EXPECT_LT(Time::Now() - TimeDelta::FromMinutes(1), last_updated);
  EXPECT_GT(Time::Now() + TimeDelta::FromMinutes(1), last_updated);

  auto error_on_add = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));
  EXPECT_FALSE(error_on_add);
  EXPECT_EQ(1, change_count());

  auto error_on_delete = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      {EntityChange::CreateDelete(specifics.cache_guid())});
  EXPECT_FALSE(error_on_delete);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyDeleteNonexistent) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());
  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          {EntityChange::CreateDelete("guid")});
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ClearProviderAndApply) {
  // This will initialize the provider a first time.
  InitializeAndPump();
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1, Time::Now());

  local_device()->Clear();
  auto error1 = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                           EntityAddList({specifics}));
  EXPECT_FALSE(error1);
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());

  local_device()->Initialize(CreateModel(kDefaultLocalSuffix));
  auto error2 = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                           EntityAddList({specifics}));
  EXPECT_FALSE(error2);
  EXPECT_EQ(2u, bridge()->GetAllDeviceInfo().size());
}

TEST_F(DeviceInfoSyncBridgeTest, MergeEmpty) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());
  auto error = bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                                       EntityDataMap());
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());
  // TODO(skym): Stop sending local twice. The first of the two puts will
  // probably happen before the processor is tracking metadata yet, and so there
  // should not be much overhead.
  EXPECT_EQ(2u, processor()->put_multimap().size());
  EXPECT_EQ(2u, processor()->put_multimap().count(
                    local_device()->GetLocalDeviceInfo()->guid()));
  EXPECT_EQ(0u, processor()->delete_set().size());
}

TEST_F(DeviceInfoSyncBridgeTest, MergeWithData) {
  const DeviceInfoSpecifics unique_local = CreateSpecifics(1);
  DeviceInfoSpecifics conflict_local = CreateSpecifics(2);
  DeviceInfoSpecifics conflict_remote = CreateSpecifics(3);
  const DeviceInfoSpecifics unique_remote = CreateSpecifics(4);

  const std::string conflict_guid = "conflict_guid";
  conflict_local.set_cache_guid(conflict_guid);
  conflict_remote.set_cache_guid(conflict_guid);

  WriteToStore({unique_local, conflict_local});
  InitializeAndPump();
  EXPECT_EQ(1, change_count());

  ModelTypeState state = StateWithEncryption("ekn");
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  metadata_changes->UpdateModelTypeState(state);

  auto error = bridge()->MergeSyncData(
      std::move(metadata_changes),
      InlineEntityDataMap({conflict_remote, unique_remote}));
  EXPECT_FALSE(error);
  EXPECT_EQ(2, change_count());

  // The remote should beat the local in conflict.
  EXPECT_EQ(4u, bridge()->GetAllDeviceInfo().size());
  VerifyEqual(unique_local,
              *bridge()->GetDeviceInfo(unique_local.cache_guid()).get());
  VerifyEqual(unique_remote,
              *bridge()->GetDeviceInfo(unique_remote.cache_guid()).get());
  VerifyEqual(conflict_remote, *bridge()->GetDeviceInfo(conflict_guid).get());

  // bridge should have told the processor about the existance of unique_local.
  EXPECT_TRUE(processor()->delete_set().empty());
  EXPECT_EQ(3u, processor()->put_multimap().size());
  EXPECT_EQ(1u, processor()->put_multimap().count(unique_local.cache_guid()));
  const auto& it = processor()->put_multimap().find(unique_local.cache_guid());
  ASSERT_NE(processor()->put_multimap().end(), it);
  VerifyEqual(unique_local, it->second->specifics.device_info());

  RestartBridge();
  EXPECT_EQ(state.encryption_key_name(),
            processor()->metadata()->GetModelTypeState().encryption_key_name());
}

TEST_F(DeviceInfoSyncBridgeTest, MergeLocalGuid) {
  // If not recent, then reconcile is going to try to send an updated version to
  // Sync, which makes interpreting change_count() more difficult.
  const DeviceInfoSpecifics specifics =
      CreateSpecifics(kDefaultLocalSuffix, Time::Now());
  WriteToStore({specifics});
  InitializeAndPump();

  auto error = bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                                       InlineEntityDataMap({specifics}));
  EXPECT_FALSE(error);
  EXPECT_EQ(0, change_count());
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  EXPECT_TRUE(processor()->delete_set().empty());
  EXPECT_TRUE(processor()->put_multimap().empty());
}

TEST_F(DeviceInfoSyncBridgeTest, MergeLocalGuidBeforeReconcile) {
  InitializeBridge();

  // The message loop is never pumped, which means local data/metadata is never
  // loaded, and thus reconcile is never called. The bridge should ignore this
  // EntityData because its cache guid is the same the local device's.
  auto error = bridge()->MergeSyncData(
      bridge()->CreateMetadataChangeList(),
      InlineEntityDataMap({CreateSpecifics(kDefaultLocalSuffix)}));
  EXPECT_FALSE(error);
  EXPECT_EQ(0, change_count());
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
}

TEST_F(DeviceInfoSyncBridgeTest, ClearProviderAndMerge) {
  // This will initialize the provider a first time.
  InitializeAndPump();
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1, Time::Now());

  local_device()->Clear();
  auto error1 = bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                                        InlineEntityDataMap({specifics}));
  EXPECT_FALSE(error1);
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());

  local_device()->Initialize(CreateModel(kDefaultLocalSuffix));
  auto error2 = bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                                        InlineEntityDataMap({specifics}));
  EXPECT_FALSE(error2);
  EXPECT_EQ(2u, bridge()->GetAllDeviceInfo().size());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevices) {
  InitializeAndPump();
  EXPECT_EQ(1, bridge()->CountActiveDevices());

  // Regardless of the time, these following two ApplySyncChanges(...) calls
  // have the same guid as the local device.
  bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({CreateSpecifics(kDefaultLocalSuffix)}));
  EXPECT_EQ(1, bridge()->CountActiveDevices());

  bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({CreateSpecifics(kDefaultLocalSuffix, Time::Now())}));
  EXPECT_EQ(1, bridge()->CountActiveDevices());

  // A different guid will actually contribute to the count.
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({CreateSpecifics(1, Time::Now())}));
  EXPECT_EQ(2, bridge()->CountActiveDevices());

  // Now set time to long ago in the past, it should not be active anymore.
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({CreateSpecifics(1)}));
  EXPECT_EQ(1, bridge()->CountActiveDevices());
}

TEST_F(DeviceInfoSyncBridgeTest, MultipleOnProviderInitialized) {
  set_provider(base::MakeUnique<LocalDeviceInfoProviderMock>());
  InitializeAndPump();
  EXPECT_EQ(nullptr, processor()->metadata());

  // Verify the processor was given metadata.
  local_device()->Initialize(CreateModel(0));
  base::RunLoop().RunUntilIdle();
  const MetadataBatch* metadata = processor()->metadata();
  EXPECT_NE(nullptr, metadata);

  // Pointer address of metadata should remain constant because the processor
  // should not have been given new metadata.
  local_device()->Initialize(CreateModel(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(metadata, processor()->metadata());
}

TEST_F(DeviceInfoSyncBridgeTest, SendLocalData) {
  InitializeAndPump();
  EXPECT_EQ(1, change_count());
  EXPECT_EQ(1u, processor()->put_multimap().size());

  ForcePulse();
  EXPECT_EQ(2, change_count());
  EXPECT_EQ(2u, processor()->put_multimap().size());

  // After clearing, pulsing should no-op and not result in a processor put or
  // a notification to observers.
  local_device()->Clear();
  ForcePulse();
  EXPECT_EQ(2, change_count());
  EXPECT_EQ(2u, processor()->put_multimap().size());
}

TEST_F(DeviceInfoSyncBridgeTest, DisableSync) {
  InitializeAndPump();
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(1, change_count());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityAddList({specifics}));

  EXPECT_FALSE(error);
  EXPECT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(2, change_count());

  // Should clear out all local data and notify observers.
  bridge()->DisableSync();
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(3, change_count());

  // Reloading from storage shouldn't contain remote data.
  RestartBridge();
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(4, change_count());
}

}  // namespace

}  // namespace syncer
