// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include "components/sync/base/model_type.h"
#include "components/sync/engine/commit_queue.h"

namespace sync_bookmarks {

BookmarkModelTypeProcessor::BookmarkModelTypeProcessor() = default;

BookmarkModelTypeProcessor::~BookmarkModelTypeProcessor() = default;

void BookmarkModelTypeProcessor::ConnectSync(
    std::unique_ptr<syncer::CommitQueue> worker) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::DisconnectSync() {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    const GetLocalChangesCallback& callback) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const syncer::CommitResponseDataList& response_list) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    const syncer::UpdateResponseDataList& updates) {
  NOTIMPLEMENTED();
}

}  // namespace sync_bookmarks
