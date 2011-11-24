// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_
#pragma once

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/engine/syncer_command.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/sessions/ordered_commit_set.h"
#include "chrome/browser/sync/sessions/sync_session.h"

using std::pair;
using std::vector;

namespace browser_sync {

class GetCommitIdsCommand : public SyncerCommand {
  friend class SyncerTest;

 public:
  explicit GetCommitIdsCommand(int commit_batch_size);
  virtual ~GetCommitIdsCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

  // Builds a vector of IDs that should be committed.
  void BuildCommitIds(const vector<int64>& unsynced_handles,
                      syncable::WriteTransaction* write_transaction,
                      const ModelSafeRoutingInfo& routes,
                      const syncable::ModelTypeSet& throttled_types);

  // TODO(chron): Remove writes from this iterator. As a warning, this
  // iterator causes writes to entries and so isn't a pure iterator.
  // It will do Put(IS_UNSYNCED). Refactor this out later.
  class CommitMetahandleIterator {
   public:
    // TODO(chron): Cache ValidateCommitEntry responses across iterators to save
    // UTF8 conversion and filename checking
    CommitMetahandleIterator(const vector<int64>& unsynced_handles,
                             syncable::WriteTransaction* write_transaction,
                             sessions::OrderedCommitSet* commit_set)
        : write_transaction_(write_transaction),
          handle_iterator_(unsynced_handles.begin()),
          unsynced_handles_end_(unsynced_handles.end()),
          commit_set_(commit_set) {

      // TODO(chron): Remove writes from this iterator.
      DCHECK(write_transaction_);

      if (Valid() && !ValidateMetahandleForCommit(*handle_iterator_))
        Increment();
    }
    ~CommitMetahandleIterator() {}

    int64 Current() const {
      DCHECK(Valid());
      return *handle_iterator_;
    }

    bool Increment() {
      if (!Valid())
        return false;

      for (++handle_iterator_;
           handle_iterator_ != unsynced_handles_end_;
           ++handle_iterator_) {
        if (ValidateMetahandleForCommit(*handle_iterator_))
          return true;
      }

      return false;
    }

    bool Valid() const {
      return !(handle_iterator_ == unsynced_handles_end_);
    }

    private:
     bool ValidateMetahandleForCommit(int64 metahandle) {
       if (commit_set_->HaveCommitItem(metahandle))
          return false;

       // We should really not WRITE in this iterator, but we can fix that
       // later. We should move that somewhere else later.
       syncable::MutableEntry entry(write_transaction_,
           syncable::GET_BY_HANDLE, metahandle);
       VerifyCommitResult verify_result =
           SyncerUtil::ValidateCommitEntry(&entry);
       if (verify_result == VERIFY_UNSYNCABLE) {
         // Drop unsyncable entries.
         entry.Put(syncable::IS_UNSYNCED, false);
       }
       return verify_result == VERIFY_OK;
     }

     syncable::WriteTransaction* const write_transaction_;
     vector<int64>::const_iterator handle_iterator_;
     vector<int64>::const_iterator unsynced_handles_end_;
     sessions::OrderedCommitSet* commit_set_;

     DISALLOW_COPY_AND_ASSIGN(CommitMetahandleIterator);
  };

 private:
  // Removes all entries not ready for commit from |unsynced_handles|.
  // An entry is considered unready for commit if:
  // 1. It's in conflict or requires (re)encryption. Any datatype requiring
  //     encryption while the cryptographer is missing a passphrase is
  //     considered unready for commit.
  // 2. Its type is currently throttled.
  void FilterUnreadyEntries(
      syncable::BaseTransaction* trans,
      const syncable::ModelTypeSet& throttled_types,
      syncable::Directory::UnsyncedMetaHandles* unsynced_handles);

  void AddUncommittedParentsAndTheirPredecessors(
      syncable::BaseTransaction* trans,
      syncable::Id parent_id,
      const ModelSafeRoutingInfo& routes,
      const syncable::ModelTypeSet& throttled_types);

  // OrderedCommitSet helpers for adding predecessors in order.
  // TODO(ncarter): Refactor these so that the |result| parameter goes away,
  // and AddItem doesn't need to consider two OrderedCommitSets.
  bool AddItem(syncable::Entry* item,
               const syncable::ModelTypeSet& throttled_types,
               sessions::OrderedCommitSet* result);
  bool AddItemThenPredecessors(syncable::BaseTransaction* trans,
                               const syncable::ModelTypeSet& throttled_types,
                               syncable::Entry* item,
                               syncable::IndexedBitField inclusion_filter,
                               sessions::OrderedCommitSet* result);
  void AddPredecessorsThenItem(syncable::BaseTransaction* trans,
                               const syncable::ModelTypeSet& throttled_types,
                               syncable::Entry* item,
                               syncable::IndexedBitField inclusion_filter,
                               const ModelSafeRoutingInfo& routes);

  bool IsCommitBatchFull();

  void AddCreatesAndMoves(const vector<int64>& unsynced_handles,
                          syncable::WriteTransaction* write_transaction,
                          const ModelSafeRoutingInfo& routes,
                          const syncable::ModelTypeSet& throttled_types);

  void AddDeletes(const vector<int64>& unsynced_handles,
                  syncable::WriteTransaction* write_transaction);

  scoped_ptr<sessions::OrderedCommitSet> ordered_commit_set_;

  int requested_commit_batch_size_;
  bool passphrase_missing_;
  syncable::ModelTypeSet encrypted_types_;

  DISALLOW_COPY_AND_ASSIGN(GetCommitIdsCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_
