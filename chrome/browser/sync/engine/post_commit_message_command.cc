// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/post_commit_message_command.h"

#include <vector>

#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/sync_types.h"

using std::vector;

namespace browser_sync {

PostCommitMessageCommand::PostCommitMessageCommand() {}
PostCommitMessageCommand::~PostCommitMessageCommand() {}

void PostCommitMessageCommand::ExecuteImpl(SyncerSession *session) {
  if (session->commit_ids_empty())
    return;  // nothing to commit
  ClientToServerResponse response;
  syncable::ScopedDirLookup dir(session->dirman(), session->account_name());
  if (!dir.good())
    return;
  if (!SyncerProtoUtil::PostClientToServerMessage(session->commit_message(),
                                                  &response, session)) {
    // None of our changes got through, let's clear sync flags and wait for
    // another list update.
    SyncerStatus status(session);
    status.increment_consecutive_problem_commits();
    status.increment_consecutive_errors();
    syncable::WriteTransaction trans(dir, syncable::SYNCER, __FILE__, __LINE__);
    // TODO(sync): why set this flag, it seems like a bad side-effect?
    const vector<syncable::Id>& commit_ids = session->commit_ids();
    for (size_t i = 0; i < commit_ids.size(); i++) {
      syncable::MutableEntry entry(&trans, syncable::GET_BY_ID, commit_ids[i]);
      entry.Put(syncable::SYNCING, false);
    }
    return;
  } else {
    session->set_item_committed();
  }
  session->set_commit_response(response);
}

}  // namespace browser_sync
