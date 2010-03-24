// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/process_updates_command.h"

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

using std::vector;

namespace browser_sync {

using sessions::SyncSession;
using sessions::StatusController;

ProcessUpdatesCommand::ProcessUpdatesCommand() {}
ProcessUpdatesCommand::~ProcessUpdatesCommand() {}

bool ProcessUpdatesCommand::ModelNeutralExecuteImpl(SyncSession* session) {
  const GetUpdatesResponse& updates =
      session->status_controller()->updates_response().get_updates();
  const int update_count = updates.entries_size();

  // Don't bother processing updates if there were none.
  return update_count != 0;
}

void ProcessUpdatesCommand::ModelChangingExecuteImpl(SyncSession* session) {
  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  StatusController* status = session->status_controller();

  const sessions::UpdateProgress& progress(status->update_progress());
  vector<sessions::VerifiedUpdate>::const_iterator it;
  for (it = progress.VerifiedUpdatesBegin();
       it != progress.VerifiedUpdatesEnd();
       ++it) {
    const sync_pb::SyncEntity& update = it->second;

    if (it->first != VERIFY_SUCCESS && it->first != VERIFY_UNDELETE)
      continue;
    switch (ProcessUpdate(dir, update)) {
      case SUCCESS_PROCESSED:
      case SUCCESS_STORED:
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  status->set_num_consecutive_errors(0);

  // TODO(nick): The following line makes no sense to me.
  status->set_syncing(true);
  return;
}

namespace {
// Returns true if the entry is still ok to process.
bool ReverifyEntry(syncable::WriteTransaction* trans, const SyncEntity& entry,
                   syncable::MutableEntry* same_id) {

  const bool deleted = entry.has_deleted() && entry.deleted();
  const bool is_directory = entry.IsFolder();
  const syncable::ModelType model_type = entry.GetModelType();

  return VERIFY_SUCCESS == SyncerUtil::VerifyUpdateConsistency(trans,
                                                               entry,
                                                               same_id,
                                                               deleted,
                                                               is_directory,
                                                               model_type);
}
}  // namespace

// TODO(sync): Refactor this code.
// Process a single update. Will avoid touching global state.
ServerUpdateProcessingResult ProcessUpdatesCommand::ProcessUpdate(
    const syncable::ScopedDirLookup& dir, const sync_pb::SyncEntity& pb_entry) {

  const SyncEntity& entry = *static_cast<const SyncEntity*>(&pb_entry);
  using namespace syncable;
  syncable::Id id = entry.id();
  const std::string name = SyncerProtoUtil::NameFromSyncEntity(entry);

  WriteTransaction trans(dir, SYNCER, __FILE__, __LINE__);

  SyncerUtil::CreateNewEntry(&trans, id);

  // We take a two step approach. First we store the entries data in the
  // server fields of a local entry and then move the data to the local fields
  MutableEntry update_entry(&trans, GET_BY_ID, id);
  // TODO(sync): do we need to run ALL these checks, or is a mere version check
  // good enough?
  if (!ReverifyEntry(&trans, entry, &update_entry)) {
    return SUCCESS_PROCESSED;  // the entry has become irrelevant
  }

  SyncerUtil::UpdateServerFieldsFromUpdate(&update_entry, entry, name);

  if (update_entry.Get(SERVER_VERSION) == update_entry.Get(BASE_VERSION) &&
      !update_entry.Get(IS_UNSYNCED)) {
      // It's largely OK if data doesn't match exactly since a future update
      // will just clobber the data. Conflict resolution will overwrite and
      // take one side as the winner and does not try to merge, so strict
      // equality isn't necessary.
      LOG_IF(ERROR, !SyncerUtil::ServerAndLocalEntriesMatch(&update_entry))
          << update_entry;
  }
  return SUCCESS_PROCESSED;
}

}  // namespace browser_sync
