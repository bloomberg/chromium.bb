// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_sqlite.h"

#include "base/file_util.h"
#include "chrome/common/sqlite_compiled_statement.h"
#include "chrome/common/sqlite_utils.h"

namespace {

// Database version.  If this is different than what's stored on disk, the
// database is reset.
const int kDatabaseVersion = 6;

// Used for reading full hashes from the database.
SBFullHash ReadFullHash(SqliteCompiledStatement* statement, int column) {
  std::vector<unsigned char> blob;
  (*statement)->column_blob_as_vector(column, &blob);

  SBFullHash ret;
  DCHECK_EQ(blob.size(), sizeof(ret));
  memcpy(ret.full_hash, &blob[0], sizeof(ret));
  return ret;
}

void DeleteChunksFromSet(const base::hash_set<int32>& deleted,
                         std::set<int32>* chunks) {
  for (std::set<int32>::iterator iter = chunks->begin();
       iter != chunks->end();) {
    std::set<int32>::iterator prev = iter++;
    if (deleted.count(*prev) > 0)
      chunks->erase(prev);
  }
}

}  // namespace

SafeBrowsingStoreSqlite::SafeBrowsingStoreSqlite()
    : db_(NULL),
      statement_cache_(NULL),
      insert_transaction_(NULL) {
}
SafeBrowsingStoreSqlite::~SafeBrowsingStoreSqlite() {
  Close();
}

bool SafeBrowsingStoreSqlite::Delete() {
  // The database should not be open at this point.  TODO(shess): It
  // can be open if corruption was detected while opening the
  // database.  Ick.
  DCHECK(!db_);

  // The file must be closed, both so that the journal file is deleted
  // by SQLite, and because open files cannot be deleted on Windows.
  if (!Close()) {
    NOTREACHED();
    return false;
  }

  // Just in case, delete the journal file, because associating the
  // wrong journal file with a database is very bad.
  const FilePath journal_file = JournalFileForFilename(filename_);
  if (!file_util::Delete(journal_file, false) &&
      file_util::PathExists(journal_file)) {
    NOTREACHED();
    return false;
  }

  if (!file_util::Delete(filename_, false) &&
      file_util::PathExists(filename_)) {
    NOTREACHED();
    return false;
  }

  return true;
}

void SafeBrowsingStoreSqlite::Init(const FilePath& filename,
                                   Callback0::Type* corruption_callback) {
  filename_ = filename;
  corruption_callback_.reset(corruption_callback);
}

bool SafeBrowsingStoreSqlite::OnCorruptDatabase() {
  if (corruption_callback_.get())
    corruption_callback_->Run();
  return false;
}

bool SafeBrowsingStoreSqlite::Open() {
  if (db_)
    return true;

  if (OpenSqliteDb(filename_, &db_) != SQLITE_OK) {
    sqlite3_close(db_);
    db_ = NULL;
    return false;
  }

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  ExecSql("PRAGMA locking_mode = EXCLUSIVE");
  ExecSql("PRAGMA cache_size = 100");

  statement_cache_.reset(new SqliteStatementCache(db_));

  if (!DoesSqliteTableExist(db_, "add_prefix"))
    return SetupDatabase();

  return CheckCompatibleVersion();
}

bool SafeBrowsingStoreSqlite::ExecSql(const char* sql) {
  DCHECK(db_);

  int rv = sqlite3_exec(db_, sql, NULL, NULL, NULL);
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK(rv == SQLITE_OK);
  return true;
}

bool SafeBrowsingStoreSqlite::Close() {
  if (!db_)
    return true;

  add_chunks_cache_.clear();
  sub_chunks_cache_.clear();

  add_del_cache_.clear();
  sub_del_cache_.clear();

  insert_transaction_.reset();
  statement_cache_.reset();  // Must free statements before closing DB.
  bool result = sqlite3_close(db_) == SQLITE_OK;
  db_ = NULL;

  return result;
}

