// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_

#include <utility>
#include <vector>

#include "chrome/browser/sync/engine/syncer_command.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/util/sync_types.h"

using std::pair;
using std::vector;

namespace browser_sync {

class GetCommitIdsCommand : public SyncerCommand {
  friend class SyncerTest;

 public:
  explicit GetCommitIdsCommand(int commit_batch_size);
  virtual ~GetCommitIdsCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session);

  // Builds a vector of IDs that should be committed.
  void BuildCommitIds(const vector<int64>& unsynced_handles,
                      syncable::WriteTransaction* write_transaction,
                      const ModelSafeRoutingInfo& routing_info);

  // These classes are public for testing.
  // TODO(ncarter): This code is more generic than just Commit and can
  // be reused elsewhere (e.g. ChangeReorderBuffer do similar things).  Merge
  // all these implementations.
  // TODO(tim): Move this to its own file.  Also, SyncSession should now
  // hold an OrderedCommitSet instead of just the vector<syncable::Id>.  Will
  // do this in upcoming CL.
  class OrderedCommitSet {
   public:
    // A list of indices into the full list of commit ids such that:
    // 1 - each element is an index belonging to a particular ModelSafeGroup.
    // 2 - the vector is in sorted (smallest to largest) order.
    // 3 - each element is a valid index for GetCommitItemAt.
    // See GetCommitIdProjection for usage.
    typedef std::vector<size_t> Projection;

    // TODO(chron): Reserve space according to batch size?
    OrderedCommitSet() {}
    ~OrderedCommitSet() {}

    bool HaveCommitItem(const int64 metahandle) const {
      return inserted_metahandles_.count(metahandle) > 0;
    }

    void AddCommitItem(const int64 metahandle, const syncable::Id& commit_id,
                       ModelSafeGroup group) {
      if (!HaveCommitItem(metahandle)) {
        inserted_metahandles_.insert(metahandle);
        metahandle_order_.push_back(metahandle);
        commit_ids_.push_back(commit_id);
        projections_[group].push_back(commit_ids_.size() - 1);
        groups_.push_back(group);
      }
    }

    const vector<syncable::Id>& GetAllCommitIds() const {
      return commit_ids_;
    }

    // Return the Id at index |position| in this OrderedCommitSet.  Note that
    // the index uniquely identifies the same logical item in each of:
    // 1) this OrderedCommitSet
    // 2) the CommitRequest sent to the server
    // 3) the list of EntryResponse objects in the CommitResponse.
    // These together allow re-association of the pre-commit Id with the
    // actual committed entry.
    const syncable::Id& GetCommitIdAt(const size_t position) const {
      return commit_ids_[position];
    }

    // Get the projection of commit ids onto the space of commit ids
    // belonging to |group|.  This is useful when you need to process a commit
    // response one ModelSafeGroup at a time. See GetCommitIdAt for how the
    // indices contained in the returned Projection can be used.
    const Projection& GetCommitIdProjection(ModelSafeGroup group) {
      return projections_[group];
    }

    int Size() const {
      return commit_ids_.size();
    }

    void AppendReverse(const OrderedCommitSet& other) {
      for (int i = other.Size() - 1; i >= 0; i--) {
        CommitItem item = other.GetCommitItemAt(i);
        AddCommitItem(item.meta, item.id, item.group);
      }
    }

    void Truncate(size_t max_size) {
      if (max_size < metahandle_order_.size()) {
        for (size_t i = max_size; i < metahandle_order_.size(); ++i) {
          inserted_metahandles_.erase(metahandle_order_[i]);
        }

        // Some projections may refer to indices that are getting chopped.
        // Since projections are in increasing order, it's easy to fix. Except
        // that you can't erase(..) using a reverse_iterator, so we use binary
        // search to find the chop point.
        Projections::iterator it = projections_.begin();
        for (; it != projections_.end(); ++it) {
          // For each projection, chop off any indices larger than or equal to
          // max_size by looking for max_size using binary search.
          Projection& p = it->second;
          Projection::iterator element = std::lower_bound(p.begin(), p.end(),
                                                          max_size);
          if (element != p.end())
            p.erase(element, p.end());
        }
        commit_ids_.resize(max_size);
        metahandle_order_.resize(max_size);
        groups_.resize(max_size);
      }
    }

