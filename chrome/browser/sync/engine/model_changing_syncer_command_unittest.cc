// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/engine/model_changing_syncer_command.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/test/engine/fake_model_worker.h"
#include "chrome/browser/sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class FakeModelChangingSyncerCommand : public ModelChangingSyncerCommand {
 public:
  FakeModelChangingSyncerCommand() {}
  virtual ~FakeModelChangingSyncerCommand() {}

  const std::set<ModelSafeGroup>& changed_groups() const {
    return changed_groups_;
  }

 protected:
  virtual bool HasCustomGroupsToChange() const OVERRIDE {
    return false;
  }

  virtual std::set<ModelSafeGroup> GetGroupsToChange(
      const sessions::SyncSession& session) const OVERRIDE {
    return session.GetEnabledGroups();
  }

  virtual void ModelChangingExecuteImpl(
      sessions::SyncSession* session) OVERRIDE {
    changed_groups_.insert(session->status_controller().group_restriction());
  }

 private:
  std::set<ModelSafeGroup> changed_groups_;

  DISALLOW_COPY_AND_ASSIGN(FakeModelChangingSyncerCommand);
};

class ModelChangingSyncerCommandTest : public SyncerCommandTest {
 protected:
  ModelChangingSyncerCommandTest() {}
  virtual ~ModelChangingSyncerCommandTest() {}

  virtual void SetUp() {
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSWORD)));
    (*mutable_routing_info())[syncable::BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[syncable::PASSWORDS] = GROUP_PASSWORD;
    SyncerCommandTest::SetUp();
  }

  FakeModelChangingSyncerCommand command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModelChangingSyncerCommandTest);
};

TEST_F(ModelChangingSyncerCommandTest, Basic) {
  ExpectGroupsToChange(command_, GROUP_UI, GROUP_PASSWORD, GROUP_PASSIVE);
  EXPECT_TRUE(command_.changed_groups().empty());
  command_.ExecuteImpl(session());
  EXPECT_EQ(command_.GetGroupsToChangeForTest(*session()),
            command_.changed_groups());
}

}  // namespace

}  // namespace browser_sync
