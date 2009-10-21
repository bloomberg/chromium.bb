// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/directory_backing_store.h"

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <string>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable_columns.h"
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/query_helpers.h"
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
static const int32 kCurrentDBVersion = 67;

#if OS_WIN
// TODO(sync): remove
static void PathNameMatch16(sqlite3_context* context, int argc,
                            sqlite3_value** argv) {
  const PathString pathspec(reinterpret_cast<const PathChar*>
      (sqlite3_value_text16(argv[0])), sqlite3_value_bytes16(argv[0]) / 2);

  const void* name_text = sqlite3_value_text16(argv[1]);
  int name_bytes = sqlite3_value_bytes16(argv[1]);
  // If the text is null, we need to avoid the PathString constructor.
  if (name_text != NULL) {
    // Have to copy to append a terminating 0 anyway.
    const PathString name(reinterpret_cast<const PathChar*>
        (sqlite3_value_text16(argv[1])),
        sqlite3_value_bytes16(argv[1]) / 2);
    sqlite3_result_int(context, PathNameMatch(name, pathspec));
  } else {
    sqlite3_result_int(context, PathNameMatch(PathString(), pathspec));
  }
}

// Sqlite allows setting of the escape character in an ESCAPE clause and
// this character is passed in as a third character to the like function.
// See: http://www.sqlite.org/lang_expr.html
static void PathNameMatch16WithEscape(sqlite3_context* context,
                                      int argc, sqlite3_value** argv) {
  // Never seen this called, but just in case.
  LOG(FATAL) << "PathNameMatch16WithEscape() not implemented";
}
#endif

static void RegisterPathNameCollate(sqlite3* dbhandle) {
#if defined(OS_WIN)
  const int collate = SQLITE_UTF16;
#else
  const int collate = SQLITE_UTF8;
#endif
  CHECK(SQLITE_OK == sqlite3_create_collation(dbhandle, "PATHNAME", collate,
      NULL, &ComparePathNames16));
}

