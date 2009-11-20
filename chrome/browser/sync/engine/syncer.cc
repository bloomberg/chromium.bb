// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.

#include "chrome/browser/sync/engine/syncer.h"

#include "base/format_macros.h"
#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/engine/apply_updates_command.h"
#include "chrome/browser/sync/engine/build_and_process_conflict_sets_command.h"
#include "chrome/browser/sync/engine/build_commit_command.h"
#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/download_updates_command.h"
#include "chrome/browser/sync/engine/get_commit_ids_command.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/post_commit_message_command.h"
#include "chrome/browser/sync/engine/process_commit_response_command.h"
#include "chrome/browser/sync/engine/process_updates_command.h"
#include "chrome/browser/sync/engine/resolve_conflicts_command.h"
#include "chrome/browser/sync/engine/syncer_end_command.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/engine/verify_updates_command.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/closure.h"

using sync_pb::ClientCommand;
using syncable::Blob;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::SERVER_BOOKMARK_FAVICON;
using syncable::SERVER_BOOKMARK_URL;
using syncable::SERVER_CTIME;
using syncable::SERVER_IS_BOOKMARK_OBJECT;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_MTIME;
using syncable::SERVER_NON_UNIQUE_NAME;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_POSITION_IN_PARENT;
using syncable::SERVER_VERSION;
using syncable::SYNCER;
using syncable::ScopedDirLookup;
using syncable::WriteTransaction;

