// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/process_commit_response_command.h"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/location.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/time.h"

using syncable::ScopedDirLookup;
using syncable::WriteTransaction;
using syncable::MutableEntry;
using syncable::Entry;

using std::set;
using std::string;
using std::vector;

using syncable::BASE_VERSION;
using syncable::GET_BY_ID;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::PARENT_ID;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_POSITION_IN_PARENT;
using syncable::SERVER_VERSION;
using syncable::SYNCER;
using syncable::SYNCING;

namespace browser_sync {

using sessions::OrderedCommitSet;
using sessions::StatusController;
using sessions::SyncSession;
using sessions::ConflictProgress;

void IncrementErrorCounters(StatusController* status) {
  status->increment_num_consecutive_errors();
}
void ResetErrorCounters(StatusController* status) {
  status->set_num_consecutive_errors(0);
}

ProcessCommitResponseCommand::ProcessCommitResponseCommand() {}
ProcessCommitResponseCommand::~ProcessCommitResponseCommand() {}

std::set<ModelSafeGroup> ProcessCommitResponseCommand::GetGroupsToChange(
    const sessions::SyncSession& session) const {
  std::set<ModelSafeGroup> groups_with_commits;
  syncable::ScopedDirLookup dir(session.context()->directory_manager(),
                                session.context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return groups_with_commits;
  }

  syncable::ReadTransaction trans(FROM_HERE, dir);
  const StatusController& status = session.status_controller();
  for (size_t i = 0; i < status.commit_ids().size(); ++i) {
    groups_with_commits.insert(
        GetGroupForModelType(status.GetUnrestrictedCommitModelTypeAt(i),
                             session.routing_info()));
  }

  return groups_with_commits;
}

bool ProcessCommitResponseCommand::ModelNeutralExecuteImpl(
    sessions::SyncSession* session) {
  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return false;
  }

  const StatusController& status = session->status_controller();
  const ClientToServerResponse& response(status.commit_response());
  const vector<syncable::Id>& commit_ids(status.commit_ids());

  if (!response.has_commit()) {
    // TODO(sync): What if we didn't try to commit anything?
    LOG(WARNING) << "Commit response has no commit body!";
    IncrementErrorCounters(session->mutable_status_controller());
    return false;
  }

  const CommitResponse& cr = response.commit();
  int commit_count = commit_ids.size();
  if (cr.entryresponse_size() != commit_count) {
    LOG(ERROR) << "Commit response has wrong number of entries! Expected:" <<
               commit_count << " Got:" << cr.entryresponse_size();
    for (int i = 0 ; i < cr.entryresponse_size() ; i++) {
      LOG(ERROR) << "Response #" << i << " Value: " <<
                 cr.entryresponse(i).response_type();
      if (cr.entryresponse(i).has_error_message())
        LOG(ERROR) << "  " << cr.entryresponse(i).error_message();
    }
    IncrementErrorCounters(session->mutable_status_controller());
    return false;
  }
  return true;
}

void ProcessCommitResponseCommand::ModelChangingExecuteImpl(
    SyncSession* session) {
  ProcessCommitResponse(session);
  ExtensionsActivityMonitor* monitor = session->context()->extensions_monitor();
  if (session->status_controller().HasBookmarkCommitActivity() &&
      session->status_controller().syncer_status()
          .num_successful_bookmark_commits == 0) {
    monitor->PutRecords(session->extensions_activity());
    session->mutable_extensions_activity()->clear();
  }
}