bool SafeBrowsingStoreSqlite::CreateTables() {
  DCHECK(db_);

  // Store 32 bit add prefixes here.
  if (!ExecSql("CREATE TABLE add_prefix ("
               "  chunk INTEGER,"
               "  prefix INTEGER"
               ")"))
    return false;

  // Store 32 bit sub prefixes here.
  if (!ExecSql("CREATE TABLE sub_prefix ("
               "  chunk INTEGER,"
               "  add_chunk INTEGER,"
               "  prefix INTEGER"
               ")"))
    return false;

  // Store 256 bit add full hashes (and GetHash results) here.
  if (!ExecSql("CREATE TABLE add_full_hash ("
               "  chunk INTEGER,"
               "  prefix INTEGER,"
               "  receive_time INTEGER,"
               "  full_hash BLOB"
               ")"))
    return false;

  // Store 256 bit sub full hashes here.
  if (!ExecSql("CREATE TABLE sub_full_hash ("
               "  chunk INTEGER,"
               "  add_chunk INTEGER,"
               "  prefix INTEGER,"
               "  full_hash BLOB"
               ")"))
    return false;

  // Store all the add and sub chunk numbers we receive. We cannot
  // just rely on the prefix tables to generate these lists, since
  // some chunks will have zero entries (and thus no prefixes), or
  // potentially an add chunk can have all of its entries sub'd
  // without receiving an AddDel, or a sub chunk might have been
  // entirely consumed by adds. In these cases, we still have to
  // report the chunk number but it will not have any prefixes in the
  // prefix tables.
  //
  // TODO(paulg): Investigate storing the chunks as a string of
  // ChunkRanges, one string for each of phish-add, phish-sub,
  // malware-add, malware-sub. This might be better performance when
  // the number of chunks is large, and is the natural format for the
  // update request.
  if (!ExecSql("CREATE TABLE add_chunks ("
               "  chunk INTEGER PRIMARY KEY"
               ")"))
    return false;

  if (!ExecSql("CREATE TABLE sub_chunks ("
               "  chunk INTEGER PRIMARY KEY"
               ")"))
    return false;

  return true;
}

bool SafeBrowsingStoreSqlite::SetupDatabase() {
  DCHECK(db_);

  SQLTransaction transaction(db_);
  if (transaction.Begin() != SQLITE_OK) {
    NOTREACHED();
    return false;
  }

  if (!CreateTables())
    return false;

  // PRAGMA does not support bind parameters...
  const std::string version =
      StringPrintf("PRAGMA user_version = %d", kDatabaseVersion);
  if (!ExecSql(version.c_str()))
    return false;

  if (transaction.Commit() != SQLITE_OK)
    return false;

  return true;
}

bool SafeBrowsingStoreSqlite::CheckCompatibleVersion() {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "PRAGMA user_version");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  int result = statement->step();
  if (result != SQLITE_ROW)
    return false;

  return statement->column_int(0) == kDatabaseVersion;
}

bool SafeBrowsingStoreSqlite::ReadAddChunks() {
  DCHECK(db_);

  add_chunks_cache_.clear();

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "SELECT chunk FROM add_chunks");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv;
  while ((rv = statement->step()) == SQLITE_ROW) {
    add_chunks_cache_.insert(statement->column_int(0));
  }
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK_EQ(rv, SQLITE_DONE);
  return rv == SQLITE_DONE;
}

bool SafeBrowsingStoreSqlite::WriteAddChunks() {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "INSERT INTO add_chunks (chunk) VALUES (?)");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  for (std::set<int32>::const_iterator iter = add_chunks_cache_.begin();
       iter != add_chunks_cache_.end(); ++iter) {
    statement->bind_int(0, *iter);
    int rv = statement->step();
    if (rv == SQLITE_CORRUPT)
      return OnCorruptDatabase();
    DCHECK(rv == SQLITE_DONE);
    statement->reset();
  }
  return true;
}

bool SafeBrowsingStoreSqlite::ReadSubChunks() {
  DCHECK(db_);

  sub_chunks_cache_.clear();

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "SELECT chunk FROM sub_chunks");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv;
  while ((rv = statement->step()) == SQLITE_ROW) {
    sub_chunks_cache_.insert(statement->column_int(0));
  }
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  return rv == SQLITE_DONE;
}

