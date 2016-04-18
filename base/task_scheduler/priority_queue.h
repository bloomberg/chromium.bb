// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_PRIORITY_QUEUE_H_
#define BASE_TASK_SCHEDULER_PRIORITY_QUEUE_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/sequence_sort_key.h"
#include "base/threading/non_thread_safe.h"

namespace base {
namespace internal {

// A PriorityQueue holds Sequences of Tasks. This class is thread-safe.
class BASE_EXPORT PriorityQueue {
 public:
  // An immutable struct combining a Sequence and the sort key that determines
  // its position in a PriorityQueue.
  struct BASE_EXPORT SequenceAndSortKey {
    // Constructs a null SequenceAndSortKey.
    SequenceAndSortKey();

    // Constructs a SequenceAndSortKey with the given |sequence| and |sort_key|.
    SequenceAndSortKey(scoped_refptr<Sequence> sequence,
                       const SequenceSortKey& sort_key);

    SequenceAndSortKey(const SequenceAndSortKey& other);

    ~SequenceAndSortKey();

    // Returns true if this is a null SequenceAndSortKey.
    bool is_null() const { return !sequence; }

    const scoped_refptr<Sequence> sequence;
    const SequenceSortKey sort_key;
  };

  // A Transaction can perform multiple operations atomically on a
  // PriorityQueue. While a Transaction is alive, it is guaranteed that nothing
  // else will access the PriorityQueue.
  //
  // A WorkerThread needs to be able to Peek sequences from both its
  // PriorityQueues (single-threaded and shared) and then Pop the sequence with
  // the highest priority. If the Peek and the Pop are done through the same
  // Transaction, it is guaranteed that the PriorityQueue hasn't changed between
  // the 2 operations.
  class BASE_EXPORT Transaction : public NonThreadSafe {
   public:
    ~Transaction();

    // Inserts |sequence_and_sort_key| in the PriorityQueue.
    void Push(std::unique_ptr<SequenceAndSortKey> sequence_and_sort_key);

    // Returns the SequenceAndSortKey with the highest priority or a null
    // SequenceAndSortKey if the PriorityQueue is empty. The reference becomes
    // invalid the next time that a Sequence is popped from the PriorityQueue.
    const SequenceAndSortKey& Peek() const;

    // Removes the SequenceAndSortKey with the highest priority from the
    // PriorityQueue. Cannot be called on an empty PriorityQueue.
    void Pop();

   private:
    friend class PriorityQueue;

    explicit Transaction(PriorityQueue* outer_queue);

    // Holds the lock of |outer_queue_| for the lifetime of this Transaction.
    AutoSchedulerLock auto_lock_;

    PriorityQueue* const outer_queue_;

    DISALLOW_COPY_AND_ASSIGN(Transaction);
  };

  PriorityQueue();

  // |predecessor_priority_queue| is a PriorityQueue for which a thread is
  // allowed to have an active Transaction when it creates a Transaction for
  // this PriorityQueue.
  PriorityQueue(const PriorityQueue* predecessor_priority_queue);

  ~PriorityQueue();

  // Begins a Transaction. This method cannot be called on a thread which has an
  // active Transaction unless the last Transaction created on the thread was
  // for the allowed predecessor specified in the constructor of this
  // PriorityQueue.
  std::unique_ptr<Transaction> BeginTransaction();

  const SchedulerLock* container_lock() const { return &container_lock_; }

 private:
  struct SequenceAndSortKeyComparator {
    bool operator()(const std::unique_ptr<SequenceAndSortKey>& left,
                    const std::unique_ptr<SequenceAndSortKey>& right) const {
      return left->sort_key < right->sort_key;
    }
  };
  using ContainerType =
      std::priority_queue<std::unique_ptr<SequenceAndSortKey>,
                          std::vector<std::unique_ptr<SequenceAndSortKey>>,
                          SequenceAndSortKeyComparator>;

  // Synchronizes access to |container_|.
  SchedulerLock container_lock_;

  ContainerType container_;

  // A null SequenceAndSortKey returned by Peek() when the PriorityQueue is
  // empty.
  const SequenceAndSortKey empty_sequence_and_sort_key_;

  DISALLOW_COPY_AND_ASSIGN(PriorityQueue);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_PRIORITY_QUEUE_H_
