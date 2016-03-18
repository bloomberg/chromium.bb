// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/priority_queue.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/task_scheduler/test_utils.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class PriorityQueueCallbackMock {
 public:
  PriorityQueueCallbackMock() = default;
  MOCK_METHOD0(SequenceInsertedInPriorityQueue, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(PriorityQueueCallbackMock);
};

class ThreadBeginningTransaction : public SimpleThread {
 public:
  explicit ThreadBeginningTransaction(PriorityQueue* priority_queue)
      : SimpleThread("ThreadBeginningTransaction"),
        priority_queue_(priority_queue),
        transaction_began_(true, false) {}

  // SimpleThread:
  void Run() override {
    scoped_ptr<PriorityQueue::Transaction> transaction =
        priority_queue_->BeginTransaction();
    transaction_began_.Signal();
  }

  void ExpectTransactionDoesNotBegin() {
    // After a few milliseconds, the call to BeginTransaction() should not have
    // returned.
    EXPECT_FALSE(
        transaction_began_.TimedWait(TimeDelta::FromMilliseconds(250)));
  }

 private:
  PriorityQueue* const priority_queue_;
  WaitableEvent transaction_began_;

  DISALLOW_COPY_AND_ASSIGN(ThreadBeginningTransaction);
};

void ExpectSequenceAndSortKeyEq(
    const PriorityQueue::SequenceAndSortKey& expected,
    const PriorityQueue::SequenceAndSortKey& actual) {
  EXPECT_EQ(expected.sequence, actual.sequence);
  EXPECT_EQ(expected.sort_key.priority, actual.sort_key.priority);
  EXPECT_EQ(expected.sort_key.next_task_sequenced_time,
            actual.sort_key.next_task_sequenced_time);
}

#define EXPECT_SEQUENCE_AND_SORT_KEY_EQ(expected, actual) \
  do {                                                    \
    SCOPED_TRACE("");                                     \
    ExpectSequenceAndSortKeyEq(expected, actual);         \
  } while (false)

}  // namespace

TEST(TaskSchedulerPriorityQueueTest, PushPopPeek) {
  // Create test sequences.
  scoped_refptr<Sequence> sequence_a(new Sequence);
  sequence_a->PushTask(make_scoped_ptr(
      new Task(FROM_HERE, Closure(),
               TaskTraits().WithPriority(TaskPriority::USER_VISIBLE))));
  SequenceSortKey sort_key_a = sequence_a->GetSortKey();

  scoped_refptr<Sequence> sequence_b(new Sequence);
  sequence_b->PushTask(make_scoped_ptr(
      new Task(FROM_HERE, Closure(),
               TaskTraits().WithPriority(TaskPriority::USER_BLOCKING))));
  SequenceSortKey sort_key_b = sequence_b->GetSortKey();

  scoped_refptr<Sequence> sequence_c(new Sequence);
  sequence_c->PushTask(make_scoped_ptr(
      new Task(FROM_HERE, Closure(),
               TaskTraits().WithPriority(TaskPriority::USER_BLOCKING))));
  SequenceSortKey sort_key_c = sequence_c->GetSortKey();

  scoped_refptr<Sequence> sequence_d(new Sequence);
  sequence_d->PushTask(make_scoped_ptr(
      new Task(FROM_HERE, Closure(),
               TaskTraits().WithPriority(TaskPriority::BACKGROUND))));
  SequenceSortKey sort_key_d = sequence_d->GetSortKey();

  // Create a PriorityQueue and a Transaction.
  testing::StrictMock<PriorityQueueCallbackMock> mock;
  PriorityQueue pq(
      Bind(&PriorityQueueCallbackMock::SequenceInsertedInPriorityQueue,
           Unretained(&mock)));
  scoped_ptr<PriorityQueue::Transaction> transaction(pq.BeginTransaction());
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(PriorityQueue::SequenceAndSortKey(),
                                  transaction->Peek());

  // Push |sequence_a| in the PriorityQueue. It becomes the sequence with the
  // highest priority.
  transaction->Push(make_scoped_ptr(
      new PriorityQueue::SequenceAndSortKey(sequence_a, sort_key_a)));
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(
      PriorityQueue::SequenceAndSortKey(sequence_a, sort_key_a),
      transaction->Peek());

  // Push |sequence_b| in the PriorityQueue. It becomes the sequence with the
  // highest priority.
  transaction->Push(make_scoped_ptr(
      new PriorityQueue::SequenceAndSortKey(sequence_b, sort_key_b)));
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(
      PriorityQueue::SequenceAndSortKey(sequence_b, sort_key_b),
      transaction->Peek());

  // Push |sequence_c| in the PriorityQueue. |sequence_b| is still the sequence
  // with the highest priority.
  transaction->Push(make_scoped_ptr(
      new PriorityQueue::SequenceAndSortKey(sequence_c, sort_key_c)));
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(
      PriorityQueue::SequenceAndSortKey(sequence_b, sort_key_b),
      transaction->Peek());

  // Push |sequence_d| in the PriorityQueue. |sequence_b| is still the sequence
  // with the highest priority.
  transaction->Push(make_scoped_ptr(
      new PriorityQueue::SequenceAndSortKey(sequence_d, sort_key_d)));
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(
      PriorityQueue::SequenceAndSortKey(sequence_b, sort_key_b),
      transaction->Peek());

  // Pop |sequence_b| from the PriorityQueue. |sequence_c| becomes the sequence
  // with the highest priority.
  transaction->Pop();
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(
      PriorityQueue::SequenceAndSortKey(sequence_c, sort_key_c),
      transaction->Peek());

  // Pop |sequence_c| from the PriorityQueue. |sequence_a| becomes the sequence
  // with the highest priority.
  transaction->Pop();
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(
      PriorityQueue::SequenceAndSortKey(sequence_a, sort_key_a),
      transaction->Peek());

  // Pop |sequence_a| from the PriorityQueue. |sequence_d| becomes the sequence
  // with the highest priority.
  transaction->Pop();
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(
      PriorityQueue::SequenceAndSortKey(sequence_d, sort_key_d),
      transaction->Peek());

  // Pop |sequence_d| from the PriorityQueue. It is now empty.
  transaction->Pop();
  EXPECT_SEQUENCE_AND_SORT_KEY_EQ(PriorityQueue::SequenceAndSortKey(),
                                  transaction->Peek());

  // Expect 4 calls to mock.SequenceInsertedInPriorityQueue() when the
  // Transaction is destroyed.
  EXPECT_CALL(mock, SequenceInsertedInPriorityQueue()).Times(4);
  transaction.reset();
}

