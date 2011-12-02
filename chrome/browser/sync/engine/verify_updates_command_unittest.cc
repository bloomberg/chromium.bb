// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "chrome/browser/sync/engine/verify_updates_command.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/test/engine/fake_model_worker.h"
#include "chrome/browser/sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

using sessions::SyncSession;
using sessions::StatusController;
using std::string;
using syncable::Entry;
using syncable::Id;
using syncable::MutableEntry;
using syncable::ReadTransaction;
using syncable::ScopedDirLookup;
using syncable::UNITTEST;
using syncable::WriteTransaction;

class VerifyUpdatesCommandTest : public SyncerCommandTest {
 public:
  virtual void SetUp() {
    workers()->clear();
    mutable_routing_info()->clear();
    workers()->push_back(make_scoped_refptr(new FakeModelWorker(GROUP_DB)));
    workers()->push_back(make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    (*mutable_routing_info())[syncable::PREFERENCES] = GROUP_UI;
    (*mutable_routing_info())[syncable::BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[syncable::AUTOFILL] = GROUP_DB;
    SyncerCommandTest::SetUp();
  }

  void CreateLocalItem(const std::string& item_id,
                       const std::string& parent_id,
                       const syncable::ModelType& type) {
    ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
    ASSERT_TRUE(dir.good());
    WriteTransaction trans(FROM_HERE, UNITTEST, dir);
    MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
        Id::CreateFromServerId(item_id));
    ASSERT_TRUE(entry.good());

    entry.Put(syncable::BASE_VERSION, 1);
    entry.Put(syncable::SERVER_VERSION, 1);
    entry.Put(syncable::NON_UNIQUE_NAME, item_id);
    entry.Put(syncable::PARENT_ID, Id::CreateFromServerId(parent_id));
    sync_pb::EntitySpecifics default_specifics;
    AddDefaultExtensionValue(type, &default_specifics);
    entry.Put(syncable::SERVER_SPECIFICS, default_specifics);
  }

  void AddUpdate(GetUpdatesResponse* updates,
      const std::string& id, const std::string& parent,
      const syncable::ModelType& type) {
    sync_pb::SyncEntity* e = updates->add_entries();
    e->set_id_string("b1");
    e->set_parent_id_string(parent);
    e->set_non_unique_name("b1");
    e->set_name("b1");
    AddDefaultExtensionValue(type, e->mutable_specifics());
  }

  VerifyUpdatesCommand command_;

};

TEST_F(VerifyUpdatesCommandTest, AllVerified) {
  string root = syncable::GetNullId().GetServerId();

  CreateLocalItem("b1", root, syncable::BOOKMARKS);
  CreateLocalItem("b2", root, syncable::BOOKMARKS);
  CreateLocalItem("p1", root, syncable::PREFERENCES);
  CreateLocalItem("a1", root, syncable::AUTOFILL);

  GetUpdatesResponse* updates = session()->mutable_status_controller()->
      mutable_updates_response()->mutable_get_updates();
  AddUpdate(updates, "b1", root, syncable::BOOKMARKS);
  AddUpdate(updates, "b2", root, syncable::BOOKMARKS);
  AddUpdate(updates, "p1", root, syncable::PREFERENCES);
  AddUpdate(updates, "a1", root, syncable::AUTOFILL);

  command_.ExecuteImpl(session());

  StatusController* status = session()->mutable_status_controller();
  {
    sessions::ScopedModelSafeGroupRestriction r(status, GROUP_UI);
    ASSERT_TRUE(status->update_progress());
    EXPECT_EQ(3, status->update_progress()->VerifiedUpdatesSize());
  }
  {
    sessions::ScopedModelSafeGroupRestriction r(status, GROUP_DB);
    ASSERT_TRUE(status->update_progress());
    EXPECT_EQ(1, status->update_progress()->VerifiedUpdatesSize());
  }
  {
    sessions::ScopedModelSafeGroupRestriction r(status, GROUP_PASSIVE);
    EXPECT_FALSE(status->update_progress());
  }
}

}
