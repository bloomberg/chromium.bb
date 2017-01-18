// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content {

namespace {

const int64_t kInactivityTimeoutPeriodSeconds = 60;

// Helper for posting a task to call IndexedDBTransaction::Commit when we know
// the transaction had no requests and therefore the commit must succeed.
void CommitUnused(base::WeakPtr<IndexedDBTransaction> transaction) {
  if (!transaction)
    return;
  leveldb::Status status = transaction->Commit();
  DCHECK(status.ok());
}

}  // namespace

IndexedDBTransaction::TaskQueue::TaskQueue() {}
IndexedDBTransaction::TaskQueue::~TaskQueue() { clear(); }

void IndexedDBTransaction::TaskQueue::clear() {
  while (!queue_.empty())
    queue_.pop();
}

IndexedDBTransaction::Operation IndexedDBTransaction::TaskQueue::pop() {
  DCHECK(!queue_.empty());
  Operation task = std::move(queue_.front());
  queue_.pop();
  return task;
}

IndexedDBTransaction::TaskStack::TaskStack() {}
IndexedDBTransaction::TaskStack::~TaskStack() { clear(); }

void IndexedDBTransaction::TaskStack::clear() {
  while (!stack_.empty())
    stack_.pop();
}

IndexedDBTransaction::AbortOperation IndexedDBTransaction::TaskStack::pop() {
  DCHECK(!stack_.empty());
  AbortOperation task = std::move(stack_.top());
  stack_.pop();
  return task;
}

IndexedDBTransaction::IndexedDBTransaction(
    int64_t id,
    IndexedDBConnection* connection,
    const std::set<int64_t>& object_store_ids,
    blink::WebIDBTransactionMode mode,
    IndexedDBBackingStore::Transaction* backing_store_transaction)
    : id_(id),
      object_store_ids_(object_store_ids),
      mode_(mode),
      connection_(std::move(connection)),
      transaction_(backing_store_transaction),
      ptr_factory_(this) {
  callbacks_ = connection_->callbacks();
  database_ = connection_->database();

  diagnostics_.tasks_scheduled = 0;
  diagnostics_.tasks_completed = 0;
  diagnostics_.creation_time = base::Time::Now();
}

IndexedDBTransaction::~IndexedDBTransaction() {
  // It shouldn't be possible for this object to get deleted until it's either
  // complete or aborted.
  DCHECK_EQ(state_, FINISHED);
  DCHECK(preemptive_task_queue_.empty());
  DCHECK_EQ(pending_preemptive_events_, 0);
  DCHECK(task_queue_.empty());
  DCHECK(abort_task_stack_.empty());
  DCHECK(!processing_event_queue_);
}

void IndexedDBTransaction::ScheduleTask(blink::WebIDBTaskType type,
                                        Operation task) {
  DCHECK_NE(state_, COMMITTING);
  if (state_ == FINISHED)
    return;

  timeout_timer_.Stop();
  used_ = true;
  if (type == blink::WebIDBTaskTypeNormal) {
    task_queue_.push(task);
    ++diagnostics_.tasks_scheduled;
  } else {
    preemptive_task_queue_.push(task);
  }
  RunTasksIfStarted();
}

void IndexedDBTransaction::ScheduleAbortTask(AbortOperation abort_task) {
  DCHECK_NE(FINISHED, state_);
  DCHECK(used_);
  abort_task_stack_.push(std::move(abort_task));
}

void IndexedDBTransaction::RunTasksIfStarted() {
  DCHECK(used_);

  // Not started by the coordinator yet.
  if (state_ != STARTED)
    return;

  // A task is already posted.
  if (should_process_queue_)
    return;

  should_process_queue_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&IndexedDBTransaction::ProcessTaskQueue,
                            ptr_factory_.GetWeakPtr()));
}

void IndexedDBTransaction::Abort() {
  Abort(IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                               "Internal error (unknown cause)"));
}

