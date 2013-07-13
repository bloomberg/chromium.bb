// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"

namespace content {

IndexedDBTransaction::TaskQueue::TaskQueue() {}
IndexedDBTransaction::TaskQueue::~TaskQueue() { clear(); }

void IndexedDBTransaction::TaskQueue::clear() {
  while (!queue_.empty())
    scoped_ptr<Operation> task(pop());
}

scoped_ptr<IndexedDBTransaction::Operation>
IndexedDBTransaction::TaskQueue::pop() {
  DCHECK(!queue_.empty());
  scoped_ptr<Operation> task(queue_.front());
  queue_.pop();
  return task.Pass();
}

IndexedDBTransaction::TaskStack::TaskStack() {}
IndexedDBTransaction::TaskStack::~TaskStack() { clear(); }

void IndexedDBTransaction::TaskStack::clear() {
  while (!stack_.empty())
    scoped_ptr<Operation> task(pop());
}

scoped_ptr<IndexedDBTransaction::Operation>
IndexedDBTransaction::TaskStack::pop() {
  DCHECK(!stack_.empty());
  scoped_ptr<Operation> task(stack_.top());
  stack_.pop();
  return task.Pass();
}

IndexedDBTransaction::IndexedDBTransaction(
    int64 id,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
    const std::set<int64>& object_store_ids,
    indexed_db::TransactionMode mode,
    IndexedDBDatabase* database)
    : id_(id),
      object_store_ids_(object_store_ids),
      mode_(mode),
      state_(UNUSED),
      commit_pending_(false),
      callbacks_(callbacks),
      database_(database),
      transaction_(database->BackingStore().get()),
      should_process_queue_(false),
      pending_preemptive_events_(0) {
  database_->transaction_coordinator().DidCreateTransaction(this);
}

IndexedDBTransaction::~IndexedDBTransaction() {
  // It shouldn't be possible for this object to get deleted until it's either
  // complete or aborted.
  DCHECK_EQ(state_, FINISHED);
  DCHECK(preemptive_task_queue_.empty());
  DCHECK(task_queue_.empty());
  DCHECK(abort_task_stack_.empty());
}

void IndexedDBTransaction::ScheduleTask(IndexedDBDatabase::TaskType type,
                                        Operation* task,
                                        Operation* abort_task) {
  if (state_ == FINISHED)
    return;

  if (type == IndexedDBDatabase::NORMAL_TASK)
    task_queue_.push(task);
  else
    preemptive_task_queue_.push(task);

  if (abort_task)
    abort_task_stack_.push(abort_task);

  if (state_ == UNUSED) {
    Start();
  } else if (state_ == RUNNING && !should_process_queue_) {
    should_process_queue_ = true;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&IndexedDBTransaction::ProcessTaskQueue, this));
  }
}

void IndexedDBTransaction::Abort() {
  Abort(IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error (unknown cause)"));
}

void IndexedDBTransaction::Abort(const IndexedDBDatabaseError& error) {
  IDB_TRACE("IndexedDBTransaction::Abort");
  if (state_ == FINISHED)
    return;

  bool was_running = state_ == RUNNING;

  // The last reference to this object may be released while performing the
  // abort steps below. We therefore take a self reference to keep ourselves
  // alive while executing this method.
  scoped_refptr<IndexedDBTransaction> protect(this);

  state_ = FINISHED;
  should_process_queue_ = false;

  if (was_running)
    transaction_.Rollback();

  // Run the abort tasks, if any.
  while (!abort_task_stack_.empty()) {
    scoped_ptr<Operation> task(abort_task_stack_.pop());
    task->Perform(0);
  }
  preemptive_task_queue_.clear();
  task_queue_.clear();

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_.Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  database_->transaction_coordinator().DidFinishTransaction(this);
#ifndef NDEBUG
  DCHECK(!database_->transaction_coordinator().IsActive(this));
#endif
  database_->TransactionFinished(this);

  if (callbacks_.get())
    callbacks_->OnAbort(id_, error);

  database_->TransactionFinishedAndAbortFired(this);

  database_ = NULL;
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

void IndexedDBTransaction::Run() {
  // TransactionCoordinator has started this transaction.
  DCHECK(state_ == START_PENDING || state_ == RUNNING);
  DCHECK(!should_process_queue_);

  should_process_queue_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&IndexedDBTransaction::ProcessTaskQueue, this));
}

