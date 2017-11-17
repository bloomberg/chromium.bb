// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/worker_entity_tracker.h"

#include "components/sync/base/hash_util.h"
#include "components/sync/base/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

// Some simple tests for the WorkerEntityTracker.
//
// The WorkerEntityTracker is an implementation detail of the ModelTypeWorker.
// As such, it doesn't make much sense to test it exhaustively, since it
// already gets a lot of test coverage from the ModelTypeWorker unit tests.
//
// These tests are intended as a basic sanity check. Anything more complicated
// would be redundant.
class WorkerEntityTrackerTest : public ::testing::Test {
 public:
  WorkerEntityTrackerTest()
      : kServerId("ServerID"),
        kClientTag("some.sample.tag"),
        kClientTagHash(GenerateSyncableHash(PREFERENCES, kClientTag)),
        entity_(new WorkerEntityTracker(kClientTagHash)) {
    specifics.mutable_preference()->set_name(kClientTag);
    specifics.mutable_preference()->set_value("pref.value");
  }

  ~WorkerEntityTrackerTest() override {}

  UpdateResponseData MakeUpdateResponseData(int64_t response_version) {
    EntityData data;
    data.id = kServerId;
    data.client_tag_hash = kClientTagHash;

    UpdateResponseData response_data;
    response_data.entity = data.PassToPtr();
    response_data.response_version = response_version;
    return response_data;
  }

  const std::string kServerId;
  const std::string kClientTag;
  const std::string kClientTagHash;
  sync_pb::EntitySpecifics specifics;
  std::unique_ptr<WorkerEntityTracker> entity_;
};

// Construct a new entity from a server update. Then receive another update.
TEST_F(WorkerEntityTrackerTest, FromUpdateResponse) {
  EXPECT_EQ("", entity_->id());
  entity_->ReceiveUpdate(MakeUpdateResponseData(20));
  EXPECT_EQ(kServerId, entity_->id());
}

}  // namespace syncer
