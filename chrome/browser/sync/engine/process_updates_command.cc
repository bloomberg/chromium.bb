// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/process_updates_command.h"

#include <vector>

#include "base/basictypes.h"
#include "base/location.h"
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
using sessions::UpdateProgress;

ProcessUpdatesCommand::ProcessUpdatesCommand() {}
ProcessUpdatesCommand::~ProcessUpdatesCommand() {}

bool ProcessUpdatesCommand::HasCustomGroupsToChange() const {
  // TODO(akalin): Set to true.
  return false;
}

std::set<ModelSafeGroup> ProcessUpdatesCommand::GetGroupsToChange(
    const sessions::SyncSession& session) const {
  return session.GetEnabledGroupsWithVerifiedUpdates();
}

bool ProcessUpdatesCommand::ModelNeutralExecuteImpl(SyncSession* session) {
  const GetUpdatesResponse& updates =
      session->status_controller().updates_response().get_updates();
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

  const sessions::UpdateProgress* progress =
      session->status_controller().update_progress();
  if (!progress)
    return;  // Nothing to do.

  syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir);
  vector<sessions::VerifiedUpdate>::const_iterator it;
  for (it = progress->VerifiedUpdatesBegin();
       it != progress->VerifiedUpdatesEnd();
       ++it) {
    const sync_pb::SyncEntity& update = it->second;

    if (it->first != VERIFY_SUCCESS && it->first != VERIFY_UNDELETE)
      continue;
    switch (ProcessUpdate(dir, update, &trans)) {
      case SUCCESS_PROCESSED:
      case SUCCESS_STORED:
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  StatusController* status = session->mutable_status_controller();
  status->set_num_consecutive_errors(0);
  status->mutable_update_progress()->ClearVerifiedUpdates();
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

// Process a single update. Will avoid touching global state.
ServerUpdateProcessingResult ProcessUpdatesCommand::ProcessUpdate(
    const syncable::ScopedDirLookup& dir,
    const sync_pb::SyncEntity& proto_update,
    syncable::WriteTransaction* const trans) {

  const SyncEntity& update = *static_cast<const SyncEntity*>(&proto_update);
  syncable::Id server_id = update.id();
  const std::string name = SyncerProtoUtil::NameFromSyncEntity(update);

  // Look to see if there's a local item that should recieve this update,
  // maybe due to a duplicate client tag or a lost commit response.
  syncable::Id local_id = SyncerUtil::FindLocalIdToUpdate(trans, update);

  // FindLocalEntryToUpdate has veto power.
  if (local_id.IsNull()) {
    return SUCCESS_PROCESSED;  // The entry has become irrelevant.
  }

  SyncerUtil::CreateNewEntry(trans, local_id);

  // We take a two step approach. First we store the entries data in the
  // server fields of a local entry and then move the data to the local fields
  syncable::MutableEntry target_entry(trans, syncable::GET_BY_ID, local_id);

  // We need to run the Verify checks again; the world could have changed
  // since VerifyUpdatesCommand.
  if (!ReverifyEntry(trans, update, &target_entry)) {
    return SUCCESS_PROCESSED;  // The entry has become irrelevant.
  }

  // If we're repurposing an existing local entry with a new server ID,
  // change the ID now, after we're sure that the update can succeed.
  if (local_id != server_id) {
    DCHECK(!update.deleted());
    SyncerUtil::ChangeEntryIDAndUpdateChildren(trans, &target_entry,
        server_id);
    // When IDs change, versions become irrelevant.  Forcing BASE_VERSION
    // to zero would ensure that this update gets applied, but would indicate
    // creation or undeletion if it were committed that way.  Instead, prefer
    // forcing BASE_VERSION to entry.version() while also forcing
    // IS_UNAPPLIED_UPDATE to true.  If the item is UNSYNCED, it's committable
    // from the new state; it may commit before the conflict resolver gets
    // a crack at it.
    if (target_entry.Get(syncable::IS_UNSYNCED) ||
        target_entry.Get(syncable::BASE_VERSION) > 0) {
      // If either of these conditions are met, then we can expect valid client
      // fields for this entry.  When BASE_VERSION is positive, consistency is
      // enforced on the client fields at update-application time.  Otherwise,
      // we leave the BASE_VERSION field alone; it'll get updated the first time
      // we successfully apply this update.
      target_entry.Put(syncable::BASE_VERSION, update.version());
    }
    // Force application of this update, no matter what.
    target_entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
  }

  SyncerUtil::UpdateServerFieldsFromUpdate(&target_entry, update, name);

  return SUCCESS_PROCESSED;
}

}  // namespace browser_sync