bool SafeBrowsingStoreSqlite::WriteSubChunks() {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "INSERT INTO sub_chunks (chunk) VALUES (?)");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  for (std::set<int32>::const_iterator iter = sub_chunks_cache_.begin();
       iter != sub_chunks_cache_.end(); ++iter) {
    statement->bind_int(0, *iter);
    int rv = statement->step();
    if (rv == SQLITE_CORRUPT)
      return OnCorruptDatabase();
    DCHECK(rv == SQLITE_DONE);
    statement->reset();
  }
  return true;
}

bool SafeBrowsingStoreSqlite::ReadAddPrefixes(
    std::vector<SBAddPrefix>* add_prefixes) {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "SELECT chunk, prefix FROM add_prefix");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv;
  while ((rv = statement->step()) == SQLITE_ROW) {
    const int32 chunk_id = statement->column_int(0);
    if (add_del_cache_.count(chunk_id) > 0)
      continue;

    const SBPrefix prefix = statement->column_int(1);
    add_prefixes->push_back(SBAddPrefix(chunk_id, prefix));
  }
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK_EQ(rv, SQLITE_DONE);
  return true;
}

bool SafeBrowsingStoreSqlite::WriteAddPrefix(int32 chunk_id, SBPrefix prefix) {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "INSERT INTO add_prefix "
                          "(chunk, prefix) VALUES (?,?)");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  statement->bind_int(0, chunk_id);
  statement->bind_int(1, prefix);
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK(rv == SQLITE_DONE);
  return true;
}

bool SafeBrowsingStoreSqlite::WriteAddPrefixes(
    const std::vector<SBAddPrefix>& add_prefixes) {
  DCHECK(db_);

  for (std::vector<SBAddPrefix>::const_iterator iter = add_prefixes.begin();
       iter != add_prefixes.end(); ++iter) {
    if (!WriteAddPrefix(iter->chunk_id, iter->prefix))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreSqlite::ReadSubPrefixes(
    std::vector<SBSubPrefix>* sub_prefixes) {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "SELECT chunk, add_chunk, prefix "
                          "FROM sub_prefix");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv;
  while ((rv = statement->step()) == SQLITE_ROW) {
    const int32 chunk_id = statement->column_int(0);
    if (sub_del_cache_.count(chunk_id) > 0)
      continue;

    const int32 add_chunk_id = statement->column_int(1);
    const SBPrefix add_prefix = statement->column_int(2);
    sub_prefixes->push_back(SBSubPrefix(chunk_id, add_chunk_id, add_prefix));
  }
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK_EQ(rv, SQLITE_DONE);
  return true;
}

bool SafeBrowsingStoreSqlite::WriteSubPrefix(
    int32 chunk_id, int32 add_chunk_id, SBPrefix prefix) {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "INSERT INTO sub_prefix "
                          "(chunk, add_chunk, prefix) VALUES (?,?, ?)");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  statement->bind_int(0, chunk_id);
  statement->bind_int(1, add_chunk_id);
  statement->bind_int(2, prefix);
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK(rv == SQLITE_DONE);
  return true;
}

bool SafeBrowsingStoreSqlite::WriteSubPrefixes(
    std::vector<SBSubPrefix>& sub_prefixes) {
  DCHECK(db_);

  for (std::vector<SBSubPrefix>::const_iterator iter = sub_prefixes.begin();
       iter != sub_prefixes.end(); ++iter) {
    const SBAddPrefix &add_prefix = iter->add_prefix;
    if (!WriteSubPrefix(iter->chunk_id, add_prefix.chunk_id, add_prefix.prefix))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreSqlite::ReadAddHashes(
    std::vector<SBAddFullHash>* add_hashes) {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "SELECT chunk, prefix, receive_time, full_hash "
                          "FROM add_full_hash");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv;
  while ((rv = statement->step()) == SQLITE_ROW) {
    const int32 chunk_id = statement->column_int(0);
    if (add_del_cache_.count(chunk_id) > 0)
      continue;

    const SBPrefix prefix = statement->column_int(1);
    const base::Time received =
        base::Time::FromTimeT(statement->column_int64(2));
    const SBFullHash full_hash = ReadFullHash(&statement, 3);
    add_hashes->push_back(SBAddFullHash(chunk_id, prefix, received, full_hash));
  }
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK_EQ(rv, SQLITE_DONE);
  return true;
}

bool SafeBrowsingStoreSqlite::WriteAddHash(int32 chunk_id, SBPrefix prefix,
                                           base::Time receive_time,
                                           SBFullHash full_hash) {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "INSERT INTO add_full_hash "
                          "(chunk, prefix, receive_time, full_hash) "
                          "VALUES (?,?, ?, ?)");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  statement->bind_int(0, chunk_id);
  statement->bind_int(1, prefix);
  statement->bind_int64(2, receive_time.ToTimeT());
  statement->bind_blob(3, full_hash.full_hash, sizeof(full_hash.full_hash));
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK(rv == SQLITE_DONE);
  return true;
}