namespace browser_sync {

Syncer::Syncer(
    syncable::DirectoryManager* dirman,
    const PathString& account_name,
    ServerConnectionManager* connection_manager,
    ModelSafeWorker* model_safe_worker)
    : account_name_(account_name),
      early_exit_requested_(false),
      max_commit_batch_size_(kDefaultMaxCommitBatchSize),
      connection_manager_(connection_manager),
      dirman_(dirman),
      command_channel_(NULL),
      model_safe_worker_(model_safe_worker),
      updates_source_(sync_pb::GetUpdatesCallerInfo::UNKNOWN),
      notifications_enabled_(false),
      pre_conflict_resolution_closure_(NULL) {
  SyncerEvent shutdown = { SyncerEvent::SHUTDOWN_USE_WITH_CARE };
  syncer_event_channel_.reset(new SyncerEventChannel(shutdown));
  shutdown_channel_.reset(new ShutdownChannel(this));

  extensions_monitor_ = new ExtensionsActivityMonitor();

  ScopedDirLookup dir(dirman_, account_name_);
  // The directory must be good here.
  CHECK(dir.good());
}

Syncer::~Syncer() {
  if (!ChromeThread::DeleteSoon(ChromeThread::UI, FROM_HERE,
                                extensions_monitor_)) {
    // In unittests, there may be no UI thread, so the above will fail.
    delete extensions_monitor_;
  }
  extensions_monitor_ = NULL;
}

void Syncer::RequestNudge(int milliseconds) {
  SyncerEvent event;
  event.what_happened = SyncerEvent::REQUEST_SYNC_NUDGE;
  event.nudge_delay_milliseconds = milliseconds;
  channel()->NotifyListeners(event);
}

bool Syncer::SyncShare() {
  SyncProcessState state(dirman_, account_name_, connection_manager_,
                         &resolver_, syncer_event_channel_.get(),
                         model_safe_worker());
  return SyncShare(&state);
}

bool Syncer::SyncShare(SyncProcessState* process_state) {
  SyncCycleState cycle_state;
  SyncerSession session(&cycle_state, process_state);
  session.set_source(TestAndSetUpdatesSource());
  session.set_notifications_enabled(notifications_enabled());
  // This isn't perfect, as we can end up bundling extensions activity
  // intended for the next session into the current one.  We could do a
  // test-and-reset as with the source, but note that also falls short if
  // the commit request fails (due to lost connection, for example), as we will
  // fall all the way back to the syncer thread main loop in that case, and
  // wind up creating a new session when a connection is established, losing
  // the records set here on the original attempt.  This should provide us
  // with the right data "most of the time", and we're only using this for
  // analysis purposes, so Law of Large Numbers FTW.
  extensions_monitor_->GetAndClearRecords(
      session.mutable_extensions_activity());
  SyncShare(&session, SYNCER_BEGIN, SYNCER_END);
  return session.HasMoreToSync();
}

bool Syncer::SyncShare(SyncerStep first_step, SyncerStep last_step) {
  SyncCycleState cycle_state;
  SyncProcessState state(dirman_, account_name_, connection_manager_,
                         &resolver_, syncer_event_channel_.get(),
                         model_safe_worker());
  SyncerSession session(&cycle_state, &state);
  SyncShare(&session, first_step, last_step);
  return session.HasMoreToSync();
}

void Syncer::SyncShare(SyncerSession* session) {
  SyncShare(session, SYNCER_BEGIN, SYNCER_END);
}

void Syncer::SyncShare(SyncerSession* session,
                       const SyncerStep first_step,
                       const SyncerStep last_step) {
  SyncerStep current_step = first_step;

  // Reset silenced_until_, it is the callers responsibility to honor throttles.
  silenced_until_ = session->silenced_until();

  SyncerStep next_step = current_step;
  while (!ExitRequested()) {
    switch (current_step) {
      case SYNCER_BEGIN:
        LOG(INFO) << "Syncer Begin";
        next_step = DOWNLOAD_UPDATES;
        break;
      case DOWNLOAD_UPDATES: {
        LOG(INFO) << "Downloading Updates";
        DownloadUpdatesCommand download_updates;
        download_updates.Execute(session);
        next_step = PROCESS_CLIENT_COMMAND;
        break;
      }
      case PROCESS_CLIENT_COMMAND: {
        LOG(INFO) << "Processing Client Command";
        ProcessClientCommand(session);
        next_step = VERIFY_UPDATES;
        break;
      }
      case VERIFY_UPDATES: {
        LOG(INFO) << "Verifying Updates";
        VerifyUpdatesCommand verify_updates;
        verify_updates.Execute(session);
        next_step = PROCESS_UPDATES;
        break;
      }
      case PROCESS_UPDATES: {
        LOG(INFO) << "Processing Updates";
        ProcessUpdatesCommand process_updates;
        process_updates.Execute(session);
        // We should download all of the updates before attempting to process
        // them.
        if (session->CountUpdates() == 0) {
          next_step = APPLY_UPDATES;
        } else {
          next_step = DOWNLOAD_UPDATES;
        }
        break;
      }
      case APPLY_UPDATES: {
        LOG(INFO) << "Applying Updates";
        ApplyUpdatesCommand apply_updates;
        apply_updates.Execute(session);
        next_step = BUILD_COMMIT_REQUEST;
        break;
      }
      // These two steps are combined since they are executed within the same
      // write transaction.
      case BUILD_COMMIT_REQUEST: {
        SyncerStatus status(session);
        status.set_syncing(true);

        LOG(INFO) << "Processing Commit Request";
        ScopedDirLookup dir(session->dirman(), session->account_name());
        if (!dir.good()) {
          LOG(ERROR) << "Scoped dir lookup failed!";
          return;
        }
        WriteTransaction trans(dir, SYNCER, __FILE__, __LINE__);
        SyncerSession::ScopedSetWriteTransaction set_trans(session, &trans);

        LOG(INFO) << "Getting the Commit IDs";
        GetCommitIdsCommand get_commit_ids_command(max_commit_batch_size_);
        get_commit_ids_command.Execute(session);

        if (!session->commit_ids().empty()) {
          LOG(INFO) << "Building a commit message";
          BuildCommitCommand build_commit_command;
          build_commit_command.Execute(session);

          next_step = POST_COMMIT_MESSAGE;
        } else {
          next_step = BUILD_AND_PROCESS_CONFLICT_SETS;
        }

        break;
      }
      case POST_COMMIT_MESSAGE: {
        LOG(INFO) << "Posting a commit request";
        PostCommitMessageCommand post_commit_command;
        post_commit_command.Execute(session);
        next_step = PROCESS_COMMIT_RESPONSE;
        break;
      }
      case PROCESS_COMMIT_RESPONSE: {
        LOG(INFO) << "Processing the commit response";
        ProcessCommitResponseCommand process_response_command(
            extensions_monitor_);
        process_response_command.Execute(session);
        next_step = BUILD_AND_PROCESS_CONFLICT_SETS;
        break;
      }
      case BUILD_AND_PROCESS_CONFLICT_SETS: {
        LOG(INFO) << "Building and Processing Conflict Sets";
        BuildAndProcessConflictSetsCommand build_process_conflict_sets;
        build_process_conflict_sets.Execute(session);
        if (session->conflict_sets_built())
          next_step = SYNCER_END;
        else
          next_step = RESOLVE_CONFLICTS;
        break;
      }
      case RESOLVE_CONFLICTS: {
        LOG(INFO) << "Resolving Conflicts";

        // Trigger the pre_conflict_resolution_closure_, which is a testing
        // hook for the unit tests, if it is non-NULL.
        if (pre_conflict_resolution_closure_) {
          pre_conflict_resolution_closure_->Run();
        }

        ResolveConflictsCommand resolve_conflicts_command;
        resolve_conflicts_command.Execute(session);
        if (session->HasConflictingUpdates())
          next_step = APPLY_UPDATES_TO_RESOLVE_CONFLICTS;
        else
          next_step = SYNCER_END;
        break;
      }
      case APPLY_UPDATES_TO_RESOLVE_CONFLICTS: {
        LOG(INFO) << "Applying updates to resolve conflicts";
        ApplyUpdatesCommand apply_updates;
        int num_conflicting_updates = session->conflicting_update_count();
        apply_updates.Execute(session);
        int post_facto_conflicting_updates =
            session->conflicting_update_count();
        session->set_conflicts_resolved(session->conflicts_resolved() ||
            num_conflicting_updates > post_facto_conflicting_updates);
        if (session->conflicts_resolved())
          next_step = RESOLVE_CONFLICTS;
        else
          next_step = SYNCER_END;
        break;
      }
      case SYNCER_END: {
        LOG(INFO) << "Syncer End";
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
  // Copy any lingering useful state out of the session.
  silenced_until_ = session->silenced_until();
  return;
}

void Syncer::ProcessClientCommand(SyncerSession* session) {
  if (!session->update_response().has_client_command())
    return;
  const ClientCommand command = session->update_response().client_command();
  if (command_channel_)
    command_channel_->NotifyListeners(&command);

  // The server limits the number of items a client can commit in one batch.
  if (command.has_max_commit_batch_size())
    max_commit_batch_size_ = command.max_commit_batch_size();
}

void CopyServerFields(syncable::Entry* src, syncable::MutableEntry* dest) {
  dest->Put(SERVER_NON_UNIQUE_NAME, src->Get(SERVER_NON_UNIQUE_NAME));
  dest->Put(SERVER_PARENT_ID, src->Get(SERVER_PARENT_ID));
  dest->Put(SERVER_MTIME, src->Get(SERVER_MTIME));
  dest->Put(SERVER_CTIME, src->Get(SERVER_CTIME));
  dest->Put(SERVER_VERSION, src->Get(SERVER_VERSION));
  dest->Put(SERVER_IS_DIR, src->Get(SERVER_IS_DIR));
  dest->Put(SERVER_IS_DEL, src->Get(SERVER_IS_DEL));
  dest->Put(SERVER_IS_BOOKMARK_OBJECT, src->Get(SERVER_IS_BOOKMARK_OBJECT));
  dest->Put(IS_UNAPPLIED_UPDATE, src->Get(IS_UNAPPLIED_UPDATE));
  dest->Put(SERVER_BOOKMARK_URL, src->Get(SERVER_BOOKMARK_URL));
  dest->Put(SERVER_BOOKMARK_FAVICON, src->Get(SERVER_BOOKMARK_FAVICON));
  dest->Put(SERVER_POSITION_IN_PARENT, src->Get(SERVER_POSITION_IN_PARENT));
}

void ClearServerData(syncable::MutableEntry* entry) {
  entry->Put(SERVER_NON_UNIQUE_NAME, PSTR(""));
  entry->Put(SERVER_PARENT_ID, syncable::kNullId);
  entry->Put(SERVER_MTIME, 0);
  entry->Put(SERVER_CTIME, 0);
  entry->Put(SERVER_VERSION, 0);
  entry->Put(SERVER_IS_DIR, false);
  entry->Put(SERVER_IS_DEL, false);
  entry->Put(SERVER_IS_BOOKMARK_OBJECT, false);
  entry->Put(IS_UNAPPLIED_UPDATE, false);
  entry->Put(SERVER_BOOKMARK_URL, PSTR(""));
  entry->Put(SERVER_BOOKMARK_FAVICON, Blob());
  entry->Put(SERVER_POSITION_IN_PARENT, 0);
}

std::string SyncEntityDebugString(const sync_pb::SyncEntity& entry) {
  return StringPrintf("id: %s, parent_id: %s, "
                      "version: %"PRId64"d, "
                      "mtime: %" PRId64"d (client: %" PRId64"d), "
                      "ctime: %" PRId64"d (client: %" PRId64"d), "
                      "name: %s, sync_timestamp: %" PRId64"d, "
                      "%s ",
                      entry.id_string().c_str(),
                      entry.parent_id_string().c_str(),
                      entry.version(),
                      entry.mtime(), ServerTimeToClientTime(entry.mtime()),
                      entry.ctime(), ServerTimeToClientTime(entry.ctime()),
                      entry.name().c_str(), entry.sync_timestamp(),
                      entry.deleted() ? "deleted, ":"");
}

}  // namespace browser_sync
