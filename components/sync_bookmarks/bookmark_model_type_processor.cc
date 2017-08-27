// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include <utility>

#include "base/callback.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/commit_queue.h"

namespace sync_bookmarks {

BookmarkModelTypeProcessor::BookmarkModelTypeProcessor()
    : weak_ptr_factory_(this) {}

BookmarkModelTypeProcessor::~BookmarkModelTypeProcessor() = default;

void BookmarkModelTypeProcessor::ConnectSync(
    std::unique_ptr<syncer::CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!worker_);
  worker_ = std::move(worker);
}

void BookmarkModelTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    const GetLocalChangesCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::CommitRequestDataList local_changes;
  callback.Run(std::move(local_changes));
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const syncer::CommitResponseDataList& response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void BookmarkModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    const syncer::UpdateResponseDataList& updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

base::WeakPtr<syncer::ModelTypeProcessor>
BookmarkModelTypeProcessor::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace sync_bookmarks