bool SafeBrowsingStoreSqlite::WriteAddHashes(
    const std::vector<SBAddFullHash>& add_hashes) {
  DCHECK(db_);

  for (std::vector<SBAddFullHash>::const_iterator iter = add_hashes.begin();
       iter != add_hashes.end(); ++iter) {
    const SBAddPrefix& add_prefix = iter->add_prefix;
    if (!WriteAddHash(add_prefix.chunk_id, add_prefix.prefix,
                      iter->received, iter->full_hash))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreSqlite::ReadSubHashes(
    std::vector<SBSubFullHash>* sub_hashes) {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "SELECT chunk, add_chunk, prefix, full_hash "
                          "FROM sub_full_hash");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  int rv;
  while ((rv = statement->step()) == SQLITE_ROW) {
    const int32 chunk_id = statement->column_int(0);
    if (sub_del_cache_.count(chunk_id) > 0)
      continue;

    const int32 add_chunk_id = statement->column_int(1);
    const SBPrefix add_prefix = statement->column_int(2);
    const SBFullHash full_hash = ReadFullHash(&statement, 3);
    sub_hashes->push_back(
        SBSubFullHash(chunk_id, add_chunk_id, add_prefix, full_hash));
  }
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK_EQ(rv, SQLITE_DONE);
  return true;
}

bool SafeBrowsingStoreSqlite::WriteSubHash(
    int32 chunk_id, int32 add_chunk_id, SBPrefix prefix, SBFullHash full_hash) {
  DCHECK(db_);

  SQLITE_UNIQUE_STATEMENT(statement, *statement_cache_,
                          "INSERT INTO sub_full_hash "
                          "(chunk, add_chunk, prefix, full_hash) "
                          "VALUES (?,?,?,?)");
  if (!statement.is_valid()) {
    NOTREACHED();
    return false;
  }

  statement->bind_int(0, chunk_id);
  statement->bind_int(1, add_chunk_id);
  statement->bind_int(2, prefix);
  statement->bind_blob(3, full_hash.full_hash, sizeof(full_hash.full_hash));
  int rv = statement->step();
  if (rv == SQLITE_CORRUPT)
    return OnCorruptDatabase();
  DCHECK(rv == SQLITE_DONE);
  return true;
}

bool SafeBrowsingStoreSqlite::WriteSubHashes(
    std::vector<SBSubFullHash>& sub_hashes) {
  DCHECK(db_);

  for (std::vector<SBSubFullHash>::const_iterator iter = sub_hashes.begin();
       iter != sub_hashes.end(); ++iter) {
    if (!WriteSubHash(iter->chunk_id, iter->add_prefix.chunk_id,
                      iter->add_prefix.prefix, iter->full_hash))
      return false;
  }
  return true;
}

bool SafeBrowsingStoreSqlite::RenameTables() {
  DCHECK(db_);

  if (!ExecSql("ALTER TABLE add_prefix RENAME TO add_prefix_old") ||
      !ExecSql("ALTER TABLE sub_prefix RENAME TO sub_prefix_old") ||
      !ExecSql("ALTER TABLE add_full_hash RENAME TO add_full_hash_old") ||
      !ExecSql("ALTER TABLE sub_full_hash RENAME TO sub_full_hash_old") ||
      !ExecSql("ALTER TABLE add_chunks RENAME TO add_chunks_old") ||
      !ExecSql("ALTER TABLE sub_chunks RENAME TO sub_chunks_old"))
    return false;

  return CreateTables();
}

bool SafeBrowsingStoreSqlite::DeleteOldTables() {
  DCHECK(db_);

  if (!ExecSql("DROP TABLE add_prefix_old") ||
      !ExecSql("DROP TABLE sub_prefix_old") ||
      !ExecSql("DROP TABLE add_full_hash_old") ||
      !ExecSql("DROP TABLE sub_full_hash_old") ||
      !ExecSql("DROP TABLE add_chunks_old") ||
      !ExecSql("DROP TABLE sub_chunks_old"))
    return false;

  return true;
}

bool SafeBrowsingStoreSqlite::BeginUpdate() {
  DCHECK(!db_);

  if (!Open())
    return false;

  insert_transaction_.reset(new SQLTransaction(db_));
  if (insert_transaction_->Begin() != SQLITE_OK) {
    DCHECK(false) << "Safe browsing store couldn't start transaction";
    Close();
    return false;
  }

  if (!ReadAddChunks() || !ReadSubChunks())
    return false;

  return true;
}

bool SafeBrowsingStoreSqlite::DoUpdate(
    const std::vector<SBAddFullHash>& pending_adds,
    std::vector<SBAddPrefix>* add_prefixes_result,
    std::vector<SBAddFullHash>* add_full_hashes_result) {
  DCHECK(db_);

  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBAddFullHash> add_full_hashes;
  std::vector<SBSubPrefix> sub_prefixes;
  std::vector<SBSubFullHash> sub_full_hashes;

  if (!ReadAddPrefixes(&add_prefixes) ||
      !ReadAddHashes(&add_full_hashes) ||
      !ReadSubPrefixes(&sub_prefixes) ||
      !ReadSubHashes(&sub_full_hashes))
    return false;

  // Add the pending adds which haven't since been deleted.
  for (std::vector<SBAddFullHash>::const_iterator iter = pending_adds.begin();
       iter != pending_adds.end(); ++iter) {
    if (add_del_cache_.count(iter->add_prefix.chunk_id) == 0)
      add_full_hashes.push_back(*iter);
  }

  SBProcessSubs(&add_prefixes, &sub_prefixes,
                &add_full_hashes, &sub_full_hashes);

  // Move the existing tables aside and prepare to write fresh tables.
  if (!RenameTables())
    return false;

  DeleteChunksFromSet(add_del_cache_, &add_chunks_cache_);
  DeleteChunksFromSet(sub_del_cache_, &sub_chunks_cache_);

  if (!WriteAddChunks() ||
      !WriteSubChunks() ||
      !WriteAddPrefixes(add_prefixes) ||
      !WriteSubPrefixes(sub_prefixes) ||
      !WriteAddHashes(add_full_hashes) ||
      !WriteSubHashes(sub_full_hashes))
    return false;

  // Delete the old tables.
  if (!DeleteOldTables())
    return false;

  // Commit all the changes to the database.
  int rv = insert_transaction_->Commit();
  if (rv != SQLITE_OK) {
    NOTREACHED() << "SafeBrowsing update transaction failed to commit.";
    // UMA_HISTOGRAM_COUNTS("SB2.FailedUpdate", 1);
    return false;
  }

  add_prefixes_result->swap(add_prefixes);
  add_full_hashes_result->swap(add_full_hashes);

  return true;
}

bool SafeBrowsingStoreSqlite::FinishUpdate(
    const std::vector<SBAddFullHash>& pending_adds,
    std::vector<SBAddPrefix>* add_prefixes_result,
    std::vector<SBAddFullHash>* add_full_hashes_result) {
  bool ret = DoUpdate(pending_adds,
                      add_prefixes_result, add_full_hashes_result);

  // Make sure everything is closed even if DoUpdate() fails.
  if (!Close())
    return false;

  return ret;
}

bool SafeBrowsingStoreSqlite::CancelUpdate() {
  return Close();
}
