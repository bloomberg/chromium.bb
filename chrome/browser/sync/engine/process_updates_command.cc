// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/process_updates_command.h"

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

using std::vector;

namespace browser_sync {

ProcessUpdatesCommand::ProcessUpdatesCommand() {}
ProcessUpdatesCommand::~ProcessUpdatesCommand() {}

void ProcessUpdatesCommand::ModelChangingExecuteImpl(SyncerSession* session) {
  syncable::ScopedDirLookup dir(session->dirman(), session->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }
  SyncerStatus status(session);

  const GetUpdatesResponse updates = session->update_response().get_updates();
  const int update_count = updates.entries_size();

  LOG(INFO) << "Get updates from ts " << dir->last_sync_timestamp() <<
    " returned " << update_count << " updates.";

  if (updates.has_changes_remaining()) {
    int64 changes_left = updates.changes_remaining();
    LOG(INFO) << "Changes remaining:" << changes_left;
    status.set_num_server_changes_remaining(changes_left);
  }

  int64 new_timestamp = 0;
  if (updates.has_new_timestamp()) {
    new_timestamp = updates.new_timestamp();
    LOG(INFO) << "Get Updates got new timestamp: " << new_timestamp;
    if (0 == update_count) {
      if (new_timestamp > dir->last_sync_timestamp()) {
        dir->set_last_sync_timestamp(new_timestamp);
        session->set_timestamp_dirty();
      }
      return;
    }
  }

  // If we have updates that are ALL supposed to be skipped, we don't want to
  // get them again.  In fact, the account's final updates are all supposed to
  // be skipped and we DON'T step past them, we will sync forever.
  int64 latest_skip_timestamp = 0;
  bool any_non_skip_results = false;
  vector<VerifiedUpdate>::iterator it;
  for (it = session->VerifiedUpdatesBegin();
       it < session->VerifiedUpdatesEnd();
       ++it) {
    const sync_pb::SyncEntity update = it->second;

    any_non_skip_results = (it->first != VERIFY_SKIP);
    if (!any_non_skip_results) {
      // ALL updates were to be skipped, including this one.
      if (update.sync_timestamp() > latest_skip_timestamp) {
        latest_skip_timestamp = update.sync_timestamp();
      }
    } else {
      latest_skip_timestamp = 0;
    }

    if (it->first != VERIFY_SUCCESS && it->first != VERIFY_UNDELETE)
      continue;
    switch (ProcessUpdate(dir, update)) {
      case SUCCESS_PROCESSED:
      case SUCCESS_STORED:
        // We can update the timestamp because we store the update even if we
        // can't apply it now.
        if (update.sync_timestamp() > new_timestamp)
          new_timestamp = update.sync_timestamp();
        break;
      default:
        NOTREACHED();
        break;
    }

  }

  if (latest_skip_timestamp > new_timestamp)
    new_timestamp = latest_skip_timestamp;

  if (new_timestamp > dir->last_sync_timestamp()) {
    dir->set_last_sync_timestamp(new_timestamp);
    session->set_timestamp_dirty();
  }

  status.zero_consecutive_problem_get_updates();
  status.zero_consecutive_errors();
  status.set_current_sync_timestamp(dir->last_sync_timestamp());
  status.set_syncing(true);
  return;
}

namespace {
// Returns true if the entry is still ok to process.
bool ReverifyEntry(syncable::WriteTransaction* trans, const SyncEntity& entry,
                   syncable::MutableEntry* same_id) {

  const bool deleted = entry.has_deleted() && entry.deleted();
  const bool is_directory = entry.IsFolder();
  const bool is_bookmark = entry.has_bookmarkdata();

  return VERIFY_SUCCESS == SyncerUtil::VerifyUpdateConsistency(trans,
                                                               entry,
                                                               same_id,
                                                               deleted,
                                                               is_directory,
                                                               is_bookmark);
}
}  // namespace

// TODO(sync): Refactor this code.
// Process a single update. Will avoid touching global state.
ServerUpdateProcessingResult ProcessUpdatesCommand::ProcessUpdate(
    const syncable::ScopedDirLookup& dir, const sync_pb::SyncEntity& pb_entry) {

  const SyncEntity& entry = *static_cast<const SyncEntity*>(&pb_entry);
  using namespace syncable;
  syncable::Id id = entry.id();
  const std::string name =
      SyncerProtoUtil::NameFromSyncEntity(entry);

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
      // Previously this was a big issue but at this point we don't really care
      // that much if things don't match up exactly.
      LOG_IF(ERROR, !SyncerUtil::ServerAndLocalEntriesMatch(&update_entry))
          << update_entry;
  }
  return SUCCESS_PROCESSED;
}

}  // namespace browser_sync
