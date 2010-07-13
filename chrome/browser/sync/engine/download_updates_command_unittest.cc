// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/download_updates_command.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/test/sync/engine/proto_extension_validator.h"
#include "chrome/test/sync/engine/syncer_command_test.h"

namespace browser_sync {

using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::MODEL_TYPE_COUNT;

// A test fixture for tests exercising DownloadUpdatesCommandTest.
class DownloadUpdatesCommandTest : public SyncerCommandTest {
 protected:
  DownloadUpdatesCommandTest() {}
  DownloadUpdatesCommand command_;
 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadUpdatesCommandTest);
};

TEST_F(DownloadUpdatesCommandTest, SetRequestedTypes) {
  {
    SCOPED_TRACE("Several enabled datatypes, spread out across groups.");
    syncable::ModelTypeBitSet enabled_types;
    enabled_types[syncable::BOOKMARKS] = true;
    enabled_types[syncable::AUTOFILL] = true;
    enabled_types[syncable::PREFERENCES] = true;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(enabled_types, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::autofill);
    v.ExpectHasExtension(sync_pb::preference);
    v.ExpectHasExtension(sync_pb::bookmark);
    v.ExpectNoOtherFieldsOrExtensions();
  }

  {
    SCOPED_TRACE("Top level folders.");
    syncable::ModelTypeBitSet enabled_types;
    enabled_types[syncable::TOP_LEVEL_FOLDER] = true;
    enabled_types[syncable::BOOKMARKS] = true;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(enabled_types, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::bookmark);
    v.ExpectNoOtherFieldsOrExtensions();
  }

  {
    SCOPED_TRACE("Bookmarks only.");
    syncable::ModelTypeBitSet enabled_types;
    enabled_types[syncable::BOOKMARKS] = true;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(enabled_types, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::bookmark);
    v.ExpectNoOtherFieldsOrExtensions();
  }

  {
    SCOPED_TRACE("Autofill only.");
    syncable::ModelTypeBitSet enabled_types;
    enabled_types[syncable::AUTOFILL] = true;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(enabled_types, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::autofill);
    v.ExpectNoOtherFieldsOrExtensions();
  }

  {
    SCOPED_TRACE("Preferences only.");
    syncable::ModelTypeBitSet enabled_types;
    enabled_types[syncable::PREFERENCES] = true;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(enabled_types, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::preference);
    v.ExpectNoOtherFieldsOrExtensions();
  }
}

TEST_F(DownloadUpdatesCommandTest, OldestTimestampPicked) {
  syncable::ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
  ASSERT_TRUE(dir.good());

  // i,j,k range over every possible model type.  j and k are enabled and at
  // the same timestamp.  if i != j or k, i is enabled but at an older
  // timestamp.
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType type_i = syncable::ModelTypeFromInt(i);
    for (int j = FIRST_REAL_MODEL_TYPE; j < MODEL_TYPE_COUNT; ++j) {
      syncable::ModelType type_j = syncable::ModelTypeFromInt(j);
      for (int k = FIRST_REAL_MODEL_TYPE; k < MODEL_TYPE_COUNT; ++k) {
        syncable::ModelType type_k = syncable::ModelTypeFromInt(k);
        SCOPED_TRACE(testing::Message() << "Iteration (" << i << "," << j
                                        << "," << k << ")");
        mutable_routing_info()->clear();
        (*mutable_routing_info())[type_i] = browser_sync::GROUP_UI;
        (*mutable_routing_info())[type_j] = browser_sync::GROUP_DB;
        (*mutable_routing_info())[type_k] = browser_sync::GROUP_UI;
        dir->set_last_download_timestamp(type_i, 5000 + j);
        dir->set_last_download_timestamp(type_j, 1000 + i);
        dir->set_last_download_timestamp(type_k, 1000 + i);

        ConfigureMockServerConnection();
        dir->set_store_birthday(mock_server()->store_birthday());

        syncable::ModelTypeBitSet expected_request_types;
        expected_request_types[j] = true;
        expected_request_types[k] = true;
        mock_server()->ExpectGetUpdatesRequestTypes(expected_request_types);

        command_.Execute(session());

        const sync_pb::ClientToServerMessage& r = mock_server()->last_request();
        EXPECT_EQ(1, mock_server()->GetAndClearNumGetUpdatesRequests());
        EXPECT_TRUE(r.has_get_updates());
        EXPECT_TRUE(r.get_updates().has_from_timestamp());
        EXPECT_EQ(1000 + i, r.get_updates().from_timestamp());
        EXPECT_TRUE(r.get_updates().has_fetch_folders());
        EXPECT_TRUE(r.get_updates().fetch_folders());
        EXPECT_TRUE(r.get_updates().has_requested_types());
      }
    }
  }
}

}  // namespace browser_sync
