// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"

namespace sql {
class Connection;
}

namespace offline_pages {

enum class InitializationStatus {
  NOT_INITIALIZED,
  INITIALIZING,
  SUCCESS,
  FAILURE,
};

// PrefetchStore is a front end to SQLite store hosting prefetch related
// items.
//
// The store controls the pointer to the SQLite database and only makes it
// available to the |RunCallback| of the |Execute| method on the blocking
// thread.
//
// Store has a set of auxiliary functions meant to be used on the blocking
// thread. They can be found in prefetch_store_sql_utils file.
class PrefetchStore {
 public:
  // Definition of the callback that is going to run the core of the command in
  // the |Execute| method.
  template <typename T>
  using RunCallback = base::OnceCallback<T(sql::Connection*)>;

  // Definition of the callback used to pass the result back to the caller of
  // |Execute| method.
  template <typename T>
  using ResultCallback = base::OnceCallback<void(T)>;

  // Creates an instance of |PrefetchStore| with an in-memory SQLite database.
  explicit PrefetchStore(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  // Creates an instance of |PrefetchStore| with a SQLite database stored in
  // |database_dir|.
  PrefetchStore(scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
                const base::FilePath& database_dir);

  ~PrefetchStore();

  // Executes a |run_callback| on SQL store on the blocking thread, and posts
  // its result back to calling thread through |result_callback|.
  // Calling |Execute| when store is NOT_INITIALIZED will cause the store
  // initialization to start.
  // Store initialization status needs to be SUCCESS for test task to run, or
  // FAILURE, in which case the |db| pointer passed to |run_callback| will be
  // null and such case should be gracefully handled.
  template <typename T>
  void Execute(RunCallback<T> run_callback, ResultCallback<T> result_callback) {
    CHECK_NE(initialization_status_, InitializationStatus::INITIALIZING);

    if (initialization_status_ == InitializationStatus::NOT_INITIALIZED) {
      Initialize(base::BindOnce(
          &PrefetchStore::Execute<T>, weak_ptr_factory_.GetWeakPtr(),
          std::move(run_callback), std::move(result_callback)));
      return;
    }

    sql::Connection* db =
        initialization_status_ == InitializationStatus::SUCCESS ? db_.get()
                                                                : nullptr;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(), FROM_HERE,
        base::BindOnce(std::move(run_callback), db),
        std::move(result_callback));
  }

  // Gets the initialization status of the store.
  InitializationStatus initialization_status() const {
    return initialization_status_;
  }

  static const char* GetTableCreationSqlForTesting();

 private:
  // Used internally to initialize connection.
  void Initialize(base::OnceClosure pending_command);

  // Used to conclude opening/resetting DB connection.
  void OnInitializeDone(base::OnceClosure pending_command, bool success);

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Path to the database on disk.
  base::FilePath db_file_path_;

  // Only open the store in memory. Used for testing.
  bool in_memory_;

  // Database connection.
  std::unique_ptr<sql::Connection, base::OnTaskRunnerDeleter> db_;

  // Initialization status of the store.
  InitializationStatus initialization_status_;

  // Weak pointer to control the callback.
  base::WeakPtrFactory<PrefetchStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchStore);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_H_
