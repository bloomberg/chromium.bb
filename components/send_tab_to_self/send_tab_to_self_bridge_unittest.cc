// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "components/send_tab_to_self/proto/send_tab_to_self.pb.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

namespace {

using testing::_;
using testing::IsEmpty;
using testing::Return;
using testing::SizeIs;

const char kGuidFormat[] = "guid %d";
const char kURLFormat[] = "https://www.url%d.com/";
const char kTitleFormat[] = "title %d";
const char kDeviceFormat[] = "device %d";

sync_pb::SendTabToSelfSpecifics CreateSpecifics(
    int suffix,
    base::Time shared_time = base::Time::Now(),
    base::Time navigation_time = base::Time::Now()) {
  sync_pb::SendTabToSelfSpecifics specifics;
  specifics.set_guid(base::StringPrintf(kGuidFormat, suffix));
  specifics.set_url(base::StringPrintf(kURLFormat, suffix));
  specifics.set_device_name(base::StringPrintf(kDeviceFormat, suffix));
  specifics.set_title(base::StringPrintf(kTitleFormat, suffix));
  specifics.set_shared_time_usec(
      shared_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_navigation_time_usec(
      navigation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return specifics;
}

sync_pb::ModelTypeState StateWithEncryption(
    const std::string& encryption_key_name) {
  sync_pb::ModelTypeState state;
  state.set_encryption_key_name(encryption_key_name);
  return state;
}
class MockSendTabToSelfModelObserver : public SendTabToSelfModelObserver {
 public:
  MOCK_METHOD0(SendTabToSelfModelLoaded, void());
  MOCK_METHOD1(EntriesAddedRemotely,
               void(const std::vector<const SendTabToSelfEntry*>&));

  MOCK_METHOD1(EntriesRemovedRemotely, void(const std::vector<std::string>&));
};

class SendTabToSelfBridgeTest : public testing::Test {
 protected:
  SendTabToSelfBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    provider_.Initialize("cache_guid", "machine");
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
  }

  // Initialized the bridge based on the current local device and store. Can
  // only be called once per run, as it passes |store_|.
  void InitializeBridge() {
    bridge_ = std::make_unique<SendTabToSelfBridge>(
        mock_processor_.CreateForwardingProcessor(), &provider_, &clock_,
        syncer::ModelTypeStoreTestUtil::MoveStoreToFactory(std::move(store_)));
    bridge_->AddObserver(&mock_observer_);
    base::RunLoop().RunUntilIdle();
  }

  void ShutdownBridge() {
    bridge_->RemoveObserver(&mock_observer_);
    store_ =
        SendTabToSelfBridge::DestroyAndStealStoreForTest(std::move(bridge_));
    base::RunLoop().RunUntilIdle();
  }

  base::Time AdvanceAndGetTime() {
    clock_.Advance(base::TimeDelta::FromMilliseconds(10));
    return clock_.Now();
  }

  syncer::EntityDataPtr MakeEntityData(const SendTabToSelfEntry& entry) {
    SendTabToSelfLocal specifics = entry.AsLocalProto();

    auto entity_data = std::make_unique<syncer::EntityData>();

    *(entity_data->specifics.mutable_send_tab_to_self()) =
        specifics.specifics();
    entity_data->non_unique_name = entry.GetURL().spec();
    // The client_tag_hash field is unused by the send_tab_to_self_bridge, but
    // is required for a valid entity_data.
    entity_data->client_tag_hash = "someclienttaghash";
    return entity_data->PassToPtr();
  }

  // Helper method to reduce duplicated code between tests. Wraps the given
  // specifics objects in an EntityData and EntityChange of type ACTION_ADD, and
  // returns an EntityChangeList containing them all. Order is maintained.
  syncer::EntityChangeList EntityAddList(
      const std::vector<sync_pb::SendTabToSelfSpecifics>& specifics_list) {
    syncer::EntityChangeList changes;
    for (const auto& specifics : specifics_list) {
      auto entity_data = std::make_unique<syncer::EntityData>();

      *(entity_data->specifics.mutable_send_tab_to_self()) = specifics;
      entity_data->non_unique_name = specifics.url();
      // The client_tag_hash field is unused by the send_tab_to_self_bridge, but
      // is required for a valid entity_data.
      entity_data->client_tag_hash = "someclienttaghash";

      changes.push_back(syncer::EntityChange::CreateAdd(
          specifics.guid(), entity_data->PassToPtr()));
    }
    return changes;
  }

  // For Model Tests.
  void AddSampleEntries() {
    bridge_->AddEntry(GURL("http://a.com"), "a");
    bridge_->AddEntry(GURL("http://b.com"), "b");
    bridge_->AddEntry(GURL("http://c.com"), "c");
    bridge_->AddEntry(GURL("http://d.com"), "d");
  }

  syncer::MockModelTypeChangeProcessor* processor() { return &mock_processor_; }

  SendTabToSelfBridge* bridge() { return bridge_.get(); }
  MockSendTabToSelfModelObserver* mock_observer() { return &mock_observer_; }

 private:
  base::SimpleTestClock clock_;

  // In memory model type store needs to be able to post tasks.
  base::test::ScopedTaskEnvironment task_environment_;

  syncer::LocalDeviceInfoProviderMock provider_;

  std::unique_ptr<syncer::ModelTypeStore> store_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;

  std::unique_ptr<SendTabToSelfBridge> bridge_;

  testing::NiceMock<MockSendTabToSelfModelObserver> mock_observer_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfBridgeTest);
};

TEST_F(SendTabToSelfBridgeTest, CheckEmpties) {
  InitializeBridge();

  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(_)).Times(0);
  EXPECT_EQ(0ul, bridge()->GetAllGuids().size());
  AddSampleEntries();
  EXPECT_EQ(4ul, bridge()->GetAllGuids().size());
}

