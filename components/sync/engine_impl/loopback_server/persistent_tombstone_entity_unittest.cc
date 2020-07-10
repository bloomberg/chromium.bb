// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/loopback_server/persistent_tombstone_entity.h"

#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(PersistentTombstoneEntityTest, CreateFromEntity) {
  sync_pb::SyncEntity entity;
  *entity.mutable_id_string() = "invalid_id";
  ASSERT_FALSE(PersistentTombstoneEntity::CreateFromEntity(entity));
  *entity.mutable_id_string() = "37702_id";
  ASSERT_TRUE(PersistentTombstoneEntity::CreateFromEntity(entity));
}

TEST(PersistentTombstoneEntityTest, CreateNew) {
  ASSERT_FALSE(PersistentTombstoneEntity::CreateNew(
      "invalid_id", "client_defined_unique_tag"));
  ASSERT_TRUE(PersistentTombstoneEntity::CreateNew(
      "37702_id", "client_defined_unique_tag"));
}

}  // namespace

}  // namespace syncer
