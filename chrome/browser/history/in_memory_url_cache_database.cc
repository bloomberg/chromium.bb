// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_cache_database.h"

#include <algorithm>
#include <functional>
#include <iterator>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "sql/diagnostic_error_delegate.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"

using content::BrowserThread;

namespace history {

namespace {

static const int kCurrentVersionNumber = 1;
static const int kCompatibleVersionNumber = 1;

const char* kWordsTableName = "words";
const char* kCharWordsTableName = "char_words";
const char* kWordHistoryTableName = "word_history";
const char* kURLsTableName = "urls";
const char* kURLWordStartsTableName = "url_word_starts";
const char* kTitleWordStartsTableName = "title_word_starts";

}  // namespace

// Public Methods --------------------------------------------------------------

InMemoryURLCacheDatabase::InMemoryURLCacheDatabase()
    : update_error_(SQLITE_OK),
      shutdown_(false) {
}

InMemoryURLCacheDatabase::~InMemoryURLCacheDatabase() {}

bool InMemoryURLCacheDatabase::Init(
    const FilePath& file_path,
    const base::SequencedWorkerPool::SequenceToken& sequence_token) {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (shutdown_)
    return false;
  sequence_token_ = sequence_token;
  db_.set_error_delegate(GetErrorHandlerForHQPCacheDb());
  // Don't use very much memory caching this database. We primarily use it
  // as a backing store for existing in-memory data.
  db_.set_cache_size(64);
  db_.set_exclusive_locking();
  if (!db_.Open(file_path)) {
    UMA_HISTOGRAM_ENUMERATION("Sqlite.HQPCacheOpen.Error", db_.GetErrorCode(),
                              sql::kMaxSqliteError);
    return false;
  }
  if (!InitDatabase()) {
    db_.Close();
    return false;
  }
  return true;
}

void InMemoryURLCacheDatabase::Shutdown() {
  shutdown_ = true;
  // Allow all outstanding sequenced database operations to complete.
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::ShutdownTask, this));
}

bool InMemoryURLCacheDatabase::RestorePrivateData(URLIndexPrivateData* data) {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  return RestoreWords(data) && RestoreCharWords(data) &&
      RestoreWordHistory(data) && RestoreURLs(data) && RestoreWordStarts(data);
}

bool InMemoryURLCacheDatabase::Reset() {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (db_.Raze() && CreateTables())
    return true;
  UMA_HISTOGRAM_ENUMERATION("Sqlite.HQPCacheCreate.Error", db_.GetErrorCode(),
                            sql::kMaxSqliteError);
  return false;
}

// Database Updating -----------------------------------------------------------

void InMemoryURLCacheDatabase::AddHistoryToURLs(history::HistoryID history_id,
                                                const history::URLRow& row) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::AddHistoryToURLsTask, this,
                 history_id, row));
}

void InMemoryURLCacheDatabase::AddHistoryToWordHistory(WordID word_id,
                                                       HistoryID history_id) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::AddHistoryToWordHistoryTask, this,
                 word_id, history_id));
}

void InMemoryURLCacheDatabase::AddWordToWords(WordID word_id,
                                              const string16& uni_word) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::AddWordToWordsTask, this, word_id,
                 uni_word));
}

void InMemoryURLCacheDatabase::AddWordToCharWords(char16 uni_char,
                                                  WordID word_id) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::AddWordToCharWordsTask, this,
                 uni_char, word_id));
}

void InMemoryURLCacheDatabase::AddRowWordStarts(
    HistoryID history_id,
    const RowWordStarts& row_word_starts) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::AddRowWordStartsTask, this,
                 history_id, row_word_starts));
}

void InMemoryURLCacheDatabase::RemoveHistoryIDFromURLs(HistoryID history_id) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::RemoveHistoryIDFromURLsTask, this,
                 history_id));
}

void InMemoryURLCacheDatabase::RemoveHistoryIDFromWordHistory(
    HistoryID history_id) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::RemoveHistoryIDFromWordHistoryTask,
                 this, history_id));
}

void InMemoryURLCacheDatabase::RemoveWordFromWords(WordID word_id) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::RemoveWordFromWordsTask, this,
                 word_id));
}