   private:
    // A set of CommitIdProjections associated with particular ModelSafeGroups.
    typedef std::map<ModelSafeGroup, Projection> Projections;

    // Helper container for return value of GetCommitItemAt.
    struct CommitItem {
      int64 meta;
      syncable::Id id;
      ModelSafeGroup group;
    };

    CommitItem GetCommitItemAt(const int position) const {
      DCHECK(position < Size());
      CommitItem return_item = {metahandle_order_[position],
                                commit_ids_[position],
                                groups_[position]};
      return return_item;
    }

    // These lists are different views of the same items; e.g they are
    // isomorphic.
    syncable::MetahandleSet inserted_metahandles_;
    vector<syncable::Id> commit_ids_;
    vector<int64> metahandle_order_;
    Projections projections_;

    // We need this because of operations like AppendReverse that take ids from
    // one OrderedCommitSet and insert into another -- we need to know the
    // group for each ID so that the insertion can update the appropriate
    // projection.  We could store it in commit_ids_, but sometimes we want
    // to just return the vector of Ids, so this is more straightforward
    // and shouldn't take up too much extra space since commit lists are small.
    std::vector<ModelSafeGroup> groups_;

    DISALLOW_COPY_AND_ASSIGN(OrderedCommitSet);
  };

  // TODO(chron): Remove writes from this iterator. As a warning, this
  // iterator causes writes to entries and so isn't a pure iterator.
  // It will do Put(IS_UNSYNCED). Refactor this out later.
  class CommitMetahandleIterator {
   public:
    // TODO(chron): Cache ValidateCommitEntry responses across iterators to save
    // UTF8 conversion and filename checking
    CommitMetahandleIterator(const vector<int64>& unsynced_handles,
                             syncable::WriteTransaction* write_transaction,
                             OrderedCommitSet* commit_set)
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
     OrderedCommitSet* commit_set_;

     DISALLOW_COPY_AND_ASSIGN(CommitMetahandleIterator);
  };

 private:
  void AddUncommittedParentsAndTheirPredecessors(
      syncable::BaseTransaction* trans,
      syncable::Id parent_id,
      const ModelSafeRoutingInfo& routing_info);

  // OrderedCommitSet helpers for adding predecessors in order.
  // TODO(ncarter): Refactor these so that the |result| parameter goes away,
  // and AddItem doesn't need to consider two OrderedCommitSets.
  bool AddItem(syncable::Entry* item, OrderedCommitSet* result,
               const ModelSafeRoutingInfo& routing_info);
  bool AddItemThenPredecessors(syncable::BaseTransaction* trans,
                               syncable::Entry* item,
                               syncable::IndexedBitField inclusion_filter,
                               OrderedCommitSet* result,
                               const ModelSafeRoutingInfo& routing_info);
  void AddPredecessorsThenItem(syncable::BaseTransaction* trans,
                               syncable::Entry* item,
                               syncable::IndexedBitField inclusion_filter,
                               const ModelSafeRoutingInfo& routing_info);

  bool IsCommitBatchFull();

  void AddCreatesAndMoves(const vector<int64>& unsynced_handles,
                          syncable::WriteTransaction* write_transaction,
                          const ModelSafeRoutingInfo& routing_info);

  void AddDeletes(const vector<int64>& unsynced_handles,
                  syncable::WriteTransaction* write_transaction,
                  const ModelSafeRoutingInfo& routing_info);

  OrderedCommitSet ordered_commit_set_;

  int requested_commit_batch_size_;

  DISALLOW_COPY_AND_ASSIGN(GetCommitIdsCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_GET_COMMIT_IDS_COMMAND_H_
