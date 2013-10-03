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
  typedef base::Callback<void(bool success)> SaveCallback;
  typedef base::Callback<void(bool success, scoped_ptr<EntryVector>)>
      LoadCallback;

  virtual ~DomDistillerDatabaseInterface() {}

  // Asynchronously destroys the object after all in-progress file operations
  // have completed. The callbacks for in-progress operations will still be
  // called.
  virtual void Destroy() {}

  // Asynchronously initializes the object. |callback| will be invoked on the UI
  // thread when complete.
  virtual void Init(const base::FilePath& database_dir,
                    InitCallback callback) = 0;

  // Asynchronously saves |entries_to_save| database. |callback| will be invoked
  // on the UI thread when complete.
  virtual void SaveEntries(scoped_ptr<EntryVector> entries_to_save,
                           SaveCallback callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(LoadCallback callback) = 0;
};

class DomDistillerDatabase
    : public DomDistillerDatabaseInterface {
 public:
  // The underlying database. Calls to this type may be blocking.
  class Database {
   public:
    virtual bool Init(const base::FilePath& database_dir) = 0;
    virtual bool Save(const EntryVector& entries) = 0;
    virtual bool Load(EntryVector* entries) = 0;
    virtual ~Database() {}
  };

  class LevelDB : public Database {
   public:
    LevelDB();
    virtual ~LevelDB();
    virtual bool Init(const base::FilePath& database_dir) OVERRIDE;
    virtual bool Save(const EntryVector& entries) OVERRIDE;
    virtual bool Load(EntryVector* entries) OVERRIDE;

   private:

    scoped_ptr<leveldb::DB> db_;
  };

  DomDistillerDatabase(scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual ~DomDistillerDatabase();

  // DomDistillerDatabaseInterface implementation.
  virtual void Destroy() OVERRIDE;
  virtual void Init(const base::FilePath& database_dir,
                    InitCallback callback) OVERRIDE;
  virtual void SaveEntries(scoped_ptr<EntryVector> entries_to_save,
                           SaveCallback callback) OVERRIDE;
  virtual void LoadEntries(LoadCallback callback) OVERRIDE;

  // Allow callers to provide their own Database implementation.
  void InitWithDatabase(scoped_ptr<Database> database,
                        const base::FilePath& database_dir,
                        InitCallback callback);

 private:
  // Whether currently being run by |task_runner_|.
  bool IsRunByTaskRunner() const;

  // Whether currently being run on |main_loop_|.
  bool IsRunOnMainLoop() const;

  // Deletes |this|.
  void DestroyFromTaskRunner();

  // Initializes the database in |database_dir| and updates |success|.
  void InitFromTaskRunner(const base::FilePath& database_dir, bool* success);

  // Saves data to disk and updates |success|.
  void SaveEntriesFromTaskRunner(scoped_ptr<EntryVector> entries_to_save,
                                 bool* success);

  // Loads entries from disk and updates |success|.
  void LoadEntriesFromTaskRunner(EntryVector* entries, bool* success);

  // The MessageLoop that the database was created on.
  base::MessageLoop* main_loop_;

  // Used to run blocking tasks in-order.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_ptr<Database> db_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DomDistillerDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerDatabase);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_DATABASE_H_