void ProcessCommitResponseCommand::ProcessCommitResponse(
    SyncSession* session) {
  // TODO(sync): This function returns if it sees problems. We probably want
  // to flag the need for an update or similar.
  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }

  StatusController* status = session->mutable_status_controller();
  const ClientToServerResponse& response(status->commit_response());
  const CommitResponse& cr = response.commit();
  const sync_pb::CommitMessage& commit_message =
      status->commit_message().commit();

  // If we try to commit a parent and child together and the parent conflicts
  // the child will have a bad parent causing an error. As this is not a
  // critical error, we trap it and don't LOG(ERROR). To enable this we keep
  // a map of conflicting new folders.
  int transient_error_commits = 0;
  int conflicting_commits = 0;
  int error_commits = 0;
  int successes = 0;
  set<syncable::Id> conflicting_new_folder_ids;
  set<syncable::Id> deleted_folders;
  ConflictProgress* conflict_progress = status->mutable_conflict_progress();
  OrderedCommitSet::Projection proj = status->commit_id_projection();
  if (!proj.empty()) { // Scope for WriteTransaction.
    WriteTransaction trans(FROM_HERE, SYNCER, dir);
    for (size_t i = 0; i < proj.size(); i++) {
      CommitResponse::ResponseType response_type =
          ProcessSingleCommitResponse(&trans, cr.entryresponse(proj[i]),
                                      commit_message.entries(proj[i]),
                                      status->GetCommitIdAt(proj[i]),
                                      &conflicting_new_folder_ids,
                                      &deleted_folders);
      switch (response_type) {
        case CommitResponse::INVALID_MESSAGE:
          ++error_commits;
          break;
        case CommitResponse::CONFLICT:
          ++conflicting_commits;
          // Only server CONFLICT responses will activate conflict resolution.
          conflict_progress->AddConflictingItemById(
              status->GetCommitIdAt(proj[i]));
          break;
        case CommitResponse::SUCCESS:
          // TODO(sync): worry about sync_rate_ rate calc?
          ++successes;
          if (status->GetCommitModelTypeAt(proj[i]) == syncable::BOOKMARKS)
            status->increment_num_successful_bookmark_commits();
          status->increment_num_successful_commits();
          break;
        case CommitResponse::OVER_QUOTA:
          // We handle over quota like a retry, which is same as transient.
        case CommitResponse::RETRY:
        case CommitResponse::TRANSIENT_ERROR:
          // TODO(tim): Now that we have SyncSession::Delegate, we
          // should plumb this directly for exponential backoff purposes rather
          // than trying to infer from HasMoreToSync(). See
          // SyncerThread::CalculatePollingWaitTime.
          ++transient_error_commits;
          break;
        default:
          LOG(FATAL) << "Bad return from ProcessSingleCommitResponse";
      }
    }
  }

  // TODO(sync): move status reporting elsewhere.
  status->increment_num_conflicting_commits_by(conflicting_commits);
  if (0 == successes) {
    status->increment_num_consecutive_transient_error_commits_by(
        transient_error_commits);
    status->increment_num_consecutive_errors_by(transient_error_commits);
  } else {
    status->set_num_consecutive_transient_error_commits(0);
    status->set_num_consecutive_errors(0);
  }
  int commit_count = static_cast<int>(proj.size());
  if (commit_count != (conflicting_commits + error_commits +
                       transient_error_commits)) {
    ResetErrorCounters(status);
  }
  SyncerUtil::MarkDeletedChildrenSynced(dir, &deleted_folders);

  return;
}

void LogServerError(const CommitResponse_EntryResponse& res) {
  if (res.has_error_message())
    LOG(WARNING) << "  " << res.error_message();
  else
    LOG(WARNING) << "  No detailed error message returned from server";
}

