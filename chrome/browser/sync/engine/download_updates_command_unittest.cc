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

  virtual void SetUp() {
    workers()->clear();
    mutable_routing_info()->clear();
    // GROUP_PASSIVE worker.
    workers()->push_back(make_scoped_refptr(new ModelSafeWorker()));
    SyncerCommandTest::SetUp();
  }

  virtual void SetRoutingInfo(const syncable::ModelTypeSet& types) {
    for (syncable::ModelTypeSet::const_iterator iter = types.begin();
         iter != types.end(); ++iter) {
      (*mutable_routing_info())[*iter] = GROUP_PASSIVE;
    }
  }

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

TEST_F(DownloadUpdatesCommandTest, ExecuteNoPayloads) {
  ConfigureMockServerConnection();

  syncable::ModelTypeSet types;
  types.insert(syncable::AUTOFILL);
  types.insert(syncable::BOOKMARKS);
  types.insert(syncable::PREFERENCES);
  SetRoutingInfo(types);
  mock_server()->ExpectGetUpdatesRequestTypes(ModelTypeBitSetFromSet(types));
  command_.ExecuteImpl(session());
}

TEST_F(DownloadUpdatesCommandTest, ExecuteWithPayloads) {
  ConfigureMockServerConnection();

  syncable::ModelTypeSet types;
  types.insert(syncable::AUTOFILL);
  types.insert(syncable::BOOKMARKS);
  types.insert(syncable::PREFERENCES);
  SetRoutingInfo(types);
  sessions::SyncSourceInfo source;
  source.types[syncable::AUTOFILL] = "autofill_payload";
  source.types[syncable::BOOKMARKS] = "bookmark_payload";
  source.types[syncable::PREFERENCES] = "preferences_payload";
  mock_server()->ExpectGetUpdatesRequestTypes(ModelTypeBitSetFromSet(types));
  mock_server()->ExpectGetUpdatesRequestPayloads(source.types);
  command_.ExecuteImpl(session(source));
}

}  // namespace browser_sync
