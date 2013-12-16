// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_DATABASE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_DATABASE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_collision_warner.h"
#include "components/dom_distiller/core/article_entry.h"

namespace base {
class SequencedTaskRunner;
class MessageLoop;
}

namespace leveldb {
class DB;
}

namespace dom_distiller {

typedef std::vector<ArticleEntry> EntryVector;

// Interface for classes providing persistent storage of DomDistiller entries.
class DomDistillerDatabaseInterface {
 public:
  typedef std::vector<std::string> ArticleEntryIds;
  typedef base::Callback<void(bool success)> InitCallback;
  typedef base::Callback<void(bool success)> UpdateCallback;
  typedef base::Callback<void(bool success, scoped_ptr<EntryVector>)>
      LoadCallback;

  virtual ~DomDistillerDatabaseInterface() {}

  // Asynchronously initializes the object. |callback| will be invoked on the UI
  // thread when complete.
  virtual void Init(const base::FilePath& database_dir,
                    InitCallback callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries from
  // |entries_to_remove| from the database. |callback| will be invoked on the UI
  // thread when complete.
  virtual void UpdateEntries(scoped_ptr<EntryVector> entries_to_save,
                             scoped_ptr<EntryVector> entries_to_remove,
                             UpdateCallback callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(LoadCallback callback) = 0;
};

// When the DomDistillerDatabase instance is deleted, in-progress asynchronous
// operations will be completed and the corresponding callbacks will be called.
class DomDistillerDatabase
    : public DomDistillerDatabaseInterface {
 public:
  // The underlying database. Calls to this type may be blocking.
  class Database {
   public:
    virtual bool Init(const base::FilePath& database_dir) = 0;
    virtual bool Save(const EntryVector& entries_to_save,
                      const EntryVector& entries_to_remove) = 0;
    virtual bool Load(EntryVector* entries) = 0;
    virtual ~Database() {}
  };

  // Once constructed, function calls and destruction should all occur on the
  // same thread (not necessarily the same as the constructor).
  class LevelDB : public Database {
   public:
    LevelDB();
    virtual ~LevelDB();
    virtual bool Init(const base::FilePath& database_dir) OVERRIDE;
    virtual bool Save(const EntryVector& entries_to_save,
                      const EntryVector& entries_to_remove) OVERRIDE;
    virtual bool Load(EntryVector* entries) OVERRIDE;

   private:
    DFAKE_MUTEX(thread_checker_);
    scoped_ptr<leveldb::DB> db_;
  };

  explicit DomDistillerDatabase(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual ~DomDistillerDatabase();

  // DomDistillerDatabaseInterface implementation.
  virtual void Init(const base::FilePath& database_dir,
                    InitCallback callback) OVERRIDE;
  virtual void UpdateEntries(scoped_ptr<EntryVector> entries_to_save,
                           scoped_ptr<EntryVector> entries_to_remove,
                           UpdateCallback callback) OVERRIDE;
  virtual void LoadEntries(LoadCallback callback) OVERRIDE;

  // Allow callers to provide their own Database implementation.
  void InitWithDatabase(scoped_ptr<Database> database,
                        const base::FilePath& database_dir,
                        InitCallback callback);

 private:
  base::ThreadChecker thread_checker_;

  // Used to run blocking tasks in-order.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_ptr<Database> db_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerDatabase);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_DATABASE_H_
