// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_URL_CACHE_DATABASE_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_URL_CACHE_DATABASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

class FilePath;

namespace sql {
class Statement;
}

namespace history {

class InMemoryURLIndex;
class URLIndexPrivateData;

// Supports caching and restoring of the private data of the InMemoryURLIndex.
// This class has intimate knowledge of InMemoryURLIndex's URLIndexPrivateData
// class and directly pokes data therein during cache restoration. The only
// client of this class is URLIndexPrivateData.
//
// Tables in this database:
//    word         - Indexed words with IDs (from word_map_ and word_list_).
//    char_words   - Indexed characters with IDs of words containing the
//                   character (from char_word_map_).
//    word_history - Word IDs with the IDs of history items containing the
//                   word (from word_id_history_map_).
//    url          - URLs and page titles along with visit and other metrics
//                   (from the history_info_map_ and word_starts_map_).
//    url_word_starts - History IDs with the starting position of each word
//                   found in its URL.
//    title_word_starts - History IDs with the starting position of each word
//                   found in its page title.
//
// NOTE: The history IDs in this database for URLs are identical to those in the
// main history database.
class InMemoryURLCacheDatabase
    : public base::RefCountedThreadSafe<InMemoryURLCacheDatabase> {
 public:
  InMemoryURLCacheDatabase();

  // Initializes the database connection. This must return true before any other
  // functions on this class are called. |file_path| gives the path to where
  // the cache database is located. |sequence_token| is used to coordinate all
  // access to the cache database. This is normally called on the DB thread but
  // may also be called in the sequenced worker pool for certain unit tests.
  bool Init(const FilePath& file_path,
            const base::SequencedWorkerPool::SequenceToken& sequence_token);

  // Shuts down the database connection. Posts a sequenced task that performs
  // the matching ShutDownTask function below.
  void Shutdown();

  // Restore the private InMemoryURLIndex |data| from the database. Returns true
  // if there was data to restore and the restore was successful. It is
  // considered an error and false is returned if _any_ of the restored data
  // structures in the private data, except |available_words_|, are empty after
  // restoration. Technically, the case where _all_ data structures are empty
  // after a restore is an error only when the user's history has _not_ been
  // cleared. In the case where history _has_ been cleared and the data
  // structures are legitimately empty we still return a false as the cost of
  // attempting to rebuild the private data from an empty history database is
  // negligible.
  bool RestorePrivateData(URLIndexPrivateData* data);

  // Reset the database by dropping and recreating all database tables. Return
  // true if successful. Note that a failure does not close the database.
  virtual bool Reset();

  // Adds information for a new |row| to the various database tables. These
  // functions post a sequenced task that performs the matching '[name]Task'
  // functions below. The client should wrap these functions in a transaction
  // by the client by calling BeginTransaction(), whatever combination of these
  // function as appropriate, and finally CommitTransaction(). The
  // CommitTransaction() will notify the client if any failure occurs.
  void AddHistoryToURLs(HistoryID history_id, const URLRow& row);
  void AddHistoryToWordHistory(WordID word_id, HistoryID history_id);
  void AddWordToWords(WordID word_id, const string16& uni_word);
  void AddWordToCharWords(char16 uni_char, WordID word_id);
  void AddRowWordStarts(HistoryID history_id,
                        const RowWordStarts& row_word_starts);

  // Deletes row information from the various database tables. These functions
  // post a sequenced task that performs the matching '[name]Task' functions
  // below.
  void RemoveHistoryIDFromURLs(HistoryID history_id);
  void RemoveHistoryIDFromWordHistory(HistoryID history_id);
  void RemoveWordFromWords(WordID word_id);
  void RemoveWordStarts(HistoryID history_id);

  // Wraps transactions on the database. We support nested transactions and only
  // commit when the outermost one is committed (sqlite doesn't support true
  // nested transactions). These functions post a sequenced task that performs
  // the matching '[name]Task' functions below.
  void BeginTransaction();
  void CommitTransaction();

  // Completely replaces all data in the cache database with a fresh image.
  // Returns true upon success. Must not be called on the main thread and and
  // the caller should ensure that no other cache database update operations
  // take place while this method is being performed. Returns true if the
  // refresh was successful.
  virtual bool Refresh(const URLIndexPrivateData& index_data);

 private:
  friend class base::RefCountedThreadSafe<InMemoryURLCacheDatabase>;
  friend class InMemoryURLIndexCacheTest;
  friend class InMemoryURLIndexTest;
  friend class InterposingCacheDatabase;
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheAddRemove);

