// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/directory_backing_store.h"

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "base/hash_tables.h"
#include "base/logging.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable_columns.h"
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/browser/sync/util/query_helpers.h"
#include "chrome/common/sqlite_utils.h"
#include "third_party/sqlite/preprocessed/sqlite3.h"

// Sometimes threads contend on the DB lock itself, especially when one thread
// is calling SaveChanges.  In the worst case scenario, the user can put his
// laptop to sleep during db contention, and wake up the laptop days later, so
// infinity seems like the best choice here.
const int kDirectoryBackingStoreBusyTimeoutMs = std::numeric_limits<int>::max();

using std::string;

namespace syncable {

// This just has to be big enough to hold an UPDATE or INSERT statement that
// modifies all the columns in the entry table.
static const string::size_type kUpdateStatementBufferSize = 2048;

// Increment this version whenever updating DB tables.
static const int32 kCurrentDBVersion = 68;

static void RegisterPathNameCollate(sqlite3* dbhandle) {
  const int collate = SQLITE_UTF8;
  CHECK(SQLITE_OK == sqlite3_create_collation(dbhandle, "PATHNAME", collate,
      NULL, &ComparePathNames16));
}

static inline bool IsSqliteErrorOurFault(int result) {
  switch (result) {
    case SQLITE_MISMATCH:
    case SQLITE_CONSTRAINT:
    case SQLITE_MISUSE:
    case SQLITE_RANGE:
      return true;
    default:
      return false;
  }
}

namespace {

// This small helper class reduces the amount of code in the table upgrade code
// below and also CHECKs as soon as there's an issue.
class StatementExecutor {
 public:
  explicit StatementExecutor(sqlite3* dbhandle) : dbhandle_(dbhandle) {
    result_ = SQLITE_DONE;
  }
  int Exec(const char* query) {
    if (SQLITE_DONE != result_)
      return result_;
    result_ = ::Exec(dbhandle_, query);
    CHECK(!IsSqliteErrorOurFault(result_)) << query;
    return result_;
  }
  template <typename T1>
  int Exec(const char* query, T1 arg1) {
    if (SQLITE_DONE != result_)
      return result_;
    result_ = ::Exec(dbhandle_, query, arg1);
    CHECK(!IsSqliteErrorOurFault(result_)) << query;
    return result_;
  }
  int result() {
    return result_;
  }
  void set_result(int result) {
    result_ = result;
    CHECK(!IsSqliteErrorOurFault(result_)) << result_;
  }
  bool healthy() const {
    return SQLITE_DONE == result_;
  }
 private:
  sqlite3* dbhandle_;
  int result_;
  DISALLOW_COPY_AND_ASSIGN(StatementExecutor);
};

}  // namespace

static string GenerateCacheGUID() {
  return Generate128BitRandomHexString();
}

// Iterate over the fields of |entry| and bind each to |statement| for
// updating.  Returns the number of args bound.
static int BindFields(const EntryKernel& entry, sqlite3_stmt* statement) {
  int index = 1;
  int i = 0;
  for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
    BindArg(statement, entry.ref(static_cast<Int64Field>(i)), index++);
  }
  for ( ; i < ID_FIELDS_END; ++i) {
    BindArg(statement, entry.ref(static_cast<IdField>(i)), index++);
  }
  for ( ; i < BIT_FIELDS_END; ++i) {
    BindArg(statement, entry.ref(static_cast<BitField>(i)), index++);
  }
  for ( ; i < STRING_FIELDS_END; ++i) {
    BindArg(statement, entry.ref(static_cast<StringField>(i)), index++);
  }
  for ( ; i < BLOB_FIELDS_END; ++i) {
    BindArg(statement, entry.ref(static_cast<BlobField>(i)), index++);
  }
  return index - 1;
}

// The caller owns the returned EntryKernel*.
static EntryKernel* UnpackEntry(sqlite3_stmt* statement) {
  EntryKernel* result = NULL;
  int query_result = sqlite3_step(statement);
  if (SQLITE_ROW == query_result) {
    result = new EntryKernel;
    result->clear_dirty();
    CHECK(sqlite3_column_count(statement) == static_cast<int>(FIELD_COUNT));
    int i = 0;
    for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
      result->ref(static_cast<Int64Field>(i)) =
          sqlite3_column_int64(statement, i);
    }
    for ( ; i < ID_FIELDS_END; ++i) {
      GetColumn(statement, i, &result->ref(static_cast<IdField>(i)));
    }
    for ( ; i < BIT_FIELDS_END; ++i) {
      result->ref(static_cast<BitField>(i)) =
          (0 != sqlite3_column_int(statement, i));
    }
    for ( ; i < STRING_FIELDS_END; ++i) {
      GetColumn(statement, i, &result->ref(static_cast<StringField>(i)));
    }
    for ( ; i < BLOB_FIELDS_END; ++i) {
      GetColumn(statement, i, &result->ref(static_cast<BlobField>(i)));
    }
    ZeroFields(result, i);
  } else {
    CHECK(SQLITE_DONE == query_result);
    result = NULL;
  }
  return result;
}

