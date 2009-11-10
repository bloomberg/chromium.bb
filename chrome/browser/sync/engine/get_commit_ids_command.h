// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_

#include <utility>
#include <vector>

#include "chrome/browser/sync/engine/syncer_command.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/util/sync_types.h"

using std::pair;
using std::vector;

namespace browser_sync {

class GetCommitIdsCommand : public SyncerCommand {
  friend class SyncerTest;

 public:
  explicit GetCommitIdsCommand(int commit_batch_size);
  virtual ~GetCommitIdsCommand();

  virtual void ExecuteImpl(SyncerSession* session);

  // Returns a vector of IDs that should be committed.
  void BuildCommitIds(SyncerSession *session);

  // These classes are public for testing.
  // TODO(ncarter): This code is more generic than just Commit and can
  // be reused elsewhere (e.g. ChangeReorderBuffer do similar things).  Merge
  // all these implementations.
  class OrderedCommitSet {
   public:
    // TODO(chron): Reserve space according to batch size?
    OrderedCommitSet() {}
    ~OrderedCommitSet() {}

    bool HaveCommitItem(const int64 metahandle) const {
      return inserted_metahandles_.count(metahandle) > 0;
    }

    void AddCommitItem(const int64 metahandle, const syncable::Id& commit_id) {
      if (!HaveCommitItem(metahandle)) {
        inserted_metahandles_.insert(metahandle);
        metahandle_order_.push_back(metahandle);
        commit_ids_.push_back(commit_id);
      }
    }

    const vector<syncable::Id>& GetCommitIds() const {
      return commit_ids_;
    }

    pair<int64, syncable::Id> GetCommitItemAt(const int position) const {
      DCHECK(position < Size());
      return pair<int64, syncable::Id> (
          metahandle_order_[position], commit_ids_[position]);
    }

    int Size() const {
      return commit_ids_.size();
    }

    void AppendReverse(const OrderedCommitSet& other) {
      for (int i = other.Size() - 1; i >= 0; i--) {
        pair<int64, syncable::Id> item = other.GetCommitItemAt(i);
        AddCommitItem(item.first, item.second);
      }
    }

    void Truncate(size_t max_size) {
      if (max_size < metahandle_order_.size()) {
        for (size_t i = max_size; i < metahandle_order_.size(); ++i) {
          inserted_metahandles_.erase(metahandle_order_[i]);
        }
        commit_ids_.resize(max_size);
        metahandle_order_.resize(max_size);
      }
    }

   private:
    // These three lists are different views of the same data; e.g they are
    // isomorphic.
    syncable::MetahandleSet inserted_metahandles_;
    vector<syncable::Id> commit_ids_;
    vector<int64> metahandle_order_;

    DISALLOW_COPY_AND_ASSIGN(OrderedCommitSet);
  };


  // TODO(chron): Remove writes from this iterator. As a warning, this
  // iterator causes writes to entries and so isn't a pure iterator.
  // It will do Put(IS_UNSYNCED) as well as add things to the blocked
  // session list. Refactor this out later.
  class CommitMetahandleIterator {
   public:
    // TODO(chron): Cache ValidateCommitEntry responses across iterators to save
    // UTF8 conversion and filename checking
    CommitMetahandleIterator(SyncerSession* session,
                             OrderedCommitSet* commit_set)
        : session_(session),
          commit_set_(commit_set) {
      handle_iterator_ = session->unsynced_handles().begin();

      // TODO(chron): Remove writes from this iterator.
      DCHECK(session->has_open_write_transaction());

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
           handle_iterator_ != session_->unsynced_handles().end();
           ++handle_iterator_) {
        if (ValidateMetahandleForCommit(*handle_iterator_))
          return true;
      }

      return false;
    }

    bool Valid() const {
      return !(handle_iterator_ == session_->unsynced_handles().end());
    }

    private:
     bool ValidateMetahandleForCommit(int64 metahandle) {
       if (commit_set_->HaveCommitItem(metahandle))
          return false;

       // We should really not WRITE in this iterator, but we can fix that
       // later. ValidateCommitEntry writes to the DB, and we add the blocked
       // items. We should move that somewhere else later.
       syncable::MutableEntry entry(session_->write_transaction(),
           syncable::GET_BY_HANDLE, metahandle);
       VerifyCommitResult verify_result =
           SyncerUtil::ValidateCommitEntry(&entry);
       if (verify_result == VERIFY_UNSYNCABLE) {
         // Drop unsyncable entries.
         entry.Put(syncable::IS_UNSYNCED, false);
       }
       return verify_result == VERIFY_OK;
     }

     SyncerSession* session_;
     vector<int64>::const_iterator handle_iterator_;
     OrderedCommitSet* commit_set_;

     DISALLOW_COPY_AND_ASSIGN(CommitMetahandleIterator);
  };

 private:
  void AddUncommittedParentsAndTheirPredecessors(
      syncable::BaseTransaction* trans,
      syncable::Id parent_id);

  // OrderedCommitSet helpers for adding predecessors in order.
  // TODO(ncarter): Refactor these so that the |result| parameter goes away,
  // and AddItem doesn't need to consider two OrderedCommitSets.
  bool AddItem(syncable::Entry* item, OrderedCommitSet* result);
  bool AddItemThenPredecessors(syncable::BaseTransaction* trans,
                               syncable::Entry* item,
                               syncable::IndexedBitField inclusion_filter,
                               OrderedCommitSet* result);
  void AddPredecessorsThenItem(syncable::BaseTransaction* trans,
                               syncable::Entry* item,
                               syncable::IndexedBitField inclusion_filter);

  bool IsCommitBatchFull();

  void AddCreatesAndMoves(SyncerSession* session);

  void AddDeletes(SyncerSession* session);

  OrderedCommitSet ordered_commit_set_;

  int requested_commit_batch_size_;

  DISALLOW_COPY_AND_ASSIGN(GetCommitIdsCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_
