// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/sync/engine/apply_updates_command.h"
#include "chrome/browser/sync/engine/build_and_process_conflict_sets_command.h"
#include "chrome/browser/sync/engine/build_commit_command.h"
#include "chrome/browser/sync/engine/cleanup_disabled_types_command.h"
#include "chrome/browser/sync/engine/clear_data_command.h"
#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/download_updates_command.h"
#include "chrome/browser/sync/engine/get_commit_ids_command.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/post_commit_message_command.h"
#include "chrome/browser/sync/engine/process_commit_response_command.h"
#include "chrome/browser/sync/engine/process_updates_command.h"
#include "chrome/browser/sync/engine/resolve_conflicts_command.h"
#include "chrome/browser/sync/engine/store_timestamps_command.h"
#include "chrome/browser/sync/engine/syncer_end_command.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/engine/verify_updates_command.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable.h"

using base::TimeDelta;
using sync_pb::ClientCommand;
using syncable::Blob;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::SERVER_CTIME;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_MTIME;
using syncable::SERVER_NON_UNIQUE_NAME;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_POSITION_IN_PARENT;
using syncable::SERVER_SPECIFICS;
using syncable::SERVER_VERSION;
using syncable::SYNCER;
using syncable::ScopedDirLookup;
using syncable::WriteTransaction;