static bool StepDone(sqlite3_stmt* statement, const char* failed_call) {
  int result = sqlite3_step(statement);
  if (SQLITE_DONE == result && SQLITE_OK == (result = sqlite3_reset(statement)))
    return true;
  // Some error code.
  LOG(WARNING) << failed_call << " failed with result " << result;
  CHECK(!IsSqliteErrorOurFault(result));
  return false;
}

static string ComposeCreateTableColumnSpecs(const ColumnSpec* begin,
                                            const ColumnSpec* end) {
  string query;
  query.reserve(kUpdateStatementBufferSize);
  char separator = '(';
  for (const ColumnSpec* column = begin; column != end; ++column) {
    query.push_back(separator);
    separator = ',';
    query.append(column->name);
    query.push_back(' ');
    query.append(column->spec);
  }
  query.push_back(')');
  return query;
}

///////////////////////////////////////////////////////////////////////////////
// DirectoryBackingStore implementation.

DirectoryBackingStore::DirectoryBackingStore(const string& dir_name,
                                             const FilePath& backing_filepath)
    : load_dbhandle_(NULL),
      save_dbhandle_(NULL),
      dir_name_(dir_name),
      backing_filepath_(backing_filepath) {
}

DirectoryBackingStore::~DirectoryBackingStore() {
  if (NULL != load_dbhandle_) {
    sqlite3_close(load_dbhandle_);
    load_dbhandle_ = NULL;
  }
  if (NULL != save_dbhandle_) {
    sqlite3_close(save_dbhandle_);
    save_dbhandle_ = NULL;
  }
}

bool DirectoryBackingStore::OpenAndConfigureHandleHelper(
    sqlite3** handle) const {
  if (SQLITE_OK == SqliteOpen(backing_filepath_, handle)) {
    sqlite3_busy_timeout(*handle, kDirectoryBackingStoreBusyTimeoutMs);
    RegisterPathNameCollate(*handle);

    return true;
  }
  return false;
}

DirOpenResult DirectoryBackingStore::Load(MetahandlesIndex* entry_bucket,
    ExtendedAttributes* xattrs_bucket,
    Directory::KernelLoadInfo* kernel_load_info) {
  DCHECK(load_dbhandle_ == NULL);
  if (!OpenAndConfigureHandleHelper(&load_dbhandle_))
    return FAILED_OPEN_DATABASE;

  DirOpenResult result = InitializeTables();
  if (OPENED != result)
    return result;

  DropDeletedEntries();
  LoadEntries(entry_bucket);
  LoadExtendedAttributes(xattrs_bucket);
  LoadInfo(kernel_load_info);

  sqlite3_close(load_dbhandle_);
  load_dbhandle_ = NULL;  // No longer used.

  return OPENED;
}