CommitResponse::ResponseType
ProcessCommitResponseCommand::ProcessSingleCommitResponse(
    syncable::WriteTransaction* trans,
    const sync_pb::CommitResponse_EntryResponse& pb_server_entry,
    const sync_pb::SyncEntity& commit_request_entry,
    const syncable::Id& pre_commit_id,
    std::set<syncable::Id>* conflicting_new_folder_ids,
    set<syncable::Id>* deleted_folders) {

  const CommitResponse_EntryResponse& server_entry =
      *static_cast<const CommitResponse_EntryResponse*>(&pb_server_entry);
  MutableEntry local_entry(trans, GET_BY_ID, pre_commit_id);
  CHECK(local_entry.good());
  bool syncing_was_set = local_entry.Get(SYNCING);
  local_entry.Put(SYNCING, false);

  CommitResponse::ResponseType response = (CommitResponse::ResponseType)
      server_entry.response_type();
  if (!CommitResponse::ResponseType_IsValid(response)) {
    LOG(ERROR) << "Commit response has unknown response type! Possibly out "
               "of date client?";
    return CommitResponse::INVALID_MESSAGE;
  }
  if (CommitResponse::TRANSIENT_ERROR == response) {
    DVLOG(1) << "Transient Error Committing: " << local_entry;
    LogServerError(server_entry);
    return CommitResponse::TRANSIENT_ERROR;
  }
  if (CommitResponse::INVALID_MESSAGE == response) {
    LOG(ERROR) << "Error Commiting: " << local_entry;
    LogServerError(server_entry);
    return response;
  }
  if (CommitResponse::CONFLICT == response) {
    DVLOG(1) << "Conflict Committing: " << local_entry;
    // TODO(nick): conflicting_new_folder_ids is a purposeless anachronism.
    if (!pre_commit_id.ServerKnows() && local_entry.Get(IS_DIR)) {
      conflicting_new_folder_ids->insert(pre_commit_id);
    }
    return response;
  }
  if (CommitResponse::RETRY == response) {
    DVLOG(1) << "Retry Committing: " << local_entry;
    return response;
  }
  if (CommitResponse::OVER_QUOTA == response) {
    LOG(WARNING) << "Hit deprecated OVER_QUOTA Committing: " << local_entry;
    return response;
  }
  if (!server_entry.has_id_string()) {
    LOG(ERROR) << "Commit response has no id";
    return CommitResponse::INVALID_MESSAGE;
  }

  // Implied by the IsValid call above, but here for clarity.
  DCHECK_EQ(CommitResponse::SUCCESS, response) << response;
  // Check to see if we've been given the ID of an existing entry. If so treat
  // it as an error response and retry later.
  if (pre_commit_id != server_entry.id()) {
    Entry e(trans, GET_BY_ID, server_entry.id());
    if (e.good()) {
      LOG(ERROR) << "Got duplicate id when commiting id: " << pre_commit_id <<
                 ". Treating as an error return";
      return CommitResponse::INVALID_MESSAGE;
    }
  }

  if (server_entry.version() == 0) {
    LOG(WARNING) << "Server returned a zero version on a commit response.";
  }

  ProcessSuccessfulCommitResponse(commit_request_entry, server_entry,
      pre_commit_id, &local_entry, syncing_was_set, deleted_folders);
  return response;
}

const string& ProcessCommitResponseCommand::GetResultingPostCommitName(
    const sync_pb::SyncEntity& committed_entry,
    const CommitResponse_EntryResponse& entry_response) {
  const string& response_name =
      SyncerProtoUtil::NameFromCommitEntryResponse(entry_response);
  if (!response_name.empty())
    return response_name;
  return SyncerProtoUtil::NameFromSyncEntity(committed_entry);
}

bool ProcessCommitResponseCommand::UpdateVersionAfterCommit(
    const sync_pb::SyncEntity& committed_entry,
    const CommitResponse_EntryResponse& entry_response,
    const syncable::Id& pre_commit_id,
    syncable::MutableEntry* local_entry) {
  int64 old_version = local_entry->Get(BASE_VERSION);
  int64 new_version = entry_response.version();
  bool bad_commit_version = false;
  if (committed_entry.deleted() &&
      !local_entry->Get(syncable::UNIQUE_CLIENT_TAG).empty()) {
    // If the item was deleted, and it's undeletable (uses the client tag),
    // change the version back to zero.  We must set the version to zero so
    // that the server knows to re-create the item if it gets committed
    // later for undeletion.
    new_version = 0;
  } else if (!pre_commit_id.ServerKnows()) {
    bad_commit_version = 0 == new_version;
  } else {
    bad_commit_version = old_version > new_version;
  }
  if (bad_commit_version) {
    LOG(ERROR) << "Bad version in commit return for " << *local_entry
               << " new_id:" << entry_response.id() << " new_version:"
               << entry_response.version();
    return false;
  }

  // Update the base version and server version.  The base version must change
  // here, even if syncing_was_set is false; that's because local changes were
  // on top of the successfully committed version.
  local_entry->Put(BASE_VERSION, new_version);
  DVLOG(1) << "Commit is changing base version of " << local_entry->Get(ID)
           << " to: " << new_version;
  local_entry->Put(SERVER_VERSION, new_version);
  return true;
}