TEST_F(SendTabToSelfBridgeTest, SyncAddOneEntry) {
  InitializeBridge();
  syncer::EntityChangeList remote_input;

  SendTabToSelfEntry entry("guid1", GURL("http://www.example.com/"), "title",
                           AdvanceAndGetTime(), AdvanceAndGetTime(), "device");

  remote_input.push_back(
      syncer::EntityChange::CreateAdd("guid1", MakeEntityData(entry)));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(SizeIs(1)));
  bridge()->MergeSyncData(std::move(metadata_change_list), remote_input);
  EXPECT_EQ(1ul, bridge()->GetAllGuids().size());
}

TEST_F(SendTabToSelfBridgeTest, ApplySyncChangesAddTwoSpecifics) {
  InitializeBridge();

  const sync_pb::SendTabToSelfSpecifics specifics1 = CreateSpecifics(1);
  const sync_pb::SendTabToSelfSpecifics specifics2 = CreateSpecifics(2);

  sync_pb::ModelTypeState state = StateWithEncryption("ekn");
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  metadata_changes->UpdateModelTypeState(state);

  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(SizeIs(2)));

  auto error = bridge()->ApplySyncChanges(
      std::move(metadata_changes), EntityAddList({specifics1, specifics2}));
  EXPECT_FALSE(error);
}

TEST_F(SendTabToSelfBridgeTest, ApplySyncChangesOneAdd) {
  InitializeBridge();
  SendTabToSelfEntry entry("guid1", GURL("http://www.example.com/"), "title",
                           AdvanceAndGetTime(), AdvanceAndGetTime(), "device");

  syncer::EntityChangeList add_changes;

  add_changes.push_back(
      syncer::EntityChange::CreateAdd("guid1", MakeEntityData(entry)));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();

  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(SizeIs(1)));
  bridge()->ApplySyncChanges(std::move(metadata_change_list), add_changes);
  EXPECT_EQ(1ul, bridge()->GetAllGuids().size());
}