bool DirectoryBackingStore::SaveChanges(
    const Directory::SaveChangesSnapshot& snapshot) {
  bool disk_full = false;
  sqlite3* dbhandle = LazyGetSaveHandle();
  {
    {
      ScopedStatement begin(PrepareQuery(dbhandle,
                                         "BEGIN EXCLUSIVE TRANSACTION"));
      if (!StepDone(begin.get(), "BEGIN")) {
        disk_full = true;
        goto DoneDBTransaction;
      }
    }

    for (OriginalEntries::const_iterator i = snapshot.dirty_metas.begin();
         !disk_full && i != snapshot.dirty_metas.end(); ++i) {
      DCHECK(i->is_dirty());
      disk_full = !SaveEntryToDB(*i);
    }

    for (ExtendedAttributes::const_iterator i = snapshot.dirty_xattrs.begin();
         !disk_full && i != snapshot.dirty_xattrs.end(); ++i) {
      DCHECK(i->second.dirty);
      if (i->second.is_deleted) {
        disk_full = !DeleteExtendedAttributeFromDB(i);
      } else {
        disk_full = !SaveExtendedAttributeToDB(i);
      }
    }

    if (!disk_full && (Directory::KERNEL_SHARE_INFO_DIRTY ==
                       snapshot.kernel_info_status)) {
      const Directory::PersistedKernelInfo& info = snapshot.kernel_info;
      ScopedStatement update(PrepareQuery(dbhandle, "UPDATE share_info "
          "SET last_sync_timestamp = ?, initial_sync_ended = ?, "
          "store_birthday = ?, "
          "next_id = ?",
          info.last_sync_timestamp,
          info.initial_sync_ended,
          info.store_birthday,
          info.next_id));
      disk_full = !(StepDone(update.get(), "UPDATE share_info")
                    && 1 == sqlite3_changes(dbhandle));
    }
    if (disk_full) {
      ExecOrDie(dbhandle, "ROLLBACK TRANSACTION");
    } else {
      ScopedStatement end_transaction(PrepareQuery(dbhandle,
                                                   "COMMIT TRANSACTION"));
      disk_full = !StepDone(end_transaction.get(), "COMMIT TRANSACTION");
    }
  }

 DoneDBTransaction:
  return !disk_full;
}

DirOpenResult DirectoryBackingStore::InitializeTables() {
  SQLTransaction transaction(load_dbhandle_);
  if (SQLITE_OK != transaction.BeginExclusive()) {
    return FAILED_DISK_FULL;
  }
  int version_on_disk = 0;
  int last_result = SQLITE_OK;

  if (DoesSqliteTableExist(load_dbhandle_, "share_version")) {
    SQLStatement version_query;
    version_query.prepare(load_dbhandle_, "SELECT data from share_version");
    last_result = version_query.step();
    if (SQLITE_ROW == last_result) {
      version_on_disk = version_query.column_int(0);
    }
    last_result = version_query.reset();
  }
  if (version_on_disk != kCurrentDBVersion) {
    if (version_on_disk > kCurrentDBVersion) {
      transaction.Rollback();
      return FAILED_NEWER_VERSION;
    }
    LOG(INFO) << "Old/null sync database, version " << version_on_disk;
    // Delete the existing database (if any), and create a freshone.
    if (SQLITE_OK == last_result) {
      DropAllTables();
      if (SQLITE_DONE == CreateTables()) {
        last_result = SQLITE_OK;
      }
    }
  }
  if (SQLITE_OK == last_result) {
    {
      SQLStatement statement;
      statement.prepare(load_dbhandle_,
          "SELECT db_create_version, db_create_time FROM share_info");
      if (SQLITE_ROW != statement.step()) {
        transaction.Rollback();
        return FAILED_DISK_FULL;
      }
      string db_create_version = statement.column_text(0);
      int db_create_time = statement.column_int(1);
      statement.reset();
      LOG(INFO) << "DB created at " << db_create_time << " by version " <<
          db_create_version;
    }
    // COMMIT TRANSACTION rolls back on failure.
    if (SQLITE_OK == transaction.Commit())
      return OPENED;
  } else {
    transaction.Rollback();
  }
  return FAILED_DISK_FULL;
}

void DirectoryBackingStore::LoadEntries(MetahandlesIndex* entry_bucket) {
  string select;
  select.reserve(kUpdateStatementBufferSize);
  select.append("SELECT");
  const char* joiner = " ";
  // Be explicit in SELECT order to match up with UnpackEntry.
  for (int i = BEGIN_FIELDS; i < BEGIN_FIELDS + FIELD_COUNT; ++i) {
    select.append(joiner);
    select.append(ColumnName(i));
    joiner = ", ";
  }
  select.append(" FROM metas ");
  ScopedStatement statement(PrepareQuery(load_dbhandle_, select.c_str()));
  base::hash_set<int64> handles;
  while (EntryKernel* kernel = UnpackEntry(statement.get())) {
    DCHECK(handles.insert(kernel->ref(META_HANDLE)).second);  // Only in debug.
    entry_bucket->insert(kernel);
  }
}