namespace browser_sync {

using sessions::ScopedSessionContextConflictResolver;
using sessions::StatusController;
using sessions::SyncSession;
using sessions::ConflictProgress;

Syncer::Syncer()
    : early_exit_requested_(false),
      max_commit_batch_size_(kDefaultMaxCommitBatchSize),
      pre_conflict_resolution_closure_(NULL) {
}

Syncer::~Syncer() {}

bool Syncer::ExitRequested() {
  base::AutoLock lock(early_exit_requested_lock_);
  return early_exit_requested_;
}

void Syncer::RequestEarlyExit() {
  base::AutoLock lock(early_exit_requested_lock_);
  early_exit_requested_ = true;
}

void Syncer::SyncShare(sessions::SyncSession* session) {
  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  // The directory must be good here.
  CHECK(dir.good());

  const sessions::SyncSourceInfo& source(session->source());
  if (sync_pb::GetUpdatesCallerInfo::CLEAR_PRIVATE_DATA ==
      source.updates_source) {
    SyncShare(session, CLEAR_PRIVATE_DATA, SYNCER_END);
    return;
  } else {
    // This isn't perfect, as we can end up bundling extensions activity
    // intended for the next session into the current one.  We could do a
    // test-and-reset as with the source, but note that also falls short if
    // the commit request fails (e.g. due to lost connection), as we will
    // fall all the way back to the syncer thread main loop in that case, and
    // wind up creating a new session when a connection is established, losing
    // the records set here on the original attempt.  This should provide us
    // with the right data "most of the time", and we're only using this for
    // analysis purposes, so Law of Large Numbers FTW.
    session->context()->extensions_monitor()->GetAndClearRecords(
        session->mutable_extensions_activity());
    SyncShare(session, SYNCER_BEGIN, SYNCER_END);
  }
}

void Syncer::SyncShare(sessions::SyncSession* session,
                       const SyncerStep first_step,
                       const SyncerStep last_step) {
  ScopedSessionContextConflictResolver scoped(session->context(),
                                              &resolver_);
  SyncerStep current_step = first_step;

  SyncerStep next_step = current_step;
  while (!ExitRequested()) {
    switch (current_step) {
      case SYNCER_BEGIN:
        VLOG(1) << "Syncer Begin";
        next_step = CLEANUP_DISABLED_TYPES;
        break;
      case CLEANUP_DISABLED_TYPES: {
        VLOG(1) << "Cleaning up disabled types";
        CleanupDisabledTypesCommand cleanup;
        cleanup.Execute(session);
        next_step = DOWNLOAD_UPDATES;
        break;
      }
      case DOWNLOAD_UPDATES: {
        VLOG(1) << "Downloading Updates";
        DownloadUpdatesCommand download_updates;
        download_updates.Execute(session);
        next_step = PROCESS_CLIENT_COMMAND;
        break;
      }
      case PROCESS_CLIENT_COMMAND: {
        VLOG(1) << "Processing Client Command";
        ProcessClientCommand(session);
        next_step = VERIFY_UPDATES;
        break;
      }
      case VERIFY_UPDATES: {
        VLOG(1) << "Verifying Updates";
        VerifyUpdatesCommand verify_updates;
        verify_updates.Execute(session);
        next_step = PROCESS_UPDATES;
        break;
      }
      case PROCESS_UPDATES: {
        VLOG(1) << "Processing Updates";
        ProcessUpdatesCommand process_updates;
        process_updates.Execute(session);
        next_step = STORE_TIMESTAMPS;
        break;
      }
      case STORE_TIMESTAMPS: {
        VLOG(1) << "Storing timestamps";
        StoreTimestampsCommand store_timestamps;
        store_timestamps.Execute(session);
        // We should download all of the updates before attempting to process
        // them.
        if (session->status_controller()->ServerSaysNothingMoreToDownload() ||
            !session->status_controller()->download_updates_succeeded()) {
          next_step = APPLY_UPDATES;
        } else {
          next_step = DOWNLOAD_UPDATES;
        }
        break;
      }
      case APPLY_UPDATES: {
        VLOG(1) << "Applying Updates";
        ApplyUpdatesCommand apply_updates;
        apply_updates.Execute(session);
        next_step = BUILD_COMMIT_REQUEST;
        break;
      }
      // These two steps are combined since they are executed within the same
      // write transaction.
      case BUILD_COMMIT_REQUEST: {
        session->status_controller()->set_syncing(true);

        VLOG(1) << "Processing Commit Request";
        ScopedDirLookup dir(session->context()->directory_manager(),
                            session->context()->account_name());
        if (!dir.good()) {
          LOG(ERROR) << "Scoped dir lookup failed!";
          return;
        }
        WriteTransaction trans(dir, SYNCER, __FILE__, __LINE__);
        sessions::ScopedSetSessionWriteTransaction set_trans(session, &trans);

        VLOG(1) << "Getting the Commit IDs";
        GetCommitIdsCommand get_commit_ids_command(max_commit_batch_size_);
        get_commit_ids_command.Execute(session);

        if (!session->status_controller()->commit_ids().empty()) {
          VLOG(1) << "Building a commit message";
          BuildCommitCommand build_commit_command;
          build_commit_command.Execute(session);

          next_step = POST_COMMIT_MESSAGE;
        } else {
          next_step = BUILD_AND_PROCESS_CONFLICT_SETS;
        }

        break;
      }
      case POST_COMMIT_MESSAGE: {
        VLOG(1) << "Posting a commit request";
        PostCommitMessageCommand post_commit_command;
        post_commit_command.Execute(session);
        next_step = PROCESS_COMMIT_RESPONSE;
        break;
      }
      case PROCESS_COMMIT_RESPONSE: {
        VLOG(1) << "Processing the commit response";
        session->status_controller()->reset_num_conflicting_commits();
        ProcessCommitResponseCommand process_response_command;
        process_response_command.Execute(session);
        next_step = BUILD_AND_PROCESS_CONFLICT_SETS;
        break;
      }
      case BUILD_AND_PROCESS_CONFLICT_SETS: {
        VLOG(1) << "Building and Processing Conflict Sets";
        BuildAndProcessConflictSetsCommand build_process_conflict_sets;
        build_process_conflict_sets.Execute(session);
        if (session->status_controller()->conflict_sets_built())
          next_step = SYNCER_END;
        else
          next_step = RESOLVE_CONFLICTS;
        break;
      }
      case RESOLVE_CONFLICTS: {
        VLOG(1) << "Resolving Conflicts";

        // Trigger the pre_conflict_resolution_closure_, which is a testing
        // hook for the unit tests, if it is non-NULL.
        if (pre_conflict_resolution_closure_) {
          pre_conflict_resolution_closure_->Run();
        }

        StatusController* status = session->status_controller();
        status->reset_conflicts_resolved();
        ResolveConflictsCommand resolve_conflicts_command;
        resolve_conflicts_command.Execute(session);
        if (status->HasConflictingUpdates())
          next_step = APPLY_UPDATES_TO_RESOLVE_CONFLICTS;
        else
          next_step = SYNCER_END;
        break;
      }
      case APPLY_UPDATES_TO_RESOLVE_CONFLICTS: {
        StatusController* status = session->status_controller();
        VLOG(1) << "Applying updates to resolve conflicts";
        ApplyUpdatesCommand apply_updates;
        int before_conflicting_updates = status->TotalNumConflictingItems();
        apply_updates.Execute(session);
        int after_conflicting_updates = status->TotalNumConflictingItems();
        status->update_conflicts_resolved(before_conflicting_updates >
                                          after_conflicting_updates);
        if (status->conflicts_resolved())
          next_step = RESOLVE_CONFLICTS;
        else
          next_step = SYNCER_END;
        break;
      }
      case CLEAR_PRIVATE_DATA: {
        VLOG(1) << "Clear Private Data";
        ClearDataCommand clear_data_command;
        clear_data_command.Execute(session);
        next_step = SYNCER_END;
      }
      case SYNCER_END: {
        VLOG(1) << "Syncer End";
        SyncerEndCommand syncer_end_command;
        // This will set "syncing" to false, and send out a notification.
        syncer_end_command.Execute(session);
        goto post_while;
      }
      default:
        LOG(ERROR) << "Unknown command: " << current_step;
    }
    if (last_step == current_step)
      break;
    current_step = next_step;
  }
 post_while:
  return;
}

void Syncer::ProcessClientCommand(sessions::SyncSession* session) {
  const ClientToServerResponse& response =
      session->status_controller()->updates_response();
  if (!response.has_client_command())
    return;
  const ClientCommand& command = response.client_command();

  // The server limits the number of items a client can commit in one batch.
  if (command.has_max_commit_batch_size())
    max_commit_batch_size_ = command.max_commit_batch_size();
  if (command.has_set_sync_long_poll_interval()) {
    session->delegate()->OnReceivedLongPollIntervalUpdate(
        TimeDelta::FromSeconds(command.set_sync_long_poll_interval()));
  }
  if (command.has_set_sync_poll_interval()) {
    session->delegate()->OnReceivedShortPollIntervalUpdate(
        TimeDelta::FromSeconds(command.set_sync_poll_interval()));
  }
}

void CopyServerFields(syncable::Entry* src, syncable::MutableEntry* dest) {
  dest->Put(SERVER_NON_UNIQUE_NAME, src->Get(SERVER_NON_UNIQUE_NAME));
  dest->Put(SERVER_PARENT_ID, src->Get(SERVER_PARENT_ID));
  dest->Put(SERVER_MTIME, src->Get(SERVER_MTIME));
  dest->Put(SERVER_CTIME, src->Get(SERVER_CTIME));
  dest->Put(SERVER_VERSION, src->Get(SERVER_VERSION));
  dest->Put(SERVER_IS_DIR, src->Get(SERVER_IS_DIR));
  dest->Put(SERVER_IS_DEL, src->Get(SERVER_IS_DEL));
  dest->Put(IS_UNAPPLIED_UPDATE, src->Get(IS_UNAPPLIED_UPDATE));
  dest->Put(SERVER_SPECIFICS, src->Get(SERVER_SPECIFICS));
  dest->Put(SERVER_POSITION_IN_PARENT, src->Get(SERVER_POSITION_IN_PARENT));
}

void ClearServerData(syncable::MutableEntry* entry) {
  entry->Put(SERVER_NON_UNIQUE_NAME, "");
  entry->Put(SERVER_PARENT_ID, syncable::kNullId);
  entry->Put(SERVER_MTIME, 0);
  entry->Put(SERVER_CTIME, 0);
  entry->Put(SERVER_VERSION, 0);
  entry->Put(SERVER_IS_DIR, false);
  entry->Put(SERVER_IS_DEL, false);
  entry->Put(IS_UNAPPLIED_UPDATE, false);
  entry->Put(SERVER_SPECIFICS, sync_pb::EntitySpecifics::default_instance());
  entry->Put(SERVER_POSITION_IN_PARENT, 0);
}

}  // namespace browser_sync
