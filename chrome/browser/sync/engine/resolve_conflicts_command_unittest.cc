// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/engine/resolve_conflicts_command.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/test/engine/fake_model_worker.h"
#include "chrome/browser/sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class ResolveConflictsCommandTest : public SyncerCommandTest {
 protected:
  ResolveConflictsCommandTest() {}
  virtual ~ResolveConflictsCommandTest() {}

  virtual void SetUp() {
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSWORD)));
    (*mutable_routing_info())[syncable::BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[syncable::PASSWORDS] = GROUP_PASSWORD;
    SyncerCommandTest::SetUp();
  }

  ResolveConflictsCommand command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResolveConflictsCommandTest);
};

TEST_F(ResolveConflictsCommandTest, GetGroupsToChange) {
  ExpectNoGroupsToChange(command_);
  // Put GROUP_PASSWORD in conflict.
  session()->mutable_status_controller()->
      GetUnrestrictedMutableConflictProgressForTest(GROUP_PASSWORD)->
      AddConflictingItemById(syncable::Id());
  ExpectGroupToChange(command_, GROUP_PASSWORD);
}

}  // namespace

}  // namespace browser_sync
