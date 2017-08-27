// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/engine/model_type_processor.h"

namespace sync_bookmarks {

class BookmarkModelTypeProcessor : public syncer::ModelTypeProcessor {
 public:
  BookmarkModelTypeProcessor();
  ~BookmarkModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<syncer::CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       const GetLocalChangesCallback& callback) override;
  void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const syncer::CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        const syncer::UpdateResponseDataList& updates) override;

  base::WeakPtr<syncer::ModelTypeProcessor> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<syncer::CommitQueue> worker_;

  base::WeakPtrFactory<BookmarkModelTypeProcessor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTypeProcessor);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_