bool ProcessCommitResponseCommand::ChangeIdAfterCommit(
    const CommitResponse_EntryResponse& entry_response,
    const syncable::Id& pre_commit_id,
    syncable::MutableEntry* local_entry) {
  syncable::WriteTransaction* trans = local_entry->write_transaction();
  if (entry_response.id() != pre_commit_id) {
    if (pre_commit_id.ServerKnows()) {
      // The server can sometimes generate a new ID on commit; for example,
      // when committing an undeletion.
      DVLOG(1) << " ID changed while committing an old entry. "
               << pre_commit_id << " became " << entry_response.id() << ".";
    }
    MutableEntry same_id(trans, GET_BY_ID, entry_response.id());
    // We should trap this before this function.
    if (same_id.good()) {
      LOG(ERROR) << "ID clash with id " << entry_response.id()
                 << " during commit " << same_id;
      return false;
    }
    SyncerUtil::ChangeEntryIDAndUpdateChildren(
        trans, local_entry, entry_response.id());
    DVLOG(1) << "Changing ID to " << entry_response.id();
  }
  return true;
}

void ProcessCommitResponseCommand::UpdateServerFieldsAfterCommit(
    const sync_pb::SyncEntity& committed_entry,
    const CommitResponse_EntryResponse& entry_response,
    syncable::MutableEntry* local_entry) {

  // We just committed an entry successfully, and now we want to make our view
  // of the server state consistent with the server state. We must be careful;
  // |entry_response| and |committed_entry| have some identically named
  // fields.  We only want to consider fields from |committed_entry| when there
  // is not an overriding field in the |entry_response|.  We do not want to
  // update the server data from the local data in the entry -- it's possible
  // that the local data changed during the commit, and even if not, the server
  // has the last word on the values of several properties.

  local_entry->Put(SERVER_IS_DEL, committed_entry.deleted());
  if (committed_entry.deleted()) {
    // Don't clobber any other fields of deleted objects.
    return;
  }

  local_entry->Put(syncable::SERVER_IS_DIR,
      (committed_entry.folder() ||
       committed_entry.bookmarkdata().bookmark_folder()));
  local_entry->Put(syncable::SERVER_SPECIFICS,
      committed_entry.specifics());
  local_entry->Put(syncable::SERVER_MTIME,
                   ProtoTimeToTime(committed_entry.mtime()));
  local_entry->Put(syncable::SERVER_CTIME,
                   ProtoTimeToTime(committed_entry.ctime()));
  local_entry->Put(syncable::SERVER_POSITION_IN_PARENT,
      entry_response.position_in_parent());
  // TODO(nick): The server doesn't set entry_response.server_parent_id in
  // practice; to update SERVER_PARENT_ID appropriately here we'd need to
  // get the post-commit ID of the parent indicated by
  // committed_entry.parent_id_string(). That should be inferrable from the
  // information we have, but it's a bit convoluted to pull it out directly.
  // Getting this right is important: SERVER_PARENT_ID gets fed back into
  // old_parent_id during the next commit.
  local_entry->Put(syncable::SERVER_PARENT_ID,
      local_entry->Get(syncable::PARENT_ID));
  local_entry->Put(syncable::SERVER_NON_UNIQUE_NAME,
      GetResultingPostCommitName(committed_entry, entry_response));

  if (local_entry->Get(IS_UNAPPLIED_UPDATE)) {
    // This shouldn't happen; an unapplied update shouldn't be committed, and
    // if it were, the commit should have failed.  But if it does happen: we've
    // just overwritten the update info, so clear the flag.
    local_entry->Put(IS_UNAPPLIED_UPDATE, false);
  }
}

