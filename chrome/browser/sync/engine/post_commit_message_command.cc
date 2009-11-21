// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/post_commit_message_command.h"

#include <vector>

#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/sync_types.h"

using std::vector;

namespace browser_sync {

PostCommitMessageCommand::PostCommitMessageCommand() {}
PostCommitMessageCommand::~PostCommitMessageCommand() {}

void PostCommitMessageCommand::ExecuteImpl(sessions::SyncSession* session) {
  if (session->status_controller()->commit_ids().empty())
    return;  // Nothing to commit.
  ClientToServerResponse response;
  syncable::ScopedDirLookup dir(session->context()->directory_manager(),
                                session->context()->account_name());
  if (!dir.good())
    return;
  sessions::StatusController* status = session->status_controller();
  if (!SyncerProtoUtil::PostClientToServerMessage(
      status->mutable_commit_message(), &response, session)) {
    // None of our changes got through, let's clear sync flags and wait for
    // another list update.
    status->increment_num_consecutive_problem_commits();
    status->increment_num_consecutive_errors();
    syncable::WriteTransaction trans(dir, syncable::SYNCER, __FILE__, __LINE__);
    // TODO(sync): why set this flag, it seems like a bad side-effect?
    const vector<syncable::Id>& commit_ids = status->commit_ids();
    for (size_t i = 0; i < commit_ids.size(); i++) {
      syncable::MutableEntry entry(&trans, syncable::GET_BY_ID, commit_ids[i]);
      entry.Put(syncable::SYNCING, false);
    }
    return;
  } else {
    status->set_items_committed(true);
  }
  status->mutable_commit_response()->CopyFrom(response);
}

}  // namespace browser_sync
