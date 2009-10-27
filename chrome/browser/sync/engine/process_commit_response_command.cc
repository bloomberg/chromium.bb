// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/process_commit_response_command.h"

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_status.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/character_set_converters.h"

using syncable::ScopedDirLookup;
using syncable::WriteTransaction;
using syncable::MutableEntry;
using syncable::Entry;
using syncable::Name;
using syncable::SyncName;
using syncable::DBName;

using std::set;
using std::vector;

using syncable::BASE_VERSION;
using syncable::GET_BY_ID;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::PARENT_ID;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_POSITION_IN_PARENT;
using syncable::SYNCER;
using syncable::SYNCING;

namespace browser_sync {

void IncrementErrorCounters(SyncerStatus status) {
  status.increment_consecutive_problem_commits();
  status.increment_consecutive_errors();
}
void ResetErrorCounters(SyncerStatus status) {
  status.zero_consecutive_problem_commits();
  status.zero_consecutive_errors();
}

ProcessCommitResponseCommand::ProcessCommitResponseCommand() {}
ProcessCommitResponseCommand::~ProcessCommitResponseCommand() {}

void ProcessCommitResponseCommand::ModelChangingExecuteImpl(
    SyncerSession* session) {
  // TODO(sync): This function returns if it sees problems. We probably want
  // to flag the need for an update or similar.
  ScopedDirLookup dir(session->dirman(), session->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }
  const ClientToServerResponse& response = session->commit_response();
  const vector<syncable::Id>& commit_ids = session->commit_ids();

  // TODO(sync): move counters out of here.
  SyncerStatus status(session);

  if (!response.has_commit()) {
    // TODO(sync): What if we didn't try to commit anything?
    LOG(WARNING) << "Commit response has no commit body!";
    IncrementErrorCounters(status);
    return;
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
    IncrementErrorCounters(status);
    return;
  }

  // If we try to commit a parent and child together and the parent conflicts
  // the child will have a bad parent causing an error. As this is not a
  // critical error, we trap it and don't LOG(ERROR). To enable this we keep
  // a map of conflicting new folders.
  int transient_error_commits = 0;
  int conflicting_commits = 0;
  int error_commits = 0;
  int successes = 0;
  bool over_quota = false;
  set<syncable::Id> conflicting_new_folder_ids;
  set<syncable::Id> deleted_folders;
  { // Scope for WriteTransaction.
    WriteTransaction trans(dir, SYNCER, __FILE__, __LINE__);
    for (int i = 0; i < cr.entryresponse_size(); i++) {
      CommitResponse::RESPONSE_TYPE response_type =
          ProcessSingleCommitResponse(&trans, cr.entryresponse(i),
                                      commit_ids[i],
                                      &conflicting_new_folder_ids,
                                      &deleted_folders, session);
      switch (response_type) {
        case CommitResponse::INVALID_MESSAGE:
          ++error_commits;
          break;
        case CommitResponse::CONFLICT:
          ++conflicting_commits;
          session->AddCommitConflict(commit_ids[i]);
          break;
        case CommitResponse::SUCCESS:
          // TODO(sync): worry about sync_rate_ rate calc?
          ++successes;
          status.increment_successful_commits();
          break;
        case CommitResponse::OVER_QUOTA:
          over_quota = true;
          // We handle over quota like a retry.
        case CommitResponse::RETRY:
          session->AddBlockedItem(commit_ids[i]);
          break;
        case CommitResponse::TRANSIENT_ERROR:
          ++transient_error_commits;
          break;
        default:
          LOG(FATAL) << "Bad return from ProcessSingleCommitResponse";
      }
    }
  }

  // TODO(sync): move status reporting elsewhere.
  status.set_conflicting_commits(conflicting_commits);
  status.set_error_commits(error_commits);
  if (0 == successes) {
    status.increment_consecutive_transient_error_commits_by(
        transient_error_commits);
    status.increment_consecutive_errors_by(transient_error_commits);
  } else {
    status.zero_consecutive_transient_error_commits();
    status.zero_consecutive_errors();
  }
  // If all commits are errors count it as an error.
  if (commit_count == error_commits) {
    // A larger error step than normal because a POST just succeeded.
    status.TallyBigNewError();
  }
  if (commit_count != (conflicting_commits + error_commits +
                       transient_error_commits)) {
    ResetErrorCounters(status);
  }
  SyncerUtil::MarkDeletedChildrenSynced(dir, &deleted_folders);
  session->set_over_quota(over_quota);

  return;
}

void LogServerError(const CommitResponse_EntryResponse& res) {
  if (res.has_error_message())
    LOG(ERROR) << "  " << res.error_message();
  else
    LOG(ERROR) << "  No detailed error message returned from server";
}

CommitResponse::RESPONSE_TYPE
ProcessCommitResponseCommand::ProcessSingleCommitResponse(
    syncable::WriteTransaction* trans,
    const sync_pb::CommitResponse_EntryResponse& pb_server_entry,
    const syncable::Id& pre_commit_id,
    std::set<syncable::Id>* conflicting_new_folder_ids,
    set<syncable::Id>* deleted_folders,
    SyncerSession* const session) {

  const CommitResponse_EntryResponse& server_entry =
      *static_cast<const CommitResponse_EntryResponse*>(&pb_server_entry);
  MutableEntry local_entry(trans, GET_BY_ID, pre_commit_id);
  CHECK(local_entry.good());
  bool syncing_was_set = local_entry.Get(SYNCING);
  local_entry.Put(SYNCING, false);

  CommitResponse::RESPONSE_TYPE response = (CommitResponse::RESPONSE_TYPE)
      server_entry.response_type();
  if (!CommitResponse::RESPONSE_TYPE_IsValid(response)) {
    LOG(ERROR) << "Commit response has unknown response type! Possibly out "
               "of date client?";
    return CommitResponse::INVALID_MESSAGE;
  }
  if (CommitResponse::TRANSIENT_ERROR == response) {
    LOG(INFO) << "Transient Error Committing: " << local_entry;
    LogServerError(server_entry);
    return CommitResponse::TRANSIENT_ERROR;
  }
  if (CommitResponse::INVALID_MESSAGE == response) {
    LOG(ERROR) << "Error Commiting: " << local_entry;
    LogServerError(server_entry);
    return response;
  }
  if (CommitResponse::CONFLICT == response) {
    LOG(INFO) << "Conflict Committing: " << local_entry;
    if (!pre_commit_id.ServerKnows() && local_entry.Get(IS_DIR)) {
      conflicting_new_folder_ids->insert(pre_commit_id);
    }
    return response;
  }
  if (CommitResponse::RETRY == response) {
    LOG(INFO) << "Retry Committing: " << local_entry;
    return response;
  }
  if (CommitResponse::OVER_QUOTA == response) {
    LOG(INFO) << "Hit Quota Committing: " << local_entry;
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

  ProcessSuccessfulCommitResponse(trans, server_entry, pre_commit_id,
                                  &local_entry, syncing_was_set,
                                  deleted_folders, session);
  return response;
}

void ProcessCommitResponseCommand::ProcessSuccessfulCommitResponse(
    syncable::WriteTransaction* trans,
    const CommitResponse_EntryResponse& server_entry,
    const syncable::Id& pre_commit_id, syncable::MutableEntry* local_entry,
    bool syncing_was_set, set<syncable::Id>* deleted_folders,
    SyncerSession* const session) {
  int64 old_version = local_entry->Get(BASE_VERSION);
  int64 new_version = server_entry.version();
  bool bad_commit_version = false;
  // TODO(sync): The !server_entry.has_id_string() clauses below were
  // introduced when working with the new protocol.
  if (!pre_commit_id.ServerKnows())
    bad_commit_version = 0 == new_version;
  else
    bad_commit_version = old_version > new_version;
  if (bad_commit_version) {
    LOG(ERROR) << "Bad version in commit return for " << *local_entry <<
        " new_id:" << server_entry.id() << " new_version:" <<
        server_entry.version();
    return;
  }
  if (server_entry.id() != pre_commit_id) {
    if (pre_commit_id.ServerKnows()) {
      // TODO(sync): In future it's possible that we'll want the opportunity
      // to do a server triggered move aside here.
      LOG(ERROR) << " ID change but not committing a new entry. " <<
          pre_commit_id << " became " << server_entry.id() << ".";
      return;
    }
    if (!server_entry.id().ServerKnows()) {
      LOG(ERROR) << " New entries id < 0." << pre_commit_id << " became " <<
          server_entry.id() << ".";
      return;
    }
    MutableEntry same_id(trans, GET_BY_ID, server_entry.id());
    // We should trap this before this function.
    CHECK(!same_id.good()) << "ID clash with id " << server_entry.id() <<
        " during commit " << same_id;
    SyncerUtil::ChangeEntryIDAndUpdateChildren(
        trans, local_entry, server_entry.id());
    LOG(INFO) << "Changing ID to " << server_entry.id();
  }

  local_entry->Put(BASE_VERSION, new_version);
  LOG(INFO) << "Commit is changing base version of " <<
    local_entry->Get(ID) << " to: " << new_version;

  if (local_entry->Get(IS_UNAPPLIED_UPDATE)) {
    // This is possible, but very unlikely.
    local_entry->Put(IS_UNAPPLIED_UPDATE, false);
  }

  if (server_entry.has_name()) {
    if (syncing_was_set) {
      PerformCommitTimeNameAside(trans, server_entry, local_entry);
    } else {
      // IS_UNSYNCED will ensure that this entry gets committed again, even if
      // we skip this name aside. IS_UNSYNCED was probably previously set, but
      // let's just set it anyway.
      local_entry->Put(IS_UNSYNCED, true);
      LOG(INFO) << "Skipping commit time name aside because" <<
          " entry was changed during commit.";
    }
  }

  if (syncing_was_set && server_entry.has_position_in_parent()) {
    // The server has the final say on positioning, so apply the absolute
    // position that it returns.
    local_entry->Put(SERVER_POSITION_IN_PARENT,
        server_entry.position_in_parent());

    // We just committed successfully, so we assume that the position
    // value we got applies to the PARENT_ID we submitted.
    syncable::Id new_prev = SyncerUtil::ComputePrevIdFromServerPosition(
        trans, local_entry, local_entry->Get(PARENT_ID));
    if (!local_entry->PutPredecessor(new_prev)) {
      LOG(WARNING) << "PutPredecessor failed after successful commit";
    }
  }

  if (syncing_was_set) {
    local_entry->Put(IS_UNSYNCED, false);
  }
  if (local_entry->Get(IS_DIR) && local_entry->Get(IS_DEL)) {
    deleted_folders->insert(local_entry->Get(ID));
  }
}

void ProcessCommitResponseCommand::PerformCommitTimeNameAside(
    syncable::WriteTransaction* trans,
    const CommitResponse_EntryResponse& server_entry,
    syncable::MutableEntry* local_entry) {
  Name old_name(local_entry->GetName());

  // Ensure that we don't collide with an existing entry.
  SyncName server_name =
      SyncerProtoUtil::NameFromCommitEntryResponse(server_entry);

  LOG(INFO) << "Server provided committed name:" << server_name.value();
  if (!server_name.value().empty() &&
      static_cast<SyncName&>(old_name) != server_name) {
    LOG(INFO) << "Server name differs from local name, attempting"
              << " commit time name aside.";

    DBName db_name(server_name.value());
    db_name.MakeOSLegal();

    // This is going to produce ~1 names instead of (Edited) names.
    // Since this should be EXTREMELY rare, we do this for now.
    db_name.MakeNoncollidingForEntry(trans, local_entry->Get(SERVER_PARENT_ID),
                                     local_entry);

    CHECK(!db_name.empty());

    LOG(INFO) << "Server commit moved aside entry: " << old_name.db_value()
              << " to new name " << db_name;

    // Should be safe since we're in a "commit lock."
    local_entry->PutName(Name::FromDBNameAndSyncName(db_name, server_name));
  }
}

}  // namespace browser_sync