void ProcessCommitResponseCommand::OverrideClientFieldsAfterCommit(
    const sync_pb::SyncEntity& committed_entry,
    const CommitResponse_EntryResponse& entry_response,
    syncable::MutableEntry* local_entry) {
  if (committed_entry.deleted()) {
    // If an entry's been deleted, nothing else matters.
    DCHECK(local_entry->Get(IS_DEL));
    return;
  }

  // Update the name.
  const string& server_name =
      GetResultingPostCommitName(committed_entry, entry_response);
  const string& old_name =
      local_entry->Get(syncable::NON_UNIQUE_NAME);

  if (!server_name.empty() && old_name != server_name) {
    DVLOG(1) << "During commit, server changed name: " << old_name
             << " to new name: " << server_name;
    local_entry->Put(syncable::NON_UNIQUE_NAME, server_name);
  }

  // The server has the final say on positioning, so apply the absolute
  // position that it returns.
  if (entry_response.has_position_in_parent()) {
    // The SERVER_ field should already have been written.
    DCHECK_EQ(entry_response.position_in_parent(),
        local_entry->Get(SERVER_POSITION_IN_PARENT));

    // We just committed successfully, so we assume that the position
    // value we got applies to the PARENT_ID we submitted.
    syncable::Id new_prev = local_entry->ComputePrevIdFromServerPosition(
        local_entry->Get(PARENT_ID));
    if (!local_entry->PutPredecessor(new_prev)) {
      // TODO(lipalani) : Propagate the error to caller. crbug.com/100444.
      NOTREACHED();
    }
  }
}

void ProcessCommitResponseCommand::ProcessSuccessfulCommitResponse(
    const sync_pb::SyncEntity& committed_entry,
    const CommitResponse_EntryResponse& entry_response,
    const syncable::Id& pre_commit_id, syncable::MutableEntry* local_entry,
    bool syncing_was_set, set<syncable::Id>* deleted_folders) {
  DCHECK(local_entry->Get(IS_UNSYNCED));

  // Update SERVER_VERSION and BASE_VERSION.
  if (!UpdateVersionAfterCommit(committed_entry, entry_response, pre_commit_id,
                                local_entry)) {
    LOG(ERROR) << "Bad version in commit return for " << *local_entry
               << " new_id:" << entry_response.id() << " new_version:"
               << entry_response.version();
    return;
  }

  // If the server gave us a new ID, apply it.
  if (!ChangeIdAfterCommit(entry_response, pre_commit_id, local_entry)) {
    return;
  }

  // Update our stored copy of the server state.
  UpdateServerFieldsAfterCommit(committed_entry, entry_response, local_entry);

  // If the item doesn't need to be committed again (an item might need to be
  // committed again if it changed locally during the commit), we can remove
  // it from the unsynced list.  Also, we should change the locally-
  // visible properties to apply any canonicalizations or fixups
  // that the server introduced during the commit.
  if (syncing_was_set) {
    OverrideClientFieldsAfterCommit(committed_entry, entry_response,
                                    local_entry);
    local_entry->Put(IS_UNSYNCED, false);
  }

  // Make a note of any deleted folders, whose children would have
  // been recursively deleted.
  // TODO(nick): Here, commit_message.deleted() would be more correct than
  // local_entry->Get(IS_DEL).  For example, an item could be renamed, and then
  // deleted during the commit of the rename.  Unit test & fix.
  if (local_entry->Get(IS_DIR) && local_entry->Get(IS_DEL)) {
    deleted_folders->insert(local_entry->Get(ID));
  }
}

}  // namespace browser_sync
