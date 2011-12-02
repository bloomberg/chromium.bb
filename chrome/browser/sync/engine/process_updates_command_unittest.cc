// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/engine/process_updates_command.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/test/engine/fake_model_worker.h"
#include "chrome/browser/sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class ProcessUpdatesCommandTest : public SyncerCommandTest {
 protected:
  ProcessUpdatesCommandTest() {}
  virtual ~ProcessUpdatesCommandTest() {}

  virtual void SetUp() {
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_DB)));
    (*mutable_routing_info())[syncable::BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[syncable::AUTOFILL] = GROUP_DB;
    SyncerCommandTest::SetUp();
  }

  ProcessUpdatesCommand command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessUpdatesCommandTest);
};

TEST_F(ProcessUpdatesCommandTest, GetGroupsToChange) {
  ExpectNoGroupsToChange(command_);
  // Add a verified update for GROUP_DB.
  session()->mutable_status_controller()->
      GetUnrestrictedMutableUpdateProgressForTest(GROUP_DB)->
      AddVerifyResult(VerifyResult(), sync_pb::SyncEntity());
  ExpectGroupToChange(command_, GROUP_DB);
}

}  // namespace

}  // namespace browser_sync