void IndexedDBTransaction::Abort(const IndexedDBDatabaseError& error) {
  IDB_TRACE1("IndexedDBTransaction::Abort", "txn.id", id());
  DCHECK(!processing_event_queue_);
  if (state_ == FINISHED)
    return;

  timeout_timer_.Stop();

  state_ = FINISHED;
  should_process_queue_ = false;

  if (backing_store_transaction_begun_)
    transaction_->Rollback();

  // Run the abort tasks, if any.
  while (!abort_task_stack_.empty())
    abort_task_stack_.pop().Run();

  preemptive_task_queue_.clear();
  pending_preemptive_events_ = 0;
  task_queue_.clear();

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_->Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  database_->transaction_coordinator().DidFinishTransaction(this);
#ifndef NDEBUG
  DCHECK(!database_->transaction_coordinator().IsActive(this));
#endif

  if (callbacks_.get())
    callbacks_->OnAbort(*this, error);

  database_->TransactionFinished(this, false);

  // RemoveTransaction will delete |this|.
  connection_->RemoveTransaction(id_);
}

bool IndexedDBTransaction::IsTaskQueueEmpty() const {
  return preemptive_task_queue_.empty() && task_queue_.empty();
}

bool IndexedDBTransaction::HasPendingTasks() const {
  return pending_preemptive_events_ || !IsTaskQueueEmpty();
}

void IndexedDBTransaction::RegisterOpenCursor(IndexedDBCursor* cursor) {
  open_cursors_.insert(cursor);
}

void IndexedDBTransaction::UnregisterOpenCursor(IndexedDBCursor* cursor) {
  open_cursors_.erase(cursor);
}