void DirectoryBackingStore::LoadExtendedAttributes(
    ExtendedAttributes* xattrs_bucket) {
  ScopedStatement statement(PrepareQuery(load_dbhandle_,
      "SELECT metahandle, key, value FROM extended_attributes"));
  int step_result = sqlite3_step(statement.get());
  while (SQLITE_ROW == step_result) {
    int64 metahandle;
    string path_string_key;
    ExtendedAttributeValue val;
    val.is_deleted = false;
    GetColumn(statement.get(), 0, &metahandle);
    GetColumn(statement.get(), 1, &path_string_key);
    GetColumn(statement.get(), 2, &(val.value));
    ExtendedAttributeKey key(metahandle, path_string_key);
    xattrs_bucket->insert(std::make_pair(key, val));
    step_result = sqlite3_step(statement.get());
  }
  CHECK(SQLITE_DONE == step_result);
}

void DirectoryBackingStore::LoadInfo(Directory::KernelLoadInfo* info) {
  ScopedStatement query(PrepareQuery(load_dbhandle_,
      "SELECT last_sync_timestamp, initial_sync_ended, "
      "store_birthday, next_id, cache_guid "
      "FROM share_info"));
  CHECK(SQLITE_ROW == sqlite3_step(query.get()));
  GetColumn(query.get(), 0, &info->kernel_info.last_sync_timestamp);
  GetColumn(query.get(), 1, &info->kernel_info.initial_sync_ended);
  GetColumn(query.get(), 2, &info->kernel_info.store_birthday);
  GetColumn(query.get(), 3, &info->kernel_info.next_id);
  GetColumn(query.get(), 4, &info->cache_guid);
  query.reset(PrepareQuery(load_dbhandle_,
      "SELECT MAX(metahandle) FROM metas"));
  CHECK(SQLITE_ROW == sqlite3_step(query.get()));
  GetColumn(query.get(), 0, &info->max_metahandle);
}

bool DirectoryBackingStore::SaveEntryToDB(const EntryKernel& entry) {
  DCHECK(save_dbhandle_);
  string query;
  query.reserve(kUpdateStatementBufferSize);
  query.append("INSERT OR REPLACE INTO metas ");
  string values;
  values.reserve(kUpdateStatementBufferSize);
  values.append("VALUES ");
  const char* separator = "( ";
  int i = 0;
  for (i = BEGIN_FIELDS; i < BLOB_FIELDS_END; ++i) {
    query.append(separator);
    values.append(separator);
    separator = ", ";
    query.append(ColumnName(i));
    values.append("?");
  }

  query.append(" ) ");
  values.append(" )");
  query.append(values);
  ScopedStatement const statement(PrepareQuery(save_dbhandle_, query.c_str()));
  BindFields(entry, statement.get());
  return StepDone(statement.get(), "SaveEntryToDB()") &&
      1 == sqlite3_changes(save_dbhandle_);
}

bool DirectoryBackingStore::SaveExtendedAttributeToDB(
    ExtendedAttributes::const_iterator i) {
  DCHECK(save_dbhandle_);
  ScopedStatement insert(PrepareQuery(save_dbhandle_,
      "INSERT INTO extended_attributes "
      "(metahandle, key, value) "
      "values ( ?, ?, ? )",
      i->first.metahandle, i->first.key, i->second.value));
  return StepDone(insert.get(), "SaveExtendedAttributeToDB()")
      && 1 == sqlite3_changes(LazyGetSaveHandle());
}

bool DirectoryBackingStore::DeleteExtendedAttributeFromDB(
    ExtendedAttributes::const_iterator i) {
  DCHECK(save_dbhandle_);
  ScopedStatement delete_attribute(PrepareQuery(save_dbhandle_,
      "DELETE FROM extended_attributes "
      "WHERE metahandle = ? AND key = ? ",
      i->first.metahandle, i->first.key));
  if (!StepDone(delete_attribute.get(), "DeleteExtendedAttributeFromDB()")) {
    LOG(ERROR) << "DeleteExtendedAttributeFromDB(),StepDone() failed "
        << "for metahandle: " << i->first.metahandle << " key: "
        << i->first.key;
    return false;
  }
  // The attribute may have never been saved to the database if it was
  // created and then immediately deleted.  So don't check that we
  // deleted exactly 1 row.
  return true;
}