void InMemoryURLCacheDatabase::RemoveWordStarts(HistoryID history_id) {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::RemoveWordStartsTask, this,
                 history_id));
}

void InMemoryURLCacheDatabase::BeginTransaction() {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::BeginTransactionTask, this));
}

void InMemoryURLCacheDatabase::CommitTransaction() {
  PostSequencedDBTask(FROM_HERE,
      base::Bind(&InMemoryURLCacheDatabase::CommitTransactionTask, this));
}

bool InMemoryURLCacheDatabase::Refresh(const URLIndexPrivateData& data) {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  return RefreshWords(data) && RefreshCharWords(data) &&
      RefreshWordHistory(data) && RefreshURLs(data) && RefreshWordStarts(data);
}

// Private Methods -------------------------------------------------------------

bool InMemoryURLCacheDatabase::InitDatabase() {
  db_.Preload();  // Prime the cache.
  if (EnsureCurrentVersion() != sql::INIT_OK)
    return false;

  // Create the tables if any do not already exist.
  return VerifyTables() || Reset();
}

void InMemoryURLCacheDatabase::PostSequencedDBTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  BrowserThread::GetBlockingPool()->PostSequencedWorkerTask(
      sequence_token_, from_here, task);
}

void InMemoryURLCacheDatabase::ShutdownTask() {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  db_.Close();
}

sql::InitStatus InMemoryURLCacheDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return sql::INIT_FAILURE;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "In-memory URL Cache database is too new.";
    return sql::INIT_TOO_NEW;
  }

  // NOTE: Add migration code here as required.

  return sql::INIT_OK;
}

void InMemoryURLCacheDatabase::BeginTransactionTask() {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  db_.BeginTransaction();
}

void InMemoryURLCacheDatabase::CommitTransactionTask() {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (update_error_ != SQLITE_OK) {
    db_.RollbackTransaction();
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&InMemoryURLCacheDatabase::NotifyDatabaseFailure,
                   base::Unretained(this)));
    return;
  }
  db_.CommitTransaction();
}

bool InMemoryURLCacheDatabase::VerifyTables() {
  if (db_.DoesTableExist(kWordsTableName) &&
      db_.DoesTableExist(kCharWordsTableName) &&
      db_.DoesTableExist(kWordHistoryTableName) &&
      db_.DoesTableExist(kURLsTableName) &&
      db_.DoesTableExist(kURLWordStartsTableName) &&
      db_.DoesTableExist(kTitleWordStartsTableName))
    return true;
  UMA_HISTOGRAM_ENUMERATION("Sqlite.HQPCacheVerify.Error", db_.GetErrorCode(),
                            sql::kMaxSqliteError);
  return false;
}

bool InMemoryURLCacheDatabase::CreateTables() {
  std::string sql(StringPrintf(
      "CREATE TABLE %s (word_id INTEGER PRIMARY KEY, word TEXT)",
      kWordsTableName));
  if (!db_.Execute(sql.c_str()))
    return false;
  sql = StringPrintf("CREATE INDEX %s_index ON %s (word_id)",
                     kWordsTableName, kWordsTableName);
  if (!db_.Execute(sql.c_str()))
    return false;

  sql = StringPrintf("CREATE TABLE %s (char LONGCHAR, word_id INTEGER)",
                     kCharWordsTableName);
  if (!db_.Execute(sql.c_str()))
    return false;
  sql = StringPrintf("CREATE INDEX %s_index ON %s (char)",
                     kCharWordsTableName, kCharWordsTableName);
  if (!db_.Execute(sql.c_str()))
    return false;

  sql = StringPrintf("CREATE TABLE %s (word_id INTEGER, history_id INTEGER)",
                     kWordHistoryTableName);
  if (!db_.Execute(sql.c_str()))
    return false;
  sql = StringPrintf("CREATE INDEX %s_history_index ON %s (history_id)",
                     kWordHistoryTableName, kWordHistoryTableName);
  if (!db_.Execute(sql.c_str()))
    return false;

  sql = StringPrintf(
      "CREATE TABLE %s (history_id INTEGER PRIMARY KEY, url TEXT, "
      "title TEXT, visit_count INTEGER, typed_count INTEGER, "
      "last_visit_time INTEGER, hidden INTEGER)", kURLsTableName);
  if (!db_.Execute(sql.c_str()))
    return false;
  sql = StringPrintf("CREATE INDEX %s_index ON %s (history_id)",
                     kURLsTableName, kURLsTableName);
  if (!db_.Execute(sql.c_str()))
    return false;

  sql = StringPrintf("CREATE TABLE %s (history_id INTEGER, word_start INTEGER)",
                     kURLWordStartsTableName);
  if (!db_.Execute(sql.c_str()))
    return false;
  sql = StringPrintf("CREATE INDEX %s_index ON %s (history_id)",
                     kURLWordStartsTableName, kURLWordStartsTableName);
  if (!db_.Execute(sql.c_str()))
    return false;

  sql = StringPrintf("CREATE TABLE %s (history_id INTEGER, word_start INTEGER)",
                     kTitleWordStartsTableName);
  if (!db_.Execute(sql.c_str()))
    return false;
  sql = StringPrintf("CREATE INDEX %s_index ON %s (history_id)",
                     kTitleWordStartsTableName, kTitleWordStartsTableName);
  return db_.Execute(sql.c_str());
}