void IndexedDBTransaction::Start() {
  // TransactionCoordinator has started this transaction.
  DCHECK_EQ(CREATED, state_);
  state_ = STARTED;
  diagnostics_.start_time = base::Time::Now();

  if (!used_) {
    if (commit_pending_) {
      // The transaction has never had requests issued against it, but the
      // front-end previously requested a commit; do the commit now, but not
      // re-entrantly as that may renter the coordinator.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&CommitUnused, ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  RunTasksIfStarted();
}

void IndexedDBTransaction::GrabSnapshotThenStart() {
  DCHECK(!backing_store_transaction_begun_);
  transaction_->Begin();
  backing_store_transaction_begun_ = true;
  Start();
}

class BlobWriteCallbackImpl : public IndexedDBBackingStore::BlobWriteCallback {
 public:
  explicit BlobWriteCallbackImpl(
      base::WeakPtr<IndexedDBTransaction> transaction)
      : transaction_(std::move(transaction)) {}

  leveldb::Status Run(IndexedDBBackingStore::BlobWriteResult result) override {
    if (!transaction_)
      return leveldb::Status::OK();
    return transaction_->BlobWriteComplete(result);
  }

 protected:
  ~BlobWriteCallbackImpl() override {}

 private:
  base::WeakPtr<IndexedDBTransaction> transaction_;
};

leveldb::Status IndexedDBTransaction::BlobWriteComplete(
    IndexedDBBackingStore::BlobWriteResult result) {
  IDB_TRACE("IndexedDBTransaction::BlobWriteComplete");
  if (state_ == FINISHED)  // aborted
    return leveldb::Status::OK();
  DCHECK_EQ(state_, COMMITTING);

  leveldb::Status s = leveldb::Status::OK();
  // Switch statement to protect against adding new enum values.
  switch (result) {
    case IndexedDBBackingStore::BlobWriteResult::FAILURE_ASYNC:
      Abort(IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionDataError,
                                   "Failed to write blobs."));
      return leveldb::Status::OK();
    case IndexedDBBackingStore::BlobWriteResult::SUCCESS_ASYNC:
    case IndexedDBBackingStore::BlobWriteResult::SUCCESS_SYNC: {
      // Save the database as |this| can be destroyed in the next line. We also
      // make
      // sure to handle the error if we're not being called synchronously.
      scoped_refptr<IndexedDBDatabase> database = database_;
      s = CommitPhaseTwo();
      if (!s.ok() &&
          result == IndexedDBBackingStore::BlobWriteResult::SUCCESS_ASYNC)
        database->ReportError(s);
      break;
    }
  }
  return s;
}

leveldb::Status IndexedDBTransaction::Commit() {
  IDB_TRACE1("IndexedDBTransaction::Commit", "txn.id", id());

  timeout_timer_.Stop();

  // In multiprocess ports, front-end may have requested a commit but
  // an abort has already been initiated asynchronously by the
  // back-end.
  if (state_ == FINISHED)
    return leveldb::Status::OK();
  DCHECK_NE(state_, COMMITTING);

  DCHECK(!used_ || state_ == STARTED);
  commit_pending_ = true;

  // Front-end has requested a commit, but this transaction is blocked by
  // other transactions. The commit will be initiated when the transaction
  // coordinator unblocks this transaction.
  if (state_ != STARTED)
    return leveldb::Status::OK();

  // Front-end has requested a commit, but there may be tasks like
  // create_index which are considered synchronous by the front-end
  // but are processed asynchronously.
  if (HasPendingTasks())
    return leveldb::Status::OK();

  state_ = COMMITTING;

  leveldb::Status s;
  if (!used_) {
    s = CommitPhaseTwo();
  } else {
    scoped_refptr<IndexedDBBackingStore::BlobWriteCallback> callback(
        new BlobWriteCallbackImpl(ptr_factory_.GetWeakPtr()));
    // CommitPhaseOne will call the callback synchronously if there are no blobs
    // to write.
    s = transaction_->CommitPhaseOne(callback);
  }

  return s;
}

leveldb::Status IndexedDBTransaction::CommitPhaseTwo() {
  // Abort may have been called just as the blob write completed.
  if (state_ == FINISHED)
    return leveldb::Status::OK();

  DCHECK_EQ(state_, COMMITTING);

  state_ = FINISHED;

  leveldb::Status s;
  bool committed;
  if (!used_) {
    committed = true;
  } else {
    s = transaction_->CommitPhaseTwo();
    committed = s.ok();
  }

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_->Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  database_->transaction_coordinator().DidFinishTransaction(this);

  if (committed) {
    abort_task_stack_.clear();

    // SendObservations must be called before OnComplete to ensure consistency
    // of callbacks at renderer.
    if (!connection_changes_map_.empty()) {
      database_->SendObservations(std::move(connection_changes_map_));
      connection_changes_map_.clear();
    }
    {
      IDB_TRACE1(
          "IndexedDBTransaction::CommitPhaseTwo.TransactionCompleteCallbacks",
          "txn.id", id());
      callbacks_->OnComplete(*this);
    }
    if (!pending_observers_.empty() && connection_)
      connection_->ActivatePendingObservers(std::move(pending_observers_));

    database_->TransactionFinished(this, true);
    // RemoveTransaction will delete |this|.
    connection_->RemoveTransaction(id_);
    return s;
  } else {
    while (!abort_task_stack_.empty())
      abort_task_stack_.pop().Run();

    IndexedDBDatabaseError error;
    if (leveldb_env::IndicatesDiskFull(s)) {
      error = IndexedDBDatabaseError(
          blink::WebIDBDatabaseExceptionQuotaError,
          "Encountered disk full while committing transaction.");
    } else {
      error = IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                                     "Internal error committing transaction.");
    }
    callbacks_->OnAbort(*this, error);
    database_->TransactionFinished(this, false);
  }
  return s;
}