// Tests that the send tab to self entry is correctly removed.
TEST_F(SendTabToSelfBridgeTest, ApplySyncChangesOneDeletion) {
  InitializeBridge();
  SendTabToSelfEntry entry("guid1", GURL("http://www.example.com/"), "title",
                           AdvanceAndGetTime(), AdvanceAndGetTime(), "device");

  syncer::EntityChangeList add_changes;

  add_changes.push_back(
      syncer::EntityChange::CreateAdd("guid1", MakeEntityData(entry)));

  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(SizeIs(1)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(), add_changes);
  EXPECT_EQ(1ul, bridge()->GetAllGuids().size());
  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(syncer::EntityChange::CreateDelete("guid1"));

  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(SizeIs(1)));
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             delete_changes);
  EXPECT_EQ(0ul, bridge()->GetAllGuids().size());
}

TEST_F(SendTabToSelfBridgeTest, ApplySyncChangesEmpty) {
  InitializeBridge();
  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(_)).Times(0);

  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          syncer::EntityChangeList());
  EXPECT_FALSE(error);
}

TEST_F(SendTabToSelfBridgeTest, AddEntryAndRestartBridge) {
  InitializeBridge();

  const sync_pb::SendTabToSelfSpecifics specifics = CreateSpecifics(1);
  sync_pb::ModelTypeState state = StateWithEncryption("ekn");
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  metadata_changes->UpdateModelTypeState(state);

  auto error = bridge()->ApplySyncChanges(std::move(metadata_changes),
                                          EntityAddList({specifics}));
  ASSERT_FALSE(error);

  ShutdownBridge();

  EXPECT_CALL(*processor(),
              ModelReadyToSync(MetadataBatchContains(
                  syncer::HasEncryptionKeyName(state.encryption_key_name()),
                  /*entities=*/IsEmpty())));

  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(_)).Times(0);
  InitializeBridge();

  std::vector<std::string> guids = bridge()->GetAllGuids();
  ASSERT_EQ(1ul, guids.size());
  EXPECT_EQ(specifics.url(),
            bridge()->GetEntryByGUID(guids[0])->GetURL().spec());
}

TEST_F(SendTabToSelfBridgeTest, ApplySyncChangesInMemory) {
  InitializeBridge();

  const sync_pb::SendTabToSelfSpecifics specifics = CreateSpecifics(1);
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();

  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(SizeIs(1)));

  auto error_on_add = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));

  EXPECT_FALSE(error_on_add);

  EXPECT_EQ(1ul, bridge()->GetAllGuids().size());

  EXPECT_CALL(*mock_observer(), EntriesRemovedRemotely(SizeIs(1)));

  auto error_on_delete = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      {syncer::EntityChange::CreateDelete(specifics.guid())});

  EXPECT_FALSE(error_on_delete);
  EXPECT_EQ(0ul, bridge()->GetAllGuids().size());
}

TEST_F(SendTabToSelfBridgeTest, ApplyDeleteNonexistent) {
  InitializeBridge();
  EXPECT_CALL(*mock_observer(), EntriesAddedRemotely(_)).Times(0);

  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();

  EXPECT_CALL(*processor(), Delete(_, _)).Times(0);
  auto error =
      bridge()->ApplySyncChanges(std::move(metadata_changes),
                                 {syncer::EntityChange::CreateDelete("guid")});
  EXPECT_FALSE(error);
}

TEST_F(SendTabToSelfBridgeTest, PreserveDissmissalAfterRestartBridge) {
  InitializeBridge();

  const sync_pb::SendTabToSelfSpecifics specifics = CreateSpecifics(1);
  std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();

  auto error = bridge()->ApplySyncChanges(std::move(metadata_changes),
                                          EntityAddList({specifics}));
  ASSERT_FALSE(error);

  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(*processor(), Delete(_, _)).Times(0);

  bridge()->DismissEntry(specifics.guid());

  ShutdownBridge();

  InitializeBridge();

  std::vector<std::string> guids = bridge()->GetAllGuids();
  ASSERT_EQ(1ul, guids.size());
  EXPECT_TRUE(bridge()->GetEntryByGUID(guids[0])->GetNotificationDismissed());
}

}  // namespace

}  // namespace send_tab_to_self
