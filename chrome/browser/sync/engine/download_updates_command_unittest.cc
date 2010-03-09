// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/download_updates_command.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/test/sync/engine/proto_extension_validator.h"
#include "chrome/test/sync/engine/syncer_command_test.h"

namespace browser_sync {

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
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_UI;
    routing_info[syncable::AUTOFILL] = GROUP_DB;
    routing_info[syncable::PREFERENCES] = GROUP_PASSIVE;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(routing_info, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::autofill);
    v.ExpectHasExtension(sync_pb::preference);
    v.ExpectHasExtension(sync_pb::bookmark);
    v.ExpectNoOtherFieldsOrExtensions();
  }

  {
    SCOPED_TRACE("Several enabled datatypes, one work group.");
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_UI;
    routing_info[syncable::AUTOFILL] = GROUP_UI;
    routing_info[syncable::PREFERENCES] = GROUP_UI;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(routing_info, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::autofill);
    v.ExpectHasExtension(sync_pb::preference);
    v.ExpectHasExtension(sync_pb::bookmark);
    v.ExpectNoOtherFieldsOrExtensions();
  }

  {
    SCOPED_TRACE("Bookmarks only.");
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(routing_info, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::bookmark);
    v.ExpectNoOtherFieldsOrExtensions();
  }

  {
    SCOPED_TRACE("Autofill only.");
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::AUTOFILL] = GROUP_PASSIVE;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(routing_info, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::autofill);
    v.ExpectNoOtherFieldsOrExtensions();
  }

  {
    SCOPED_TRACE("Preferences only.");
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::PREFERENCES] = GROUP_PASSIVE;
    sync_pb::EntitySpecifics get_updates_filter;
    command_.SetRequestedTypes(routing_info, &get_updates_filter);
    ProtoExtensionValidator<sync_pb::EntitySpecifics> v(get_updates_filter);
    v.ExpectHasExtension(sync_pb::preference);
    v.ExpectNoOtherFieldsOrExtensions();
  }
}

}  // namespace browser_sync
