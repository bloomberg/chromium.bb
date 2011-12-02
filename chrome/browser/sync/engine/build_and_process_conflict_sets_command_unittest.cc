// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/engine/build_and_process_conflict_sets_command.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/test/engine/fake_model_worker.h"
#include "chrome/browser/sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class BuildAndProcessConflictSetsCommandTest : public SyncerCommandTest {
 protected:
  BuildAndProcessConflictSetsCommandTest() {}
  virtual ~BuildAndProcessConflictSetsCommandTest() {}

  virtual void SetUp() {
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSWORD)));
    (*mutable_routing_info())[syncable::BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[syncable::PASSWORDS] = GROUP_PASSWORD;
    SyncerCommandTest::SetUp();
  }

  BuildAndProcessConflictSetsCommand command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BuildAndProcessConflictSetsCommandTest);
};

TEST_F(BuildAndProcessConflictSetsCommandTest, GetGroupsToChange) {
  ExpectNoGroupsToChange(command_);
  // Put GROUP_UI in conflict.
  session()->mutable_status_controller()->
      GetUnrestrictedMutableConflictProgressForTest(GROUP_UI)->
      AddConflictingItemById(syncable::Id());
  ExpectGroupToChange(command_, GROUP_UI);
}

}  // namespace

}  // namespace browser_sync