  virtual ~InMemoryURLCacheDatabase();

  // Initializes the database and returns true if successful.
  bool InitDatabase();

  // Posts the given |task| for sequential execution using a pool task
  // coordinated by |sequence_token_|.
  void PostSequencedDBTask(const tracked_objects::Location& from_here,
                           const base::Closure& task);

  // Closes the database.
  void ShutdownTask();

  // Makes sure the version is up-to-date, updating if necessary. Notify the
  // user if the database is newer than expected.
  sql::InitStatus EnsureCurrentVersion();

  // Starts a database transaction.
  void BeginTransactionTask();

  // Commits the database transaction if no update operations failed after
  // BeginTransactionTask() was called, otherwise rolls back the transaction.
  // Sends a NOTIFICATION_IN_MEMORY_URL_CACHE_DATABASE_FAILURE notification
  // if the transaction failed.
  void CommitTransactionTask();

  // Returns true if all tables exist.
  bool VerifyTables();

  // Creates the various tables and indexes returning true if successful.
  bool CreateTables();

  // Notifies observers that a failure occurred during a database update
  // operation.
  void NotifyDatabaseFailure();

  // Executes |statement|, sets |update_error_| if the statement did not
  // run successfully, and returns true if the statement was run successfully.
  bool RunStatement(sql::Statement* statement);

  // Adds information about a new row to the various database tables.
  void AddHistoryToURLsTask(HistoryID history_id, const URLRow& row);
  void AddHistoryToWordHistoryTask(WordID word_id, HistoryID history_id);
  // Note: AddWordToWordsTask is virtual so that it can be overridden by
  // unit tests to simulate database failures.
  virtual void AddWordToWordsTask(WordID word_id, const string16& uni_word);
  void AddWordToCharWordsTask(char16 uni_char, WordID word_id);
  void AddRowWordStartsTask(HistoryID history_id,
                            const RowWordStarts& row_word_starts);

  // Deletes row information from the various database tables. Returns true if
  // successful.
  void RemoveHistoryIDFromURLsTask(HistoryID history_id);
  void RemoveHistoryIDFromWordHistoryTask(HistoryID history_id);
  void RemoveWordFromWordsTask(WordID word_id);
  void RemoveWordStartsTask(HistoryID history_id);

  // Completely replaces all data in the cache with a fresh image. Returns true
  // if successful.
  bool RefreshWords(const URLIndexPrivateData& index_data);
  bool RefreshCharWords(const URLIndexPrivateData& index_data);
  bool RefreshWordHistory(const URLIndexPrivateData& index_data);
  bool RefreshURLs(const URLIndexPrivateData& index_data);
  bool RefreshWordStarts(const URLIndexPrivateData& index_data);

  // Restores individual sections of the private data for the InMemoryURLIndex.
  // Returns true if there was data to restore and the restore was successful.
  bool RestoreWords(URLIndexPrivateData* index_data);
  bool RestoreCharWords(URLIndexPrivateData* index_data);
  bool RestoreWordHistory(URLIndexPrivateData* index_data);
  bool RestoreURLs(URLIndexPrivateData* index_data);
  bool RestoreWordStarts(URLIndexPrivateData* index_data);

  // Returns the database connection.
  sql::Connection* get_db_for_testing() { return &db_; }

  // Sequence token for coordinating database tasks.
  base::SequencedWorkerPool::SequenceToken sequence_token_;

  // The database.
  sql::Connection db_;
  sql::MetaTable meta_table_;
  int update_error_;

  // Set to true once the shutdown process has begun.
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryURLCacheDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_CACHE_DATABASE_H_