void DirectoryBackingStore::DropDeletedEntries() {
  static const char delete_extended_attributes[] =
      "DELETE FROM extended_attributes WHERE metahandle IN "
      "(SELECT metahandle from death_row)";
  static const char delete_metas[] = "DELETE FROM metas WHERE metahandle IN "
                                     "(SELECT metahandle from death_row)";
  // Put all statements into a transaction for better performance
  ExecOrDie(load_dbhandle_, "BEGIN TRANSACTION");
  ExecOrDie(load_dbhandle_, "CREATE TEMP TABLE death_row (metahandle BIGINT)");
  ExecOrDie(load_dbhandle_, "INSERT INTO death_row "
            "SELECT metahandle from metas WHERE is_del > 0 "
            " AND is_unsynced < 1"
            " AND is_unapplied_update < 1");
  StatementExecutor x(load_dbhandle_);
  x.Exec(delete_extended_attributes);
  x.Exec(delete_metas);
  ExecOrDie(load_dbhandle_, "DROP TABLE death_row");
  ExecOrDie(load_dbhandle_, "COMMIT TRANSACTION");
}

void DirectoryBackingStore::SafeDropTable(const char* table_name) {
  string query = "DROP TABLE IF EXISTS ";
  query.append(table_name);
  SQLStatement statement;
  if (SQLITE_OK == statement.prepare(load_dbhandle_, query.data(),
                                     query.size())) {
    CHECK(SQLITE_DONE == statement.step());
  }
  statement.finalize();
}

int DirectoryBackingStore::CreateExtendedAttributeTable() {
  SafeDropTable("extended_attributes");
  LOG(INFO) << "CreateExtendedAttributeTable";
  return Exec(load_dbhandle_, "CREATE TABLE extended_attributes("
      "metahandle bigint, "
      "key varchar(127), "
      "value blob, "
      "PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE)");
}

void DirectoryBackingStore::DropAllTables() {
  SafeDropTable("metas");
  SafeDropTable("share_info");
  SafeDropTable("share_version");
  SafeDropTable("extended_attributes");
}

int DirectoryBackingStore::CreateTables() {
  LOG(INFO) << "First run, creating tables";
  // Create two little tables share_version and share_info
  int result = Exec(load_dbhandle_, "CREATE TABLE share_version ("
                    "id VARCHAR(128) primary key, data INT)");
  result = SQLITE_DONE != result ? result :
      Exec(load_dbhandle_, "INSERT INTO share_version VALUES(?, ?)",
           dir_name_, kCurrentDBVersion);
  result = SQLITE_DONE != result ? result :
      Exec(load_dbhandle_, "CREATE TABLE share_info ("
           "id VARCHAR(128) primary key, "
           "last_sync_timestamp INT, "
           "name VARCHAR(128), "
           // Gets set if the syncer ever gets updates from the
           // server and the server returns 0.  Lets us detect the
           // end of the initial sync.
           "initial_sync_ended BIT default 0, "
           "store_birthday VARCHAR(256), "
           "db_create_version VARCHAR(128), "
           "db_create_time int, "
           "next_id bigint default -2, "
           "cache_guid VARCHAR(32))");
  result = SQLITE_DONE != result ? result :
      Exec(load_dbhandle_, "INSERT INTO share_info VALUES"
           "(?, "  // id
           "0, "   // last_sync_timestamp
           "?, "   // name
           "?, "   // initial_sync_ended
           "?, "   // store_birthday
           "?, "   // db_create_version
           "?, "   // db_create_time
           "-2, "  // next_id
           "?)",   // cache_guid
           dir_name_,                  // id
           dir_name_,                  // name
           false,                        // initial_sync_ended
           "",                           // store_birthday
           SYNC_ENGINE_VERSION_STRING,   // db_create_version
           static_cast<int32>(time(0)),  // db_create_time
           GenerateCacheGUID());         // cache_guid
  // Create the big metas table.
  string query = "CREATE TABLE metas " + ComposeCreateTableColumnSpecs
      (g_metas_columns, g_metas_columns + arraysize(g_metas_columns));
  result = SQLITE_DONE != result ? result : Exec(load_dbhandle_, query.c_str());
  // Insert the entry for the root into the metas table.
  const int64 now = Now();
  result = SQLITE_DONE != result ? result :
    Exec(load_dbhandle_, "INSERT INTO metas "
         "( id, metahandle, is_dir, ctime, mtime) "
         "VALUES ( \"r\", 1, 1, ?, ?)",
         now, now);
  result = SQLITE_DONE != result ? result : CreateExtendedAttributeTable();
  return result;
}

sqlite3* DirectoryBackingStore::LazyGetSaveHandle() {
  if (!save_dbhandle_ && !OpenAndConfigureHandleHelper(&save_dbhandle_)) {
    DCHECK(FALSE) << "Unable to open handle for saving";
    return NULL;
  }
  return save_dbhandle_;
}

}  // namespace syncable
