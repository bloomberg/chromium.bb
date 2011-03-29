// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/sync/engine/cleanup_disabled_types_command.h"

#include "chrome/browser/sync/engine/syncer_end_command.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/test/sync/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace browser_sync {

class CleanupDisabledTypesCommandTest : public MockDirectorySyncerCommandTest {
 public:
  CleanupDisabledTypesCommandTest() {
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; i++) {
      all_types_.insert(syncable::ModelTypeFromInt(i));
    }
  }
  virtual void SetUp() {
    mutable_routing_info()->clear();
    (*mutable_routing_info())[syncable::BOOKMARKS] = GROUP_PASSIVE;
    MockDirectorySyncerCommandTest::SetUp();
  }

  // Overridden to allow SyncerEndCommand Execute to work.
  virtual bool IsSyncingCurrentlySilenced() {
    return false;
  }

  const syncable::ModelTypeSet& all_types() { return all_types_; }

 private:
  syncable::ModelTypeSet all_types_;
};

// TODO(tim): Add syncer test to verify previous routing info is set.
TEST_F(CleanupDisabledTypesCommandTest, NoPreviousRoutingInfo) {
  CleanupDisabledTypesCommand command;
  syncable::ModelTypeSet expected(all_types());
  expected.erase(syncable::BOOKMARKS);
  EXPECT_CALL(*mock_directory(), PurgeEntriesWithTypeIn(expected));
  command.ExecuteImpl(session());
}

TEST_F(CleanupDisabledTypesCommandTest, NoPurge) {
  CleanupDisabledTypesCommand command;
  EXPECT_CALL(*mock_directory(), PurgeEntriesWithTypeIn(_)).Times(0);

  ModelSafeRoutingInfo prev(routing_info());
  session()->context()->set_previous_session_routing_info(prev);
  (*mutable_routing_info())[syncable::AUTOFILL] = GROUP_PASSIVE;
  command.ExecuteImpl(session());

  prev = routing_info();
  command.ExecuteImpl(session());
}

TEST_F(CleanupDisabledTypesCommandTest, TypeDisabled) {
  CleanupDisabledTypesCommand command;
  syncable::ModelTypeSet expected;
  expected.insert(syncable::PASSWORDS);
  expected.insert(syncable::PREFERENCES);

  (*mutable_routing_info())[syncable::AUTOFILL] = GROUP_PASSIVE;
  (*mutable_routing_info())[syncable::THEMES] = GROUP_PASSIVE;
  (*mutable_routing_info())[syncable::EXTENSIONS] = GROUP_PASSIVE;

  ModelSafeRoutingInfo prev(routing_info());
  prev[syncable::PASSWORDS] = GROUP_PASSIVE;
  prev[syncable::PREFERENCES] = GROUP_PASSIVE;
  session()->context()->set_previous_session_routing_info(prev);

  EXPECT_CALL(*mock_directory(), PurgeEntriesWithTypeIn(expected));
  command.ExecuteImpl(session());
}

}  // namespace browser_sync