void IndexedDBTransaction::Start() {
  DCHECK_EQ(state_, UNUSED);

  state_ = START_PENDING;
  database_->transaction_coordinator().DidStartTransaction(this);
  database_->TransactionStarted(this);
}

void IndexedDBTransaction::Commit() {
  IDB_TRACE("IndexedDBTransaction::Commit");

  // In multiprocess ports, front-end may have requested a commit but
  // an abort has already been initiated asynchronously by the
  // back-end.
  if (state_ == FINISHED)
    return;

  DCHECK(state_ == UNUSED || state_ == RUNNING);
  commit_pending_ = true;

  // Front-end has requested a commit, but there may be tasks like
  // create_index which are considered synchronous by the front-end
  // but are processed asynchronously.
  if (HasPendingTasks())
    return;

  // The last reference to this object may be released while performing the
  // commit steps below. We therefore take a self reference to keep ourselves
  // alive while executing this method.
  scoped_refptr<IndexedDBTransaction> protect(this);

  // TODO(jsbell): Run abort tasks if commit fails? http://crbug.com/241843
  abort_task_stack_.clear();

  bool unused = state_ == UNUSED;
  state_ = FINISHED;

  bool committed = unused || transaction_.Commit();

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_.Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  database_->transaction_coordinator().DidFinishTransaction(this);
  database_->TransactionFinished(this);

  if (committed) {
    callbacks_->OnComplete(id_);
    database_->TransactionFinishedAndCompleteFired(this);
  } else {
    callbacks_->OnAbort(
        id_,
        IndexedDBDatabaseError(WebKit::WebIDBDatabaseExceptionUnknownError,
                               "Internal error committing transaction."));
    database_->TransactionFinishedAndAbortFired(this);
  }

  database_ = NULL;
}

void IndexedDBTransaction::ProcessTaskQueue() {
  IDB_TRACE("IndexedDBTransaction::ProcessTaskQueue");

  // May have been aborted.
  if (!should_process_queue_)
    return;

  DCHECK(!IsTaskQueueEmpty());
  should_process_queue_ = false;

  if (state_ == START_PENDING) {
    transaction_.Begin();
    state_ = RUNNING;
  }

  // The last reference to this object may be released while performing the
  // tasks. Take take a self reference to keep this object alive so that
  // the loop termination conditions can be checked.
  scoped_refptr<IndexedDBTransaction> protect(this);

  TaskQueue* task_queue =
      pending_preemptive_events_ ? &preemptive_task_queue_ : &task_queue_;
  while (!task_queue->empty() && state_ != FINISHED) {
    DCHECK_EQ(state_, RUNNING);
    scoped_ptr<Operation> task(task_queue->pop());
    task->Perform(this);

    // Event itself may change which queue should be processed next.
    task_queue =
        pending_preemptive_events_ ? &preemptive_task_queue_ : &task_queue_;
  }

  // If there are no pending tasks, we haven't already committed/aborted,
  // and the front-end requested a commit, it is now safe to do so.
  if (!HasPendingTasks() && state_ != FINISHED && commit_pending_)
    Commit();
}

void IndexedDBTransaction::CloseOpenCursors() {
  for (std::set<IndexedDBCursor*>::iterator i = open_cursors_.begin();
       i != open_cursors_.end();
       ++i)
    (*i)->Close();
  open_cursors_.clear();
}

}  // namespace content