void IndexedDBTransaction::ProcessTaskQueue() {
  IDB_TRACE1("IndexedDBTransaction::ProcessTaskQueue", "txn.id", id());

  DCHECK(!processing_event_queue_);

  // May have been aborted.
  if (!should_process_queue_)
    return;

  processing_event_queue_ = true;

  DCHECK(!IsTaskQueueEmpty());
  should_process_queue_ = false;

  if (!backing_store_transaction_begun_) {
    transaction_->Begin();
    backing_store_transaction_begun_ = true;
  }

  TaskQueue* task_queue =
      pending_preemptive_events_ ? &preemptive_task_queue_ : &task_queue_;
  while (!task_queue->empty() && state_ != FINISHED) {
    DCHECK_EQ(state_, STARTED);
    Operation task(task_queue->pop());
    leveldb::Status result = task.Run(this);
    if (!pending_preemptive_events_) {
      DCHECK(diagnostics_.tasks_completed < diagnostics_.tasks_scheduled);
      ++diagnostics_.tasks_completed;
    }
    if (!result.ok()) {
      processing_event_queue_ = false;
      database_->ReportError(result);
      return;
    }

    // Event itself may change which queue should be processed next.
    task_queue =
        pending_preemptive_events_ ? &preemptive_task_queue_ : &task_queue_;
  }

  // If there are no pending tasks, we haven't already committed/aborted,
  // and the front-end requested a commit, it is now safe to do so.
  if (!HasPendingTasks() && state_ != FINISHED && commit_pending_) {
    processing_event_queue_ = false;
    // This can delete |this|.
    leveldb::Status result = Commit();
    if (!result.ok())
      database_->ReportError(result);
    return;
  }

  // The transaction may have been aborted while processing tasks.
  if (state_ == FINISHED) {
    processing_event_queue_ = false;
    return;
  }

  DCHECK(state_ == STARTED);

  // Otherwise, start a timer in case the front-end gets wedged and
  // never requests further activity. Read-only transactions don't
  // block other transactions, so don't time those out.
  if (mode_ != blink::WebIDBTransactionModeReadOnly) {
    timeout_timer_.Start(
        FROM_HERE, GetInactivityTimeout(),
        base::Bind(&IndexedDBTransaction::Timeout, ptr_factory_.GetWeakPtr()));
  }
  processing_event_queue_ = false;
}

base::TimeDelta IndexedDBTransaction::GetInactivityTimeout() const {
  return base::TimeDelta::FromSeconds(kInactivityTimeoutPeriodSeconds);
}

void IndexedDBTransaction::Timeout() {
  Abort(IndexedDBDatabaseError(
      blink::WebIDBDatabaseExceptionTimeoutError,
      base::ASCIIToUTF16("Transaction timed out due to inactivity.")));
}

void IndexedDBTransaction::CloseOpenCursors() {
  IDB_TRACE1("IndexedDBTransaction::CloseOpenCursors", "txn.id", id());
  for (auto* cursor : open_cursors_)
    cursor->Close();
  open_cursors_.clear();
}

void IndexedDBTransaction::AddPendingObserver(
    int32_t observer_id,
    const IndexedDBObserver::Options& options) {
  DCHECK_NE(mode(), blink::WebIDBTransactionModeVersionChange);
  pending_observers_.push_back(base::MakeUnique<IndexedDBObserver>(
      observer_id, object_store_ids_, options));
}

void IndexedDBTransaction::RemovePendingObservers(
    const std::vector<int32_t>& pending_observer_ids) {
  const auto& it = std::remove_if(
      pending_observers_.begin(), pending_observers_.end(),
      [&pending_observer_ids](const std::unique_ptr<IndexedDBObserver>& o) {
        return base::ContainsValue(pending_observer_ids, o->id());
      });
  if (it != pending_observers_.end())
    pending_observers_.erase(it, pending_observers_.end());
}

void IndexedDBTransaction::AddObservation(
    int32_t connection_id,
    ::indexed_db::mojom::ObservationPtr observation) {
  auto it = connection_changes_map_.find(connection_id);
  if (it == connection_changes_map_.end()) {
    it = connection_changes_map_
             .insert(std::make_pair(
                 connection_id, ::indexed_db::mojom::ObserverChanges::New()))
             .first;
  }
  it->second->observations.push_back(std::move(observation));
}

::indexed_db::mojom::ObserverChangesPtr*
IndexedDBTransaction::GetPendingChangesForConnection(int32_t connection_id) {
  auto it = connection_changes_map_.find(connection_id);
  if (it != connection_changes_map_.end())
    return &it->second;
  return nullptr;
}

}  // namespace content