// Replace the LIKE operator with our own implementation that does file spec
// matching like "*.txt".
static void RegisterPathNameMatch(sqlite3* dbhandle) {
  // We only register this on Windows. We use the normal sqlite
  // matching function on mac/linux.
  // note that the function PathNameMatch() does a simple ==
  // comparison on mac, so that would have to be fixed if
  // we really wanted to use PathNameMatch on mac/linux w/ the
  // same pattern strings as we do on windows.
#if defined(OS_WIN)
  CHECK(SQLITE_OK == sqlite3_create_function(dbhandle, "like",
      2, SQLITE_ANY, NULL, &PathNameMatch16, NULL, NULL));
  CHECK(SQLITE_OK == sqlite3_create_function(dbhandle, "like",
      3, SQLITE_ANY, NULL, &PathNameMatch16WithEscape, NULL, NULL));
#endif  // OS_WIN
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

// Iterate over the fields of |entry| and bind dirty ones to |statement| for
// updating.  Returns the number of args bound.
static int BindDirtyFields(const EntryKernel& entry, sqlite3_stmt* statement) {
  int index = 1;
  int i = 0;
  for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
    if (entry.dirty[i])
      BindArg(statement, entry.ref(static_cast<Int64Field>(i)), index++);
  }
  for ( ; i < ID_FIELDS_END; ++i) {
    if (entry.dirty[i])
      BindArg(statement, entry.ref(static_cast<IdField>(i)), index++);
  }
  for ( ; i < BIT_FIELDS_END; ++i) {
    if (entry.dirty[i])
      BindArg(statement, entry.ref(static_cast<BitField>(i)), index++);
  }
  for ( ; i < STRING_FIELDS_END; ++i) {
    if (entry.dirty[i])
      BindArg(statement, entry.ref(static_cast<StringField>(i)), index++);
  }
  for ( ; i < BLOB_FIELDS_END; ++i) {
    if (entry.dirty[i])
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

DirectoryBackingStore::DirectoryBackingStore(const PathString& dir_name,
    const PathString& backing_filepath)
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
  if (SQLITE_OK == SqliteOpen(backing_filepath_.c_str(), handle)) {
    sqlite3_busy_timeout(*handle, kDirectoryBackingStoreBusyTimeoutMs);
    RegisterPathNameCollate(*handle);
    RegisterPathNameMatch(*handle);
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
      DCHECK(i->dirty.any());
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
  StatementExecutor se(load_dbhandle_);
  if (SQLITE_DONE != se.Exec("BEGIN EXCLUSIVE TRANSACTION")) {
    return FAILED_DISK_FULL;
  }
  int version_on_disk = 0;

  if (DoesTableExist(load_dbhandle_, "share_version")) {
    ScopedStatement version_query(
        PrepareQuery(load_dbhandle_, "SELECT data from share_version"));
    int query_result = sqlite3_step(version_query.get());
    if (SQLITE_ROW == query_result) {
      version_on_disk = sqlite3_column_int(version_query.get(), 0);
    }
    version_query.reset(NULL);
  }
  if (version_on_disk != kCurrentDBVersion) {
    if (version_on_disk > kCurrentDBVersion) {
      ExecOrDie(load_dbhandle_, "END TRANSACTION");
      return FAILED_NEWER_VERSION;
    }
    LOG(INFO) << "Old/null sync database, version " << version_on_disk;
    // Delete the existing database (if any), and create a freshone.
    if (se.healthy()) {
      DropAllTables();
      se.set_result(CreateTables());
    }
  }
  if (SQLITE_DONE == se.result()) {
    {
      ScopedStatement statement(PrepareQuery(load_dbhandle_,
          "SELECT db_create_version, db_create_time FROM share_info"));
      CHECK(SQLITE_ROW == sqlite3_step(statement.get()));
      PathString db_create_version;
      int db_create_time;
      GetColumn(statement.get(), 0, &db_create_version);
      GetColumn(statement.get(), 1, &db_create_time);
      statement.reset(0);
      LOG(INFO) << "DB created at " << db_create_time << " by version " <<
          db_create_version;
    }
    // COMMIT TRANSACTION rolls back on failure.
    if (SQLITE_DONE == Exec(load_dbhandle_, "COMMIT TRANSACTION"))
      return OPENED;
  } else {
    ExecOrDie(load_dbhandle_, "ROLLBACK TRANSACTION");
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
    PathString path_string_key;
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
  return entry.ref(IS_NEW) ? SaveNewEntryToDB(entry) : UpdateEntryToDB(entry);
}

bool DirectoryBackingStore::SaveNewEntryToDB(const EntryKernel& entry) {
  DCHECK(save_dbhandle_);
  // TODO(timsteele): Should use INSERT OR REPLACE and eliminate one of
  // the SaveNew / UpdateEntry code paths.
  string query;
  query.reserve(kUpdateStatementBufferSize);
  query.append("INSERT INTO metas ");
  string values;
  values.reserve(kUpdateStatementBufferSize);
  values.append("VALUES ");
  const char* separator = "( ";
  int i = 0;
  for (i = BEGIN_FIELDS; i < BLOB_FIELDS_END; ++i) {
    if (entry.dirty[i]) {
      query.append(separator);
      values.append(separator);
      separator = ", ";
      query.append(ColumnName(i));
      values.append("?");
    }
  }
  query.append(" ) ");
  values.append(" )");
  query.append(values);
  ScopedStatement const statement(PrepareQuery(save_dbhandle_, query.c_str()));
  BindDirtyFields(entry, statement.get());
  return StepDone(statement.get(), "SaveNewEntryToDB()") &&
      1 == sqlite3_changes(save_dbhandle_);
}

bool DirectoryBackingStore::UpdateEntryToDB(const EntryKernel& entry) {
  DCHECK(save_dbhandle_);
  string query;
  query.reserve(kUpdateStatementBufferSize);
  query.append("UPDATE metas ");
  const char* separator = "SET ";
  int i;
  for (i = BEGIN_FIELDS; i < BLOB_FIELDS_END; ++i) {
    if (entry.dirty[i]) {
      query.append(separator);
      separator = ", ";
      query.append(ColumnName(i));
      query.append(" = ? ");
    }
  }
  query.append("WHERE metahandle = ?");
  ScopedStatement const statement(PrepareQuery(save_dbhandle_, query.c_str()));
  const int var_count = BindDirtyFields(entry, statement.get());
  BindArg(statement.get(), entry.ref(META_HANDLE), var_count + 1);
  return StepDone(statement.get(), "UpdateEntryToDB()") &&
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
  const char* tail;
  sqlite3_stmt* statement = NULL;
  if (SQLITE_OK == sqlite3_prepare(load_dbhandle_, query.data(),
                                   query.size(), &statement, &tail)) {
    CHECK(SQLITE_DONE == sqlite3_step(statement));
  }
  sqlite3_finalize(statement);
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
