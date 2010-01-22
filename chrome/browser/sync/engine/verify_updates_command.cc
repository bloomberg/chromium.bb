// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/sync/engine/verify_updates_command.h"

#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

using syncable::ScopedDirLookup;
using syncable::SyncName;
using syncable::WriteTransaction;

using syncable::GET_BY_ID;
using syncable::SYNCER;

VerifyUpdatesCommand::VerifyUpdatesCommand() {}
VerifyUpdatesCommand::~VerifyUpdatesCommand() {}

void VerifyUpdatesCommand::ExecuteImpl(sessions::SyncSession* session) {
  LOG(INFO) << "Beginning Update Verification";
  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }
  WriteTransaction trans(dir, SYNCER, __FILE__, __LINE__);
  sessions::StatusController* status = session->status_controller();
  const GetUpdatesResponse& updates = status->updates_response().get_updates();
  int update_count = updates.entries().size();

  LOG(INFO) << update_count << " entries to verify";
  for (int i = 0; i < update_count; i++) {
    const SyncEntity entry =
        *reinterpret_cast<const SyncEntity *>(&(updates.entries(i)));
    // Needs to be done separately in order to make sure the update processing
    // still happens like normal. We should really just use one type of
    // ID in fact, there isn't actually a need for server_knows and not IDs.
    SyncerUtil::AttemptReuniteLostCommitResponses(&trans, entry,
        trans.directory()->cache_guid());
    VerifyResult result = VerifyUpdate(&trans, entry);
    status->mutable_update_progress()->AddVerifyResult(result, entry);
  }
}

VerifyResult VerifyUpdatesCommand::VerifyUpdate(
    syncable::WriteTransaction* trans, const SyncEntity& entry) {
  syncable::Id id = entry.id();

  const bool deleted = entry.has_deleted() && entry.deleted();
  const bool is_directory = entry.IsFolder();
  const bool is_bookmark =
      SyncerUtil::GetModelType(entry) == syncable::BOOKMARKS;

  if (!id.ServerKnows()) {
    LOG(ERROR) << "Illegal negative id in received updates";
    return VERIFY_FAIL;
  }
  if (!entry.parent_id().ServerKnows()) {
    LOG(ERROR) << "Illegal parent id in received updates";
    return VERIFY_FAIL;
  }
  {
    const std::string name = SyncerProtoUtil::NameFromSyncEntity(entry);
    if (name.empty() && !deleted) {
      LOG(ERROR) << "Zero length name in non-deleted update";
      return VERIFY_FAIL;
    }
  }

  syncable::MutableEntry same_id(trans, GET_BY_ID, id);
  VerifyResult result = VERIFY_UNDECIDED;
  result = SyncerUtil::VerifyNewEntry(entry, &same_id, deleted);

  if (VERIFY_UNDECIDED == result) {
    if (deleted)
      result = VERIFY_SUCCESS;
  }

  // If we have an existing entry, we check here for updates that break
  // consistency rules.
  if (VERIFY_UNDECIDED == result) {
    result = SyncerUtil::VerifyUpdateConsistency(trans, entry, &same_id,
      deleted, is_directory, is_bookmark);
  }

  if (VERIFY_UNDECIDED == result)
    return VERIFY_SUCCESS;  // No news is good news.
  else
    return result;  // This might be VERIFY_SUCCESS as well
}

}  // namespace browser_sync
