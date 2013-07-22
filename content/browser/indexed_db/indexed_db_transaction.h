// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_

#include <queue>
#include <set>
#include <stack>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"

namespace content {

class IndexedDBCursor;
class IndexedDBDatabaseCallbacks;

class IndexedDBTransaction : public base::RefCounted<IndexedDBTransaction> {
 public:
  IndexedDBTransaction(int64 id,
                       scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
                       const std::set<int64>& object_store_ids,
                       indexed_db::TransactionMode,
                       IndexedDBDatabase* db);

  virtual void Abort();
  void Commit();

  class Operation {
   public:
    Operation() {}
    virtual ~Operation() {}
    virtual void Perform(IndexedDBTransaction* transaction) = 0;
  };

  void Abort(const IndexedDBDatabaseError& error);
  void Run();
  indexed_db::TransactionMode mode() const { return mode_; }
  const std::set<int64>& scope() const { return object_store_ids_; }
  void ScheduleTask(Operation* task) {
    ScheduleTask(IndexedDBDatabase::NORMAL_TASK, task, NULL);
  }
  void ScheduleTask(Operation* task, Operation* abort_task) {
    ScheduleTask(IndexedDBDatabase::NORMAL_TASK, task, abort_task);
  }
  void ScheduleTask(IndexedDBDatabase::TaskType task_type, Operation* task) {
    ScheduleTask(task_type, task, NULL);
  }
  void ScheduleTask(IndexedDBDatabase::TaskType task_type,
                    Operation* task,
                    Operation* abort_task);
  void RegisterOpenCursor(IndexedDBCursor* cursor);
  void UnregisterOpenCursor(IndexedDBCursor* cursor);
  void AddPreemptiveEvent() { pending_preemptive_events_++; }
  void DidCompletePreemptiveEvent() {
    pending_preemptive_events_--;
    DCHECK_GE(pending_preemptive_events_, 0);
  }
  IndexedDBBackingStore::Transaction* BackingStoreTransaction() {
    return &transaction_;
  }
  int64 id() const { return id_; }

  IndexedDBDatabase* database() const { return database_; }
  IndexedDBDatabaseCallbacks* connection() const { return callbacks_; }
  bool IsRunning() const { return state_ == RUNNING; }

 protected:
  virtual ~IndexedDBTransaction();
  friend class base::RefCounted<IndexedDBTransaction>;

 private:
  enum State {
    UNUSED,         // Created, but no tasks yet.
    START_PENDING,  // Enqueued tasks, but backing store transaction not yet
                    // started.
    RUNNING,        // Backing store transaction started but not yet finished.
    FINISHED,       // Either aborted or committed.
  };

  void Start();

  bool IsTaskQueueEmpty() const;
  bool HasPendingTasks() const;

  void ProcessTaskQueue();
  void CloseOpenCursors();

  const int64 id_;
  const std::set<int64> object_store_ids_;
  const indexed_db::TransactionMode mode_;

  State state_;
  bool commit_pending_;
  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks_;
  scoped_refptr<IndexedDBDatabase> database_;

  class TaskQueue {
   public:
    TaskQueue();
    ~TaskQueue();
    bool empty() const { return queue_.empty(); }
    void push(Operation* task) { queue_.push(task); }
    scoped_ptr<Operation> pop();
    void clear();

   private:
    std::queue<Operation*> queue_;
  };

  class TaskStack {
   public:
    TaskStack();
    ~TaskStack();
    bool empty() const { return stack_.empty(); }
    void push(Operation* task) { stack_.push(task); }
    scoped_ptr<Operation> pop();
    void clear();

   private:
    std::stack<Operation*> stack_;
  };

  TaskQueue task_queue_;
  TaskQueue preemptive_task_queue_;
  TaskStack abort_task_stack_;

  IndexedDBBackingStore::Transaction transaction_;

  bool should_process_queue_;
  int pending_preemptive_events_;

  std::set<IndexedDBCursor*> open_cursors_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_
