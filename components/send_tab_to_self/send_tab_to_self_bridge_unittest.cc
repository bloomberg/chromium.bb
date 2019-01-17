// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

namespace {

using testing::_;

class SendTabToSelfBridgeTest : public testing::Test {
 protected:
  SendTabToSelfBridgeTest()
      : bridge_(mock_processor_.CreateForwardingProcessor(),
                &provider_,
                &clock_) {
    provider_.Initialize("cache_guid", "machine");
    ON_CALL(mock_processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));
  }

  base::Time AdvanceAndGetTime() {
    clock_.Advance(base::TimeDelta::FromMilliseconds(10));
    return clock_.Now();
  }

  syncer::EntityDataPtr MakeEntityData(const SendTabToSelfEntry& entry) {
    std::unique_ptr<sync_pb::SendTabToSelfSpecifics> specifics =
        entry.AsProto();

    auto entity_data = std::make_unique<syncer::EntityData>();

    *(entity_data->specifics.mutable_send_tab_to_self()) = *specifics;
    entity_data->non_unique_name = entry.GetURL().spec();
    // The client_tag_hash field is unused by the send_tab_to_self_bridge, but
    // is required for a valid entity_data.
    entity_data->client_tag_hash = "someclienttaghash";
    return entity_data->PassToPtr();
  }

  // For Model Tests.
  void AddSampleEntries() {
    // Adds timer to avoid having two entries with the same shared timestamp.
    bridge_.AddEntry(GURL("http://a.com"), "a", AdvanceAndGetTime());
    bridge_.AddEntry(GURL("http://b.com"), "b", AdvanceAndGetTime());
    bridge_.AddEntry(GURL("http://c.com"), "c", AdvanceAndGetTime());
    bridge_.AddEntry(GURL("http://d.com"), "d", AdvanceAndGetTime());
  }

  base::SimpleTestClock clock_;

  // In memory model type store needs to be able to post tasks.
  base::test::ScopedTaskEnvironment task_environment_;

  syncer::LocalDeviceInfoProviderMock provider_;

  syncer::MockModelTypeChangeProcessor mock_processor_;

  SendTabToSelfBridge bridge_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfBridgeTest);
};

TEST_F(SendTabToSelfBridgeTest, CheckEmpties) {
  EXPECT_EQ(0ul, bridge_.GetAllGuids().size());
  AddSampleEntries();
  EXPECT_EQ(4ul, bridge_.GetAllGuids().size());
}

TEST_F(SendTabToSelfBridgeTest, SyncAddOneEntry) {
  syncer::EntityChangeList remote_input;

  SendTabToSelfEntry entry("guid1", GURL("http://www.example.com/"), "title",
                           AdvanceAndGetTime(), AdvanceAndGetTime(), "device");

  remote_input.push_back(
      syncer::EntityChange::CreateAdd("guid1", MakeEntityData(entry)));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  bridge_.MergeSyncData(std::move(metadata_change_list), remote_input);
  EXPECT_EQ(1ul, bridge_.GetAllGuids().size());
}

TEST_F(SendTabToSelfBridgeTest, ApplySyncChangesOneAdd) {
  SendTabToSelfEntry entry("guid1", GURL("http://www.example.com/"), "title",
                           AdvanceAndGetTime(), AdvanceAndGetTime(), "device");
  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> specifics = entry.AsProto();

  syncer::EntityChangeList add_changes;

  add_changes.push_back(
      syncer::EntityChange::CreateAdd("guid1", MakeEntityData(entry)));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  bridge_.ApplySyncChanges(std::move(metadata_change_list), add_changes);
  EXPECT_EQ(1ul, bridge_.GetAllGuids().size());
}

// Tests that the send tab to self entry is correctly removed.
TEST_F(SendTabToSelfBridgeTest, ApplySyncChangesOneDeletion) {
  SendTabToSelfEntry entry("guid1", GURL("http://www.example.com/"), "title",
                           AdvanceAndGetTime(), AdvanceAndGetTime(), "device");
  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> specifics = entry.AsProto();

  syncer::EntityChangeList add_changes;

  add_changes.push_back(
      syncer::EntityChange::CreateAdd("guid1", MakeEntityData(entry)));
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  bridge_.ApplySyncChanges(std::move(metadata_change_list), add_changes);
  EXPECT_EQ(1ul, bridge_.GetAllGuids().size());
  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(syncer::EntityChange::CreateDelete("guid1"));
  bridge_.ApplySyncChanges(std::move(metadata_change_list), delete_changes);
  EXPECT_EQ(0ul, bridge_.GetAllGuids().size());
}

}  // namespace

}  // namespace send_tab_to_self
