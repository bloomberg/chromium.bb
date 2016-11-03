// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/passthrough_metadata_change_list.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "components/sync/model/mock_model_type_store.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using EntityMetadata = sync_pb::EntityMetadata;
using ModelTypeState = sync_pb::ModelTypeState;
using WriteBatch = ModelTypeStore::WriteBatch;

const char kKey1[] = "key1";
const char kKey2[] = "key2";

void RecordWriteGlobalMetadata(std::vector<std::string>* vector,
                               WriteBatch* batch,
                               const std::string& value) {
  vector->push_back(value);
}

void RecordDeleteGlobalMetadata(int* call_count, WriteBatch* batch) {
  ++(*call_count);
}

void RecordWriteMetadata(std::multimap<std::string, std::string>* map,
                         WriteBatch* batch,
                         const std::string& key,
                         const std::string& value) {
  map->insert(std::make_pair(key, value));
}

void RecordDeleteMetadata(std::multiset<std::string>* set,
                          WriteBatch* batch,
                          const std::string& id) {
  set->insert(id);
}

}  // namespace

class PassthroughMetadataChangeListTest : public testing::Test {
 protected:
  PassthroughMetadataChangeListTest()
      : batch_(store_.CreateWriteBatch()), passthrough_(&store_, batch_.get()) {
    store_.RegisterWriteGlobalMetadataHandler(
        base::Bind(&RecordWriteGlobalMetadata, &global_metadata_vector_));
    store_.RegisterDeleteGlobalMetadataHandler(
        base::Bind(&RecordDeleteGlobalMetadata, &clear_global_metadata_count_));
    store_.RegisterWriteMetadataHandler(
        base::Bind(&RecordWriteMetadata, &update_metadata_map_));
    store_.RegisterDeleteMetadataHandler(
        base::Bind(&RecordDeleteMetadata, &clear_metadata_set_));
  }

  MetadataChangeList* mcl() { return &passthrough_; }
  const std::vector<std::string>& global_metadata_vector() {
    return global_metadata_vector_;
  }
  int clear_global_metadata_count() { return clear_global_metadata_count_; }
  const std::multimap<std::string, std::string>& update_metadata_map() {
    return update_metadata_map_;
  }
  const std::multiset<std::string>& clear_metadata_set() {
    return clear_metadata_set_;
  }

 private:
  // MockModelTypeStore needs MessageLoop.
  base::MessageLoop message_loop_;

  MockModelTypeStore store_;
  std::unique_ptr<WriteBatch> batch_;
  PassthroughMetadataChangeList passthrough_;

  std::vector<std::string> global_metadata_vector_;
  int clear_global_metadata_count_ = 0;
  std::multimap<std::string, std::string> update_metadata_map_;
  std::multiset<std::string> clear_metadata_set_;
};

TEST_F(PassthroughMetadataChangeListTest, ConstructorOnly) {
  EXPECT_EQ(0u, global_metadata_vector().size());
  EXPECT_EQ(0, clear_global_metadata_count());
  EXPECT_EQ(0u, update_metadata_map().size());
  EXPECT_EQ(0u, clear_metadata_set().size());
}

TEST_F(PassthroughMetadataChangeListTest, UpdateModelTypeState) {
  ModelTypeState state;
  mcl()->UpdateModelTypeState(state);
  EXPECT_EQ(1u, global_metadata_vector().size());
  EXPECT_EQ(0, clear_global_metadata_count());
  EXPECT_EQ(0u, update_metadata_map().size());
  EXPECT_EQ(0u, clear_metadata_set().size());
  EXPECT_EQ(state.SerializeAsString(), global_metadata_vector()[0]);

  state.set_encryption_key_name("ekn");
  EXPECT_NE(state.SerializeAsString(), global_metadata_vector()[0]);
  mcl()->UpdateModelTypeState(state);
  EXPECT_EQ(2u, global_metadata_vector().size());
  EXPECT_EQ(state.SerializeAsString(), global_metadata_vector()[1]);
}

TEST_F(PassthroughMetadataChangeListTest, ClearModelTypeState) {
  mcl()->ClearModelTypeState();
  EXPECT_EQ(0u, global_metadata_vector().size());
  EXPECT_EQ(1, clear_global_metadata_count());
  EXPECT_EQ(0u, update_metadata_map().size());
  EXPECT_EQ(0u, clear_metadata_set().size());

  mcl()->ClearModelTypeState();
  EXPECT_EQ(2, clear_global_metadata_count());
}

TEST_F(PassthroughMetadataChangeListTest, UpdateMetadata) {
  EntityMetadata metadata;
  metadata.set_client_tag_hash("some_hash");
  std::string serialized_metadata1 = metadata.SerializeAsString();
  mcl()->UpdateMetadata(kKey1, metadata);
  EXPECT_EQ(0u, global_metadata_vector().size());
  EXPECT_EQ(0, clear_global_metadata_count());
  EXPECT_EQ(1u, update_metadata_map().size());
  EXPECT_EQ(1u, update_metadata_map().count(kKey1));
  EXPECT_EQ(serialized_metadata1, update_metadata_map().find(kKey1)->second);
  EXPECT_EQ(0u, clear_metadata_set().size());

  metadata.set_client_tag_hash("other_hash");
  std::string serialized_metadata2 = metadata.SerializeAsString();
  EXPECT_NE(serialized_metadata1, serialized_metadata2);
  mcl()->UpdateMetadata(kKey1, metadata);
  mcl()->UpdateMetadata(kKey2, metadata);
  EXPECT_EQ(3u, update_metadata_map().size());
  EXPECT_EQ(2u, update_metadata_map().count(kKey1));
  EXPECT_EQ(1u, update_metadata_map().count(kKey2));
  auto key1_iter = update_metadata_map().find(kKey1);
  EXPECT_EQ(serialized_metadata1, key1_iter->second);
  EXPECT_EQ(serialized_metadata2, std::next(key1_iter)->second);
  EXPECT_EQ(serialized_metadata2, update_metadata_map().find(kKey2)->second);
}

TEST_F(PassthroughMetadataChangeListTest, ClearMetadata) {
  mcl()->ClearMetadata(kKey1);
  EXPECT_EQ(1u, clear_metadata_set().size());
  EXPECT_EQ(1u, clear_metadata_set().count(kKey1));

  mcl()->ClearMetadata(kKey1);
  mcl()->ClearMetadata(kKey2);
  EXPECT_EQ(3u, clear_metadata_set().size());
  EXPECT_EQ(2u, clear_metadata_set().count(kKey1));
  EXPECT_EQ(1u, clear_metadata_set().count(kKey2));
}

TEST_F(PassthroughMetadataChangeListTest, Everything) {
  mcl()->UpdateModelTypeState(ModelTypeState());
  mcl()->ClearModelTypeState();
  mcl()->UpdateMetadata(kKey1, EntityMetadata());
  mcl()->ClearMetadata(kKey1);
  EXPECT_EQ(1u, global_metadata_vector().size());
  EXPECT_EQ(1, clear_global_metadata_count());
  EXPECT_EQ(1u, update_metadata_map().size());
  EXPECT_EQ(1u, clear_metadata_set().size());
}

}  // namespace syncer