void InMemoryURLCacheDatabase::NotifyDatabaseFailure() {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_IN_MEMORY_URL_CACHE_DATABASE_FAILURE,
        content::Source<InMemoryURLCacheDatabase>(this),
        content::NotificationService::NoDetails());
}

bool InMemoryURLCacheDatabase::RunStatement(sql::Statement* statement) {
  if (statement->Run())
    return true;
  // If a failure has already occurred don't spew more histograms and don't
  // forget the very first failure code.
  if (update_error_ != SQLITE_OK)
    return false;
  update_error_ = db_.GetErrorCode();
  UMA_HISTOGRAM_ENUMERATION("Sqlite.HQPCacheUpdate.Error", update_error_,
                            sql::kMaxSqliteError);
  return false;
}

// Database Additions ----------------------------------------------------------

void InMemoryURLCacheDatabase::AddHistoryToURLsTask(
    history::HistoryID history_id,
    const history::URLRow& row) {
  std::string sql(StringPrintf(
      "INSERT INTO %s (history_id, url, title, visit_count, typed_count, "
      "last_visit_time, hidden) VALUES (?,?,?,?,?,?,?)",
      kURLsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  statement.BindInt(0, history_id);
  statement.BindString(1, URLDatabase::GURLToDatabaseURL(row.url()));
  statement.BindString16(2, row.title());
  statement.BindInt(3, row.visit_count());
  statement.BindInt(4, row.typed_count());
  statement.BindInt64(5, row.last_visit().ToInternalValue());
  statement.BindBool(6, row.hidden());
  RunStatement(&statement);
}

void InMemoryURLCacheDatabase::AddHistoryToWordHistoryTask(
    WordID word_id,
    HistoryID history_id) {
  std::string sql(StringPrintf(
      "INSERT INTO %s (word_id, history_id) VALUES (?,?)",
      kWordHistoryTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  statement.BindInt(0, word_id);
  statement.BindInt(1, history_id);
  RunStatement(&statement);
}

void InMemoryURLCacheDatabase::AddWordToWordsTask(WordID word_id,
                                                  const string16& uni_word) {
  std::string sql(StringPrintf(
      "INSERT INTO %s (word_id, word) VALUES (?,?)",
      kWordsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  statement.BindInt(0, word_id);
  statement.BindString16(1, uni_word);
  RunStatement(&statement);
}

void InMemoryURLCacheDatabase::AddWordToCharWordsTask(char16 uni_char,
                                                      WordID word_id) {
  std::string sql(StringPrintf("INSERT INTO %s (char, word_id) VALUES (?,?)",
                               kCharWordsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  statement.BindInt(0, uni_char);
  statement.BindInt(1, word_id);
  RunStatement(&statement);
}

void InMemoryURLCacheDatabase::AddRowWordStartsTask(
    HistoryID history_id,
    const RowWordStarts& row_word_starts) {
  std::string sql_1(StringPrintf(
      "INSERT INTO %s (history_id, word_start) VALUES (?,?)",
      kURLWordStartsTableName));
  sql::Statement url_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                      sql_1.c_str()));
  for (WordStarts::const_iterator i = row_word_starts.url_word_starts_.begin();
       i != row_word_starts.url_word_starts_.end(); ++i) {
    url_statement.Reset(true);
    url_statement.BindInt(0, history_id);
    url_statement.BindInt(1, *i);
    RunStatement(&url_statement);
  }
  std::string sql_2(StringPrintf(
      "INSERT INTO %s (history_id, word_start) VALUES (?,?)",
      kTitleWordStartsTableName));
  sql::Statement title_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                        sql_2.c_str()));
  for (WordStarts::const_iterator i =
          row_word_starts.title_word_starts_.begin();
       i != row_word_starts.title_word_starts_.end(); ++i) {
    title_statement.Reset(true);
    title_statement.BindInt(0, history_id);
    title_statement.BindInt(1, *i);
    RunStatement(&title_statement);
  }
}

// Database Removals -----------------------------------------------------------

void InMemoryURLCacheDatabase::RemoveHistoryIDFromURLsTask(
    HistoryID history_id) {
  std::string sql(StringPrintf("DELETE FROM %s WHERE history_id = ?",
                               kURLsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  statement.BindInt(0, history_id);
  RunStatement(&statement);
}

void InMemoryURLCacheDatabase::RemoveHistoryIDFromWordHistoryTask(
    HistoryID history_id) {
  std::string sql(StringPrintf("DELETE FROM %s WHERE history_id = ?",
                               kWordHistoryTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  statement.BindInt(0, history_id);
  RunStatement(&statement);
}

void InMemoryURLCacheDatabase::RemoveWordFromWordsTask(WordID word_id) {
  std::string sql_1(StringPrintf("DELETE FROM %s WHERE word_id = ?",
                                 kWordsTableName));
  sql::Statement statement_1(db_.GetCachedStatement(SQL_FROM_HERE,
                                                    sql_1.c_str()));
  statement_1.BindInt(0, word_id);
  RunStatement(&statement_1);

  std::string sql_2(StringPrintf("DELETE FROM %s WHERE word_id = ?",
                                 kCharWordsTableName));
  sql::Statement statement_2(db_.GetCachedStatement(SQL_FROM_HERE,
                                                    sql_2.c_str()));
  statement_2.BindInt(0, word_id);
  RunStatement(&statement_2);
}

void InMemoryURLCacheDatabase::RemoveWordStartsTask(HistoryID history_id) {
  std::string url_sql(StringPrintf("DELETE FROM %s WHERE history_id = ?",
                                   kURLWordStartsTableName));
  sql::Statement url_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                      url_sql.c_str()));
  url_statement.BindInt(0, history_id);
  RunStatement(&url_statement);

  std::string title_sql(StringPrintf("DELETE FROM %s WHERE history_id = ?",
                                     kTitleWordStartsTableName));
  sql::Statement title_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                        title_sql.c_str()));
  title_statement.BindInt(0, history_id);
  RunStatement(&title_statement);
}

// Database Refreshing ---------------------------------------------------------

bool InMemoryURLCacheDatabase::RefreshWords(const URLIndexPrivateData& data) {
  // Populate the  words table from the contents of the word_map_. This will
  // allow the restoration of the word_map_ as well as the word_list_ and
  // available_words_.
  std::string sql(StringPrintf("INSERT INTO %s (word_id, word) VALUES (?,?)",
                               kWordsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  for (WordMap::const_iterator i = data.word_map_.begin();
       i != data.word_map_.end(); ++i) {
    statement.Reset(true);
    statement.BindInt(0, i->second);
    statement.BindString16(1, i->first);
    if (!RunStatement(&statement))
      return false;
  }
  return true;
}

bool InMemoryURLCacheDatabase::RefreshCharWords(
    const URLIndexPrivateData& data) {
  std::string sql(StringPrintf("INSERT INTO %s (char, word_id) VALUES (?,?)",
                               kCharWordsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  for (CharWordIDMap::const_iterator i = data.char_word_map_.begin();
       i != data.char_word_map_.end(); ++i) {
    for (WordIDSet::const_iterator j = i->second.begin(); j != i->second.end();
         ++j) {
      statement.Reset(true);
      statement.BindInt(0, i->first);
      statement.BindInt(1, *j);
      if (!RunStatement(&statement))
        return false;
    }
  }
  return true;
}

bool InMemoryURLCacheDatabase::RefreshWordHistory(
    const URLIndexPrivateData& data) {
  // Populate the  word_history table from the contents of the
  // word_id_history_map_. This will allow the restoration of the
  // word_id_history_map_ as well as the history_id_word_map_.
  std::string sql(StringPrintf(
      "INSERT INTO %s (word_id, history_id) VALUES (?,?)",
      kWordHistoryTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  for (WordIDHistoryMap::const_iterator i = data.word_id_history_map_.begin();
       i != data.word_id_history_map_.end(); ++i) {
    for (HistoryIDSet::const_iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      statement.Reset(true);
      statement.BindInt(0, i->first);
      statement.BindInt64(1, *j);
      if (!RunStatement(&statement))
        return false;
    }
  }
  return true;
}

bool InMemoryURLCacheDatabase::RefreshURLs(const URLIndexPrivateData& data) {
  std::string sql(StringPrintf(
      "INSERT INTO %s (history_id, url, title, visit_count, typed_count, "
      "last_visit_time, hidden) VALUES (?,?,?,?,?,?,?)",
      kURLsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  for (HistoryInfoMap::const_iterator i = data.history_info_map_.begin();
       i != data.history_info_map_.end(); ++i) {
    statement.Reset(true);
    statement.BindInt(0, i->first);
    statement.BindString(1, URLDatabase::GURLToDatabaseURL(i->second.url()));
    statement.BindString16(2, i->second.title());
    statement.BindInt(3, i->second.visit_count());
    statement.BindInt(4, i->second.typed_count());
    statement.BindInt64(5, i->second.last_visit().ToInternalValue());
    statement.BindBool(6, i->second.hidden());
    if (!RunStatement(&statement))
      return false;
  }
  return true;
}

bool InMemoryURLCacheDatabase::RefreshWordStarts(
    const URLIndexPrivateData& data) {
  std::string url_sql(StringPrintf(
      "INSERT INTO %s (history_id, word_start) VALUES (?,?)",
      kURLWordStartsTableName));
  sql::Statement url_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                      url_sql.c_str()));
  std::string title_sql(StringPrintf(
      "INSERT INTO %s (history_id, word_start) VALUES (?,?)",
      kTitleWordStartsTableName));
  sql::Statement title_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                        title_sql.c_str()));
  for (WordStartsMap::const_iterator i = data.word_starts_map_.begin();
       i != data.word_starts_map_.end(); ++i) {
    for (WordStarts::const_iterator w = i->second.url_word_starts_.begin();
         w != i->second.url_word_starts_.end(); ++w) {
      url_statement.Reset(true);
      url_statement.BindInt(0, i->first);
      url_statement.BindInt(1, *w);
      if (!RunStatement(&url_statement))
        return false;
    }
    for (WordStarts::const_iterator w = i->second.title_word_starts_.begin();
         w != i->second.title_word_starts_.end(); ++w) {
      title_statement.Reset(true);
      title_statement.BindInt(0, i->first);
      title_statement.BindInt(1, *w);
      if (!RunStatement(&title_statement))
        return false;
    }
  }
  return true;
}

// Database Restoration --------------------------------------------------------

bool InMemoryURLCacheDatabase::RestoreWords(URLIndexPrivateData* data) {
  // Rebuild word_list_, word_map_ and available_words_.
  std::string sql(StringPrintf("SELECT word_id, word FROM %s ORDER BY word_id",
                               kWordsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  while (statement.Step()) {
    WordID word_id = statement.ColumnInt(0);
    // A gap in contiguous word IDs indicates word slots in the vector that are
    // currently unused and available for future use as new words are indexed.
    while (data->word_list_.size() < word_id) {
      data->available_words_.insert(data->word_list_.size());
      data->word_list_.push_back(string16());
    }
    string16 word = statement.ColumnString16(1);
    DCHECK_EQ(static_cast<size_t>(word_id), data->word_list_.size());
    data->word_list_.push_back(word);
    data->word_map_[word] = word_id;
  }
  return statement.Succeeded() && !data->word_list_.empty();
}

bool InMemoryURLCacheDatabase::RestoreCharWords(URLIndexPrivateData* data) {
  // Rebuild char_word_map_.
  std::string sql(StringPrintf("SELECT char, word_id FROM %s ORDER BY char",
                               kCharWordsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  CharWordIDMap& char_map(data->char_word_map_);
  CharWordIDMap::iterator it = char_map.begin();
  while (statement.Step()) {
    char16 next_char = statement.ColumnInt(0);
    if (it == char_map.begin() || it->first != next_char)
      it = char_map.insert(it, std::make_pair(next_char, WordIDSet()));
    it->second.insert(statement.ColumnInt(1));
  }
  return statement.Succeeded() && !char_map.empty();
}

bool InMemoryURLCacheDatabase::RestoreWordHistory(URLIndexPrivateData* data) {
  // Rebuild word_id_history_map_ and history_id_word_map_.
  std::string sql(StringPrintf(
      "SELECT word_id, history_id FROM %s ORDER BY word_id",
      kWordHistoryTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  WordIDHistoryMap& word_map(data->word_id_history_map_);
  WordIDHistoryMap::iterator it = word_map.begin();
  while (statement.Step()) {
    WordID word_id = statement.ColumnInt(0);
    HistoryID history_id = statement.ColumnInt(1);
    data->AddToHistoryIDWordMap(history_id, word_id);
    if (it == word_map.begin() || it->first != word_id)
      it = word_map.insert(it, std::make_pair(word_id, HistoryIDSet()));
    it->second.insert(history_id);
  }
  return statement.Succeeded() && !word_map.empty();
}

bool InMemoryURLCacheDatabase::RestoreURLs(URLIndexPrivateData* data) {
  // Rebuild history_info_map_.
  std::string sql(StringPrintf(
      "SELECT history_id, url, title, visit_count, typed_count, "
      "last_visit_time, hidden FROM %s",
      kURLsTableName));
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, sql.c_str()));
  while (statement.Step()) {
    HistoryID history_id = statement.ColumnInt64(0);
    URLRow row(GURL(statement.ColumnString(1)), history_id);
    row.set_title(statement.ColumnString16(2));
    row.set_visit_count(statement.ColumnInt(3));
    row.set_typed_count(statement.ColumnInt(4));
    row.set_last_visit(base::Time::FromInternalValue(statement.ColumnInt64(5)));
    row.set_hidden(statement.ColumnInt(6) != 0);
    data->history_info_map_[history_id] = row;
  }
  return statement.Succeeded() && !data->history_info_map_.empty();
}

bool InMemoryURLCacheDatabase::RestoreWordStarts(URLIndexPrivateData* data) {
  std::string url_sql(StringPrintf(
      "SELECT history_id, word_start FROM %s ORDER BY word_start",
      kURLWordStartsTableName));
  sql::Statement url_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                      url_sql.c_str()));
  while (url_statement.Step()) {
    HistoryID history_id = url_statement.ColumnInt64(0);
    size_t word_start = url_statement.ColumnInt64(1);
    data->word_starts_map_[history_id].url_word_starts_.push_back(word_start);
  }
  if (!url_statement.Succeeded())
    return false;

  std::string title_sql(StringPrintf(
      "SELECT history_id, word_start FROM %s ORDER BY word_start",
      kTitleWordStartsTableName));
  sql::Statement title_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                        title_sql.c_str()));
  while (title_statement.Step()) {
    HistoryID history_id = title_statement.ColumnInt64(0);
    size_t word_start = title_statement.ColumnInt64(1);
    data->word_starts_map_[history_id].title_word_starts_.push_back(word_start);
  }

  return title_statement.Succeeded() && !data->word_starts_map_.empty();
}

}  // namespace history