// Check that creating Transactions on the same thread for 2 unrelated
// PriorityQueues causes a crash.
TEST(TaskSchedulerPriorityQueueTest, IllegalTwoTransactionsSameThread) {
  PriorityQueue pq_a(Bind(&DoNothing));
  PriorityQueue pq_b(Bind(&DoNothing));

  EXPECT_DCHECK_DEATH(
      {
        scoped_ptr<PriorityQueue::Transaction> transaction_a =
            pq_a.BeginTransaction();
        scoped_ptr<PriorityQueue::Transaction> transaction_b =
            pq_b.BeginTransaction();
      },
      "");
}

// Check that there is no crash when Transactions are created on the same thread
// for 2 PriorityQueues which have a predecessor relationship.
TEST(TaskSchedulerPriorityQueueTest, LegalTwoTransactionsSameThread) {
  PriorityQueue pq_a(Bind(&DoNothing));
  PriorityQueue pq_b(Bind(&DoNothing), &pq_a);

  // This shouldn't crash.
  scoped_ptr<PriorityQueue::Transaction> transaction_a =
      pq_a.BeginTransaction();
  scoped_ptr<PriorityQueue::Transaction> transaction_b =
      pq_b.BeginTransaction();
}

// Check that it is possible to begin multiple Transactions for the same
// PriorityQueue on different threads. The call to BeginTransaction() on the
// second thread should block until the Transaction has ended on the first
// thread.
TEST(TaskSchedulerPriorityQueueTest, TwoTransactionsTwoThreads) {
  PriorityQueue pq(Bind(&DoNothing));

  // Call BeginTransaction() on this thread and keep the Transaction alive.
  scoped_ptr<PriorityQueue::Transaction> transaction = pq.BeginTransaction();

  // Call BeginTransaction() on another thread.
  ThreadBeginningTransaction thread_beginning_transaction(&pq);
  thread_beginning_transaction.Start();

  // After a few milliseconds, the call to BeginTransaction() on the other
  // thread should not have returned.
  thread_beginning_transaction.ExpectTransactionDoesNotBegin();

  // End the Transaction on the current thread.
  transaction.reset();

  // The other thread should exit after its call to BeginTransaction() returns.
  thread_beginning_transaction.Join();
}

TEST(TaskSchedulerPriorityQueueTest, SequenceAndSortKeyIsNull) {
  EXPECT_TRUE(PriorityQueue::SequenceAndSortKey().is_null());

  const PriorityQueue::SequenceAndSortKey non_null_sequence_andsort_key(
      make_scoped_refptr(new Sequence),
      SequenceSortKey(TaskPriority::USER_VISIBLE, TimeTicks()));
  EXPECT_FALSE(non_null_sequence_andsort_key.is_null());
}

}  // namespace internal
}  // namespace base
