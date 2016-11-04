// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/accumulating_metadata_change_list.h"

#include <set>

#include "base/bind.h"
#include "base/run_loop.h"
#include "components/sync/model/mock_model_type_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using WriteBatch = ModelTypeStore::WriteBatch;

void RecordMetadataWrite(std::map<std::string, std::string>* map,
                         WriteBatch* batch,
                         const std::string& key,
                         const std::string& value) {
  (*map)[key] = value;
}

void RecordGlobalMetadataWrite(std::string* str,
                               WriteBatch* batch,
                               const std::string& value) {
  *str = value;
}

void RecordMetadataDelete(std::set<std::string>* set,
                          WriteBatch* batch,
                          const std::string& id) {
  set->insert(id);
}

void RecordGlobalMetadataDelete(bool* was_delete_called, WriteBatch* batch) {
  *was_delete_called = true;
}

class AccumulatingMetadataChangeListTest : public testing::Test {
 protected:
  AccumulatingMetadataChangeListTest() {}

  MockModelTypeStore* store() { return &store_; }

 private:
  // MockModelTypeStore needs MessageLoop
  base::MessageLoop message_loop_;

  MockModelTypeStore store_;
};

TEST_F(AccumulatingMetadataChangeListTest, TransferChangesEmptyChangeList) {
  AccumulatingMetadataChangeList cl;
  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();

  std::map<std::string, std::string> change_map;
  std::set<std::string> delete_set;
  std::string global_metadata;
  store()->RegisterWriteMetadataHandler(
      base::Bind(&RecordMetadataWrite, &change_map));
  store()->RegisterWriteGlobalMetadataHandler(
      base::Bind(&RecordGlobalMetadataWrite, &global_metadata));
  store()->RegisterDeleteMetadataHandler(
      base::Bind(&RecordMetadataDelete, &delete_set));

  cl.TransferChanges(store(), batch.get());

  EXPECT_EQ(change_map.size(), 0u);
  EXPECT_EQ(delete_set.size(), 0u);
  EXPECT_EQ(global_metadata.size(), 0u);
}

TEST_F(AccumulatingMetadataChangeListTest, TransferChangesClearsLocalState) {
  AccumulatingMetadataChangeList cl;
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash("some_hash");
  cl.UpdateMetadata("client_tag", metadata);

  sync_pb::ModelTypeState state;
  state.set_encryption_key_name("ekn");
  cl.UpdateModelTypeState(state);

  std::map<std::string, std::string> change_map;
  std::string global_metadata;
  store()->RegisterWriteMetadataHandler(
      base::Bind(&RecordMetadataWrite, &change_map));
  store()->RegisterWriteGlobalMetadataHandler(
      base::Bind(&RecordGlobalMetadataWrite, &global_metadata));

  EXPECT_EQ(0U, change_map.size());
  EXPECT_EQ("", global_metadata);

  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  cl.TransferChanges(store(), batch.get());

  EXPECT_EQ(1U, change_map.size());
  EXPECT_NE("", global_metadata);

  change_map.clear();
  global_metadata = "";
  cl.TransferChanges(store(), batch.get());

  EXPECT_EQ(0U, change_map.size());
  EXPECT_EQ("", global_metadata);
}

TEST_F(AccumulatingMetadataChangeListTest,
       TransferChangesMultipleInvocationsSafe) {
  AccumulatingMetadataChangeList cl;
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash("some_hash");
  cl.UpdateMetadata("client_tag", metadata);

  sync_pb::ModelTypeState state;
  state.set_encryption_key_name("ekn");
  cl.UpdateModelTypeState(state);

  std::string global_metadata;
  std::map<std::string, std::string> change_map;
  store()->RegisterWriteMetadataHandler(
      base::Bind(&RecordMetadataWrite, &change_map));
  store()->RegisterWriteGlobalMetadataHandler(
      base::Bind(&RecordGlobalMetadataWrite, &global_metadata));

  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  cl.TransferChanges(store(), batch.get());
  cl.TransferChanges(store(), batch.get());
  cl.TransferChanges(store(), batch.get());

  EXPECT_EQ(state.SerializeAsString(), global_metadata);
  EXPECT_EQ(metadata.SerializeAsString(), change_map["client_tag"]);
}

TEST_F(AccumulatingMetadataChangeListTest, TransferChangesMultipleChanges) {
  AccumulatingMetadataChangeList cl;

  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash("some_hash");
  cl.UpdateMetadata("client_tag", metadata);
  metadata.set_specifics_hash("specifics_hash");
  cl.UpdateMetadata("client_tag", metadata);

  sync_pb::EntityMetadata metadata2;
  metadata2.set_client_tag_hash("some_other_hash");
  cl.UpdateMetadata("client_tag2", metadata2);

  sync_pb::ModelTypeState state;
  state.set_encryption_key_name("ekn");
  cl.UpdateModelTypeState(state);
  state.set_encryption_key_name("ekn2");
  cl.UpdateModelTypeState(state);

  std::string global_metadata;
  std::map<std::string, std::string> change_map;
  store()->RegisterWriteGlobalMetadataHandler(
      base::Bind(&RecordGlobalMetadataWrite, &global_metadata));
  store()->RegisterWriteMetadataHandler(
      base::Bind(&RecordMetadataWrite, &change_map));

  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  cl.TransferChanges(store(), batch.get());

  EXPECT_EQ(metadata.SerializeAsString(), change_map["client_tag"]);
  EXPECT_EQ(metadata2.SerializeAsString(), change_map["client_tag2"]);
  EXPECT_EQ(state.SerializeAsString(), global_metadata);
}

TEST_F(AccumulatingMetadataChangeListTest, TransferChangesDeletesClearedItems) {
  AccumulatingMetadataChangeList cl;
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash("some_hash");
  cl.UpdateMetadata("client_tag", metadata);
  cl.ClearMetadata("client_tag");

  sync_pb::ModelTypeState state;
  state.set_encryption_key_name("ekn");
  cl.UpdateModelTypeState(state);
  cl.ClearModelTypeState();

  std::map<std::string, std::string> change_map;
  std::set<std::string> delete_set;
  std::string global_metadata;
  bool delete_called = false;
  store()->RegisterWriteMetadataHandler(
      base::Bind(&RecordMetadataWrite, &change_map));
  store()->RegisterWriteGlobalMetadataHandler(
      base::Bind(&RecordGlobalMetadataWrite, &global_metadata));
  store()->RegisterDeleteMetadataHandler(
      base::Bind(&RecordMetadataDelete, &delete_set));
  store()->RegisterDeleteGlobalMetadataHandler(
      base::Bind(&RecordGlobalMetadataDelete, &delete_called));

  std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
  cl.TransferChanges(store(), batch.get());

  EXPECT_TRUE(delete_set.find("client_tag") != delete_set.end());
  EXPECT_TRUE(change_map.find("client_tag") == change_map.end());
  EXPECT_TRUE(delete_called);
  EXPECT_TRUE(global_metadata.empty());
}

}  // namespace
}  // namespace syncer
