// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/directory_backing_store.h"

#include "build/build_config.h"

#include <limits>

#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable_columns.h"
#include "chrome/browser/sync/util/sqlite_utils.h"
#include "chrome/browser/sync/util/time.h"
#include "chrome/common/random.h"
#include "third_party/sqlite/sqlite3.h"

// Sometimes threads contend on the DB lock itself, especially when one thread
// is calling SaveChanges.  In the worst case scenario, the user can put his
// laptop to sleep during db contention, and wake up the laptop days later, so
// infinity seems like the best choice here.
const int kDirectoryBackingStoreBusyTimeoutMs = INT_MAX;

using std::string;

namespace syncable {

// This just has to be big enough to hold an UPDATE or INSERT statement that
// modifies all the columns in the entry table.
static const string::size_type kUpdateStatementBufferSize = 2048;

// Increment this version whenever updating DB tables.
extern const int32 kCurrentDBVersion;  // Global visibility for our unittest.
const int32 kCurrentDBVersion = 77;

namespace {

int ExecQuery(sqlite3* dbhandle, const char* query) {
  sqlite_utils::SQLStatement statement;
  int result = statement.prepare(dbhandle, query);
  if (SQLITE_OK != result)
    return result;
  do {
    result = statement.step();
  } while (SQLITE_ROW == result);

  return result;
}

string GenerateCacheGUID() {
  return Generate128BitRandomBase64String();
}

}  // namespace


// Iterate over the fields of |entry| and bind each to |statement| for
// updating.  Returns the number of args bound.
int BindFields(const EntryKernel& entry,
               sqlite_utils::SQLStatement* statement) {
  int index = 0;
  int i = 0;
  for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
    statement->bind_int64(index++, entry.ref(static_cast<Int64Field>(i)));
  }
  for ( ; i < TIME_FIELDS_END; ++i) {
    statement->bind_int64(index++,
                          browser_sync::TimeToProtoTime(
                              entry.ref(static_cast<TimeField>(i))));
  }
  for ( ; i < ID_FIELDS_END; ++i) {
    statement->bind_string(index++, entry.ref(static_cast<IdField>(i)).s_);
  }
  for ( ; i < BIT_FIELDS_END; ++i) {
    statement->bind_int(index++, entry.ref(static_cast<BitField>(i)));
  }
  for ( ; i < STRING_FIELDS_END; ++i) {
    statement->bind_string(index++, entry.ref(static_cast<StringField>(i)));
  }
  std::string temp;
  for ( ; i < PROTO_FIELDS_END; ++i) {
    entry.ref(static_cast<ProtoField>(i)).SerializeToString(&temp);
    statement->bind_blob(index++, temp.data(), temp.length());
  }
  return index;
}

// The caller owns the returned EntryKernel*.
int UnpackEntry(sqlite_utils::SQLStatement* statement, EntryKernel** kernel) {
  *kernel = NULL;
  int query_result = statement->step();
  if (query_result == SQLITE_ROW) {
    *kernel = new EntryKernel();
    DCHECK_EQ(statement->column_count(), static_cast<int>(FIELD_COUNT));
    int i = 0;
    for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
      (*kernel)->put(static_cast<Int64Field>(i), statement->column_int64(i));
    }
    for ( ; i < TIME_FIELDS_END; ++i) {
      (*kernel)->put(static_cast<TimeField>(i),
                     browser_sync::ProtoTimeToTime(
                         statement->column_int64(i)));
    }
    for ( ; i < ID_FIELDS_END; ++i) {
      (*kernel)->mutable_ref(static_cast<IdField>(i)).s_ =
          statement->column_string(i);
    }
    for ( ; i < BIT_FIELDS_END; ++i) {
      (*kernel)->put(static_cast<BitField>(i), (0 != statement->column_int(i)));
    }
    for ( ; i < STRING_FIELDS_END; ++i) {
      (*kernel)->put(static_cast<StringField>(i),
          statement->column_string(i));
    }
    for ( ; i < PROTO_FIELDS_END; ++i) {
      (*kernel)->mutable_ref(static_cast<ProtoField>(i)).ParseFromArray(
          statement->column_blob(i), statement->column_bytes(i));
    }
  } else {
    DCHECK_EQ(query_result, SQLITE_DONE);
    (*kernel) = NULL;
  }
  return query_result;
}

namespace {

string ComposeCreateTableColumnSpecs() {
  const ColumnSpec* begin = g_metas_columns;
  const ColumnSpec* end = g_metas_columns + arraysize(g_metas_columns);
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

void AppendColumnList(std::string* output) {
  const char* joiner = " ";
  // Be explicit in SELECT order to match up with UnpackEntry.
  for (int i = BEGIN_FIELDS; i < BEGIN_FIELDS + FIELD_COUNT; ++i) {
    output->append(joiner);
    output->append(ColumnName(i));
    joiner = ", ";
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DirectoryBackingStore implementation.

DirectoryBackingStore::DirectoryBackingStore(const string& dir_name,
                                             const FilePath& backing_filepath)
    : load_dbhandle_(NULL),
      save_dbhandle_(NULL),
      dir_name_(dir_name),
      backing_filepath_(backing_filepath),
      needs_column_refresh_(false) {
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
  if (SQLITE_OK == sqlite_utils::OpenSqliteDb(backing_filepath_, handle)) {
    sqlite3_busy_timeout(*handle, std::numeric_limits<int>::max());
    {
      string integrity_error;
      bool is_ok = CheckIntegrity(*handle, &integrity_error);
      if (!is_ok) {
        LOG(ERROR) << "Integrity check failed: " << integrity_error;
        sqlite3_close(*handle);
        *handle = NULL;
        return false;
      }
    }
    {
      sqlite_utils::SQLStatement statement;
      statement.prepare(*handle, "PRAGMA fullfsync = 1");
      if (SQLITE_DONE != statement.step()) {
        LOG(ERROR) << sqlite3_errmsg(*handle);
        sqlite3_close(*handle);
        *handle = NULL;
        return false;
      }
    }
    {
      sqlite_utils::SQLStatement statement;
      statement.prepare(*handle, "PRAGMA synchronous = 2");
      if (SQLITE_DONE != statement.step()) {
        LOG(ERROR) << sqlite3_errmsg(*handle);
        sqlite3_close(*handle);
        *handle = NULL;
        return false;
      }
    }
    sqlite3_busy_timeout(*handle, kDirectoryBackingStoreBusyTimeoutMs);
#if defined(OS_WIN)
    // Do not index this file. Scanning can occur every time we close the file,
    // which causes long delays in SQLite's file locking.
    const DWORD attrs = GetFileAttributes(backing_filepath_.value().c_str());
    const BOOL attrs_set =
      SetFileAttributes(backing_filepath_.value().c_str(),
                        attrs | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
#endif

    return true;
  } else {
    // The sqlite3 API docs indicate a handle is usually returned even
    // in the case of error.  We clean it up here.
    if (*handle != NULL) {
      sqlite3_close(*handle);
      *handle = NULL;
    }
    return false;
  }
}

bool DirectoryBackingStore::CheckIntegrity(sqlite3* handle, string* error)
    const {
  sqlite_utils::SQLStatement statement;
  if (SQLITE_OK !=
      statement.prepare(handle, "PRAGMA integrity_check(1)")) {
    *error = sqlite3_errmsg(handle);
    return false;
  }
  if (SQLITE_ROW != statement.step()) {
    *error =  sqlite3_errmsg(handle);
    return false;
  }
  string integrity_result = statement.column_text(0);
  if (integrity_result != "ok") {
    *error = integrity_result;
    return false;
  }
  return true;
}

DirOpenResult DirectoryBackingStore::DoLoad(MetahandlesIndex* entry_bucket,
    Directory::KernelLoadInfo* kernel_load_info) {
  {
    DirOpenResult result = InitializeTables();
    if (result != OPENED)
      return result;
  }

  if (!DropDeletedEntries())
    return FAILED_DATABASE_CORRUPT;
  if (!LoadEntries(entry_bucket))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadInfo(kernel_load_info))
    return FAILED_DATABASE_CORRUPT;

  return OPENED;
}

DirOpenResult DirectoryBackingStore::Load(MetahandlesIndex* entry_bucket,
    Directory::KernelLoadInfo* kernel_load_info) {

  // Open database handle.
  if (!BeginLoad())
    return FAILED_OPEN_DATABASE;

  // Load data from the database.
  DirOpenResult result = DoLoad(entry_bucket, kernel_load_info);

  // Clean up partial results after failure.
  if (result != OPENED)
    STLDeleteElements(entry_bucket);

  // Close database handle.
  EndLoad();

  return result;
}

bool DirectoryBackingStore::BeginLoad() {
  DCHECK(load_dbhandle_ == NULL);
  return OpenAndConfigureHandleHelper(&load_dbhandle_);
}

void DirectoryBackingStore::EndLoad() {
  sqlite3_close(load_dbhandle_);
  load_dbhandle_ = NULL;  // No longer used.
}

void DirectoryBackingStore::EndSave() {
  sqlite3_close(save_dbhandle_);
  save_dbhandle_ = NULL;
}

bool DirectoryBackingStore::DeleteEntries(const MetahandleSet& handles) {
  if (handles.empty())
    return true;

  sqlite3* dbhandle = LazyGetSaveHandle();

  string query = "DELETE FROM metas WHERE metahandle IN (";
  for (MetahandleSet::const_iterator it = handles.begin(); it != handles.end();
       ++it) {
    if (it != handles.begin())
      query.append(",");
    query.append(base::Int64ToString(*it));
  }
  query.append(")");
  sqlite_utils::SQLStatement statement;
  int result = statement.prepare(dbhandle, query.data(), query.size());
  if (SQLITE_OK == result)
    result = statement.step();

  return SQLITE_DONE == result;
}

bool DirectoryBackingStore::SaveChanges(
    const Directory::SaveChangesSnapshot& snapshot) {
  sqlite3* dbhandle = LazyGetSaveHandle();

  // SQLTransaction::BeginExclusive causes a disk write to occur. This is not
  // something that should happen every 10 seconds when this function runs, so
  // just stop here if there's nothing to save.
  bool save_info =
    (Directory::KERNEL_SHARE_INFO_DIRTY == snapshot.kernel_info_status);
  if (snapshot.dirty_metas.size() < 1 && !save_info)
    return true;

  sqlite_utils::SQLTransaction transaction(dbhandle);
  if (SQLITE_OK != transaction.BeginExclusive())
    return false;

  for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    DCHECK(i->is_dirty());
    if (!SaveEntryToDB(*i))
      return false;
  }

  if (!DeleteEntries(snapshot.metahandles_to_purge))
    return false;

  if (save_info) {
    const Directory::PersistedKernelInfo& info = snapshot.kernel_info;
    sqlite_utils::SQLStatement update;
    update.prepare(dbhandle, "UPDATE share_info "
                   "SET store_birthday = ?, "
                   "next_id = ?, "
                   "notification_state = ? ");

    update.bind_string(0, info.store_birthday);
    update.bind_int64(1, info.next_id);
    update.bind_blob(2, info.notification_state.data(),
                     info.notification_state.size());

    if (!(SQLITE_DONE == update.step() &&
          SQLITE_OK == update.reset() &&
          1 == update.changes())) {
      return false;
    }

    for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
      sqlite_utils::SQLStatement op;
      op.prepare(dbhandle, "INSERT OR REPLACE INTO models (model_id, "
      "progress_marker, initial_sync_ended) VALUES ( ?, ?, ?)");
      // We persist not ModelType but rather a protobuf-derived ID.
      string model_id = ModelTypeEnumToModelId(ModelTypeFromInt(i));
      string progress_marker;
      info.download_progress[i].SerializeToString(&progress_marker);
      op.bind_blob(0, model_id.data(), model_id.length());
      op.bind_blob(1, progress_marker.data(), progress_marker.length());
      op.bind_bool(2, info.initial_sync_ended[i]);

      if (!(SQLITE_DONE == op.step() &&
            SQLITE_OK == op.reset() &&
            1 == op.changes())) {
        return false;
      }
    }
  }

  return (SQLITE_OK == transaction.Commit());
}

DirOpenResult DirectoryBackingStore::InitializeTables() {
  sqlite_utils::SQLTransaction transaction(load_dbhandle_);
  if (SQLITE_OK != transaction.BeginExclusive()) {
    return FAILED_DISK_FULL;
  }
  int version_on_disk = GetVersion();
  int last_result = SQLITE_DONE;

  // Upgrade from version 67. Version 67 was widely distributed as the original
  // Bookmark Sync release. Version 68 removed unique naming.
  if (version_on_disk == 67) {
    if (MigrateVersion67To68())
      version_on_disk = 68;
  }
  // Version 69 introduced additional datatypes.
  if (version_on_disk == 68) {
    if (MigrateVersion68To69())
      version_on_disk = 69;
  }

  if (version_on_disk == 69) {
    if (MigrateVersion69To70())
      version_on_disk = 70;
  }

  // Version 71 changed the sync progress information to be per-datatype.
  if (version_on_disk == 70) {
    if (MigrateVersion70To71())
      version_on_disk = 71;
  }

  // Version 72 removed extended attributes, a legacy way to do extensible
  // key/value information, stored in their own table.
  if (version_on_disk == 71) {
    if (MigrateVersion71To72())
      version_on_disk = 72;
  }

  // Version 73 added a field for notification state.
  if (version_on_disk == 72) {
    if (MigrateVersion72To73())
      version_on_disk = 73;
  }

  // Version 74 added state for the autofill migration.
  if (version_on_disk == 73) {
    if (MigrateVersion73To74())
      version_on_disk = 74;
  }

  // Version 75 migrated from int64-based timestamps to per-datatype tokens.
  if (version_on_disk == 74) {
    if (MigrateVersion74To75())
      version_on_disk = 75;
  }

  // Version 76 removed all (5) autofill migration related columns.
  if (version_on_disk == 75) {
    if (MigrateVersion75To76())
      version_on_disk = 76;
  }

  // Version 77 standardized all time fields to ms since the Unix
  // epoch.
  if (version_on_disk == 76) {
    if (MigrateVersion76To77())
      version_on_disk = 77;
  }

  // If one of the migrations requested it, drop columns that aren't current.
  // It's only safe to do this after migrating all the way to the current
  // version.
  if (version_on_disk == kCurrentDBVersion && needs_column_refresh_) {
    if (!RefreshColumns())
      version_on_disk = 0;
  }

  // A final, alternative catch-all migration to simply re-sync everything.
  if (version_on_disk != kCurrentDBVersion) {
    if (version_on_disk > kCurrentDBVersion) {
      transaction.Rollback();
      return FAILED_NEWER_VERSION;
    }
    // Fallback (re-sync everything) migration path.
    DVLOG(1) << "Old/null sync database, version " << version_on_disk;
    // Delete the existing database (if any), and create a fresh one.
    DropAllTables();
    last_result = CreateTables();
  }
  if (SQLITE_DONE == last_result) {
    {
      sqlite_utils::SQLStatement statement;
      statement.prepare(load_dbhandle_,
          "SELECT db_create_version, db_create_time FROM share_info");
      if (SQLITE_ROW != statement.step()) {
        transaction.Rollback();
        return FAILED_DISK_FULL;
      }
      string db_create_version = statement.column_text(0);
      int db_create_time = statement.column_int(1);
      statement.reset();
      DVLOG(1) << "DB created at " << db_create_time << " by version " <<
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

bool DirectoryBackingStore::RefreshColumns() {
  DCHECK(needs_column_refresh_);

  // Create a new table named temp_metas.
  SafeDropTable("temp_metas");
  if (CreateMetasTable(true) != SQLITE_DONE)
    return false;

  // Populate temp_metas from metas.
  std::string query = "INSERT INTO temp_metas (";
  AppendColumnList(&query);
  query.append(") SELECT ");
  AppendColumnList(&query);
  query.append(" FROM metas");
  if (ExecQuery(load_dbhandle_, query.c_str()) != SQLITE_DONE)
    return false;

  // Drop metas.
  SafeDropTable("metas");

  // Rename temp_metas -> metas.
  int result = ExecQuery(load_dbhandle_,
                         "ALTER TABLE temp_metas RENAME TO metas");
  if (result != SQLITE_DONE)
    return false;

  // Repeat the process for share_info.
  SafeDropTable("temp_share_info");
  if (CreateShareInfoTable(true) != SQLITE_DONE)
    return false;

  result = ExecQuery(load_dbhandle_,
                "INSERT INTO temp_share_info (id, name, store_birthday, "
                "db_create_version, db_create_time, next_id, cache_guid,"
                "notification_state) "
                "SELECT id, name, store_birthday, db_create_version, "
                "db_create_time, next_id, cache_guid, notification_state "
                "FROM share_info");
  if (result != SQLITE_DONE)
    return false;

  SafeDropTable("share_info");
  result = ExecQuery(load_dbhandle_,
                     "ALTER TABLE temp_share_info RENAME TO share_info");
  if (result != SQLITE_DONE)
    return false;

  needs_column_refresh_ = false;
  return true;
}

bool DirectoryBackingStore::LoadEntries(MetahandlesIndex* entry_bucket) {
  string select;
  select.reserve(kUpdateStatementBufferSize);
  select.append("SELECT ");
  AppendColumnList(&select);
  select.append(" FROM metas ");
  sqlite_utils::SQLStatement statement;
  statement.prepare(load_dbhandle_, select.c_str());
  base::hash_set<int64> handles;
  EntryKernel* kernel = NULL;
  int query_result;
  while (SQLITE_ROW == (query_result = UnpackEntry(&statement, &kernel))) {
    DCHECK(handles.insert(kernel->ref(META_HANDLE)).second);  // Only in debug.
    entry_bucket->insert(kernel);
  }
  return SQLITE_DONE == query_result;
}

bool DirectoryBackingStore::LoadInfo(Directory::KernelLoadInfo* info) {
  {
    sqlite_utils::SQLStatement query;
    query.prepare(load_dbhandle_,
                  "SELECT store_birthday, next_id, cache_guid, "
                  "notification_state "
                  "FROM share_info");
    if (SQLITE_ROW != query.step())
      return false;
    info->kernel_info.store_birthday = query.column_string(0);
    info->kernel_info.next_id = query.column_int64(1);
    info->cache_guid = query.column_string(2);
    query.column_blob_as_string(3, &info->kernel_info.notification_state);
  }
  {
    sqlite_utils::SQLStatement query;
    query.prepare(load_dbhandle_,
        "SELECT model_id, progress_marker, initial_sync_ended "
        "FROM models");
    while (SQLITE_ROW == query.step()) {
      ModelType type = ModelIdToModelTypeEnum(query.column_blob(0),
                                              query.column_bytes(0));
      if (type != UNSPECIFIED && type != TOP_LEVEL_FOLDER) {
        info->kernel_info.download_progress[type].ParseFromArray(
            query.column_blob(1), query.column_bytes(1));
        info->kernel_info.initial_sync_ended[type] = query.column_bool(2);
      }
    }
  }
  {
    sqlite_utils::SQLStatement query;
    query.prepare(load_dbhandle_,
                  "SELECT MAX(metahandle) FROM metas");
    if (SQLITE_ROW != query.step())
      return false;
    info->max_metahandle = query.column_int64(0);
  }
  return true;
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
  for (i = BEGIN_FIELDS; i < PROTO_FIELDS_END; ++i) {
    query.append(separator);
    values.append(separator);
    separator = ", ";
    query.append(ColumnName(i));
    values.append("?");
  }

  query.append(" ) ");
  values.append(" )");
  query.append(values);
  sqlite_utils::SQLStatement statement;
  statement.prepare(save_dbhandle_, query.c_str());
  BindFields(entry, &statement);
  return (SQLITE_DONE == statement.step() &&
          SQLITE_OK == statement.reset() &&
          1 == statement.changes());
}

bool DirectoryBackingStore::DropDeletedEntries() {
  static const char delete_metas[] = "DELETE FROM metas WHERE metahandle IN "
                                     "(SELECT metahandle from death_row)";
  // Put all statements into a transaction for better performance
  sqlite_utils::SQLTransaction transaction(load_dbhandle_);
  transaction.Begin();
  if (SQLITE_DONE != ExecQuery(
                         load_dbhandle_,
                         "CREATE TEMP TABLE death_row (metahandle BIGINT)")) {
    return false;
  }
  if (SQLITE_DONE != ExecQuery(load_dbhandle_,
                               "INSERT INTO death_row "
                               "SELECT metahandle from metas WHERE is_del > 0 "
                               " AND is_unsynced < 1"
                               " AND is_unapplied_update < 1")) {
    return false;
  }
  if (SQLITE_DONE != ExecQuery(load_dbhandle_, delete_metas)) {
    return false;
  }
  if (SQLITE_DONE != ExecQuery(load_dbhandle_, "DROP TABLE death_row")) {
    return false;
  }
  transaction.Commit();
  return true;
}

int DirectoryBackingStore::SafeDropTable(const char* table_name) {
  string query = "DROP TABLE IF EXISTS ";
  query.append(table_name);
  sqlite_utils::SQLStatement statement;
  int result = statement.prepare(load_dbhandle_, query.data(),
                                 query.size());
  if (SQLITE_OK == result) {
    result = statement.step();
    if (SQLITE_DONE == result)
      statement.finalize();
  }

  return result;
}

void DirectoryBackingStore::DropAllTables() {
  SafeDropTable("metas");
  SafeDropTable("temp_metas");
  SafeDropTable("share_info");
  SafeDropTable("temp_share_info");
  SafeDropTable("share_version");
  SafeDropTable("extended_attributes");
  SafeDropTable("models");
  SafeDropTable("temp_models");
  needs_column_refresh_ = false;
}

// static
ModelType DirectoryBackingStore::ModelIdToModelTypeEnum(
    const void* data, int size) {
  sync_pb::EntitySpecifics specifics;
  if (!specifics.ParseFromArray(data, size))
    return syncable::UNSPECIFIED;
  return syncable::GetModelTypeFromSpecifics(specifics);
}

// static
string DirectoryBackingStore::ModelTypeEnumToModelId(ModelType model_type) {
  sync_pb::EntitySpecifics specifics;
  syncable::AddDefaultExtensionValue(model_type, &specifics);
  return specifics.SerializeAsString();
}

bool DirectoryBackingStore::MigrateToSpecifics(
    const char* old_columns,
    const char* specifics_column,
    void (*handler_function)(sqlite_utils::SQLStatement* old_value_query,
                             int old_value_column,
                             sync_pb::EntitySpecifics* mutable_new_value)) {
  std::string query_sql = base::StringPrintf(
      "SELECT metahandle, %s, %s FROM metas", specifics_column, old_columns);
  std::string update_sql = base::StringPrintf(
      "UPDATE metas SET %s = ? WHERE metahandle = ?", specifics_column);
  sqlite_utils::SQLStatement query;
  query.prepare(load_dbhandle_, query_sql.c_str());
  while (query.step() == SQLITE_ROW) {
    int64 metahandle = query.column_int64(0);
    std::string new_value_bytes;
    query.column_blob_as_string(1, &new_value_bytes);
    sync_pb::EntitySpecifics new_value;
    new_value.ParseFromString(new_value_bytes);
    handler_function(&query, 2, &new_value);
    new_value.SerializeToString(&new_value_bytes);

    sqlite_utils::SQLStatement update;
    update.prepare(load_dbhandle_, update_sql.data(), update_sql.length());
    update.bind_blob(0, new_value_bytes.data(), new_value_bytes.length());
    update.bind_int64(1, metahandle);
    if (update.step() != SQLITE_DONE) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool DirectoryBackingStore::AddColumn(const ColumnSpec* column) {
  sqlite_utils::SQLStatement add_column;
  std::string sql = base::StringPrintf(
      "ALTER TABLE metas ADD COLUMN %s %s", column->name, column->spec);
  add_column.prepare(load_dbhandle_, sql.c_str());
  return add_column.step() == SQLITE_DONE;
}

bool DirectoryBackingStore::SetVersion(int version) {
  sqlite_utils::SQLStatement statement;
  statement.prepare(load_dbhandle_, "UPDATE share_version SET data = ?");
  statement.bind_int(0, version);
  return statement.step() == SQLITE_DONE;
}

int DirectoryBackingStore::GetVersion() {
  if (!sqlite_utils::DoesSqliteTableExist(load_dbhandle_, "share_version"))
    return 0;
  sqlite_utils::SQLStatement version_query;
  version_query.prepare(load_dbhandle_, "SELECT data from share_version");
  if (SQLITE_ROW != version_query.step())
    return 0;
  int value = version_query.column_int(0);
  if (version_query.reset() != SQLITE_OK)
    return 0;
  return value;
}

bool DirectoryBackingStore::MigrateVersion67To68() {
  // This change simply removed three columns:
  //   string NAME
  //   string UNSANITIZED_NAME
  //   string SERVER_NAME
  // No data migration is necessary, but we should do a column refresh.
  SetVersion(68);
  needs_column_refresh_ = true;
  return true;
}

bool DirectoryBackingStore::MigrateVersion69To70() {
  // Added "unique_client_tag", renamed "singleton_tag" to unique_server_tag
  SetVersion(70);
  // We use these metas column names but if in the future
  // we rename the column again, we need to inline the old
  // intermediate name / column spec.
  if (!AddColumn(&g_metas_columns[UNIQUE_SERVER_TAG])) {
    return false;
  }
  if (!AddColumn(&g_metas_columns[UNIQUE_CLIENT_TAG])) {
    return false;
  }
  needs_column_refresh_ = true;

  sqlite_utils::SQLStatement statement;
  statement.prepare(load_dbhandle_,
      "UPDATE metas SET unique_server_tag = singleton_tag");
  return statement.step() == SQLITE_DONE;
}

namespace {

// Callback passed to MigrateToSpecifics for the v68->v69 migration.  See
// MigrateVersion68To69().
void EncodeBookmarkURLAndFavicon(sqlite_utils::SQLStatement* old_value_query,
                                 int old_value_column,
                                 sync_pb::EntitySpecifics* mutable_new_value) {
  // Extract data from the column trio we expect.
  bool old_is_bookmark_object = old_value_query->column_bool(old_value_column);
  std::string old_url = old_value_query->column_string(old_value_column + 1);
  std::string old_favicon;
  old_value_query->column_blob_as_string(old_value_column + 2, &old_favicon);
  bool old_is_dir = old_value_query->column_bool(old_value_column + 3);

  if (old_is_bookmark_object) {
    sync_pb::BookmarkSpecifics* bookmark_data =
        mutable_new_value->MutableExtension(sync_pb::bookmark);
    if (!old_is_dir) {
      bookmark_data->set_url(old_url);
      bookmark_data->set_favicon(old_favicon);
    }
  }
}

}  // namespace

bool DirectoryBackingStore::MigrateVersion68To69() {
  // In Version 68, there were columns on table 'metas':
  //   string BOOKMARK_URL
  //   string SERVER_BOOKMARK_URL
  //   blob BOOKMARK_FAVICON
  //   blob SERVER_BOOKMARK_FAVICON
  // In version 69, these columns went away in favor of storing
  // a serialized EntrySpecifics protobuf in the columns:
  //   protobuf blob SPECIFICS
  //   protobuf blob SERVER_SPECIFICS
  // For bookmarks, EntrySpecifics is extended as per
  // bookmark_specifics.proto. This migration converts bookmarks from the
  // former scheme to the latter scheme.

  // First, add the two new columns to the schema.
  if (!AddColumn(&g_metas_columns[SPECIFICS]))
    return false;
  if (!AddColumn(&g_metas_columns[SERVER_SPECIFICS]))
    return false;

  // Next, fold data from the old columns into the new protobuf columns.
  if (!MigrateToSpecifics(("is_bookmark_object, bookmark_url, "
                           "bookmark_favicon, is_dir"),
                          "specifics",
                          &EncodeBookmarkURLAndFavicon)) {
    return false;
  }
  if (!MigrateToSpecifics(("server_is_bookmark_object, "
                           "server_bookmark_url, "
                           "server_bookmark_favicon, "
                           "server_is_dir"),
                          "server_specifics",
                          &EncodeBookmarkURLAndFavicon)) {
    return false;
  }

  // Lastly, fix up the "Google Chrome" folder, which is of the TOP_LEVEL_FOLDER
  // ModelType: it shouldn't have BookmarkSpecifics.
  sqlite_utils::SQLStatement clear_permanent_items;
  clear_permanent_items.prepare(load_dbhandle_,
      "UPDATE metas SET specifics = NULL, server_specifics = NULL WHERE "
      "singleton_tag IN ('google_chrome')");
  if (clear_permanent_items.step() != SQLITE_DONE)
    return false;

  SetVersion(69);
  needs_column_refresh_ = true;  // Trigger deletion of old columns.
  return true;
}

// Version 71, the columns 'initial_sync_ended' and 'last_sync_timestamp'
// were removed from the share_info table.  They were replaced by
// the 'models' table, which has these values on a per-datatype basis.
bool DirectoryBackingStore::MigrateVersion70To71() {
  if (SQLITE_DONE != CreateV71ModelsTable())
    return false;

  // Move data from the old share_info columns to the new models table.
  {
    sqlite_utils::SQLStatement fetch;
    fetch.prepare(load_dbhandle_,
        "SELECT last_sync_timestamp, initial_sync_ended FROM share_info");

    if (SQLITE_ROW != fetch.step())
      return false;
    int64 last_sync_timestamp = fetch.column_int64(0);
    bool initial_sync_ended = fetch.column_bool(1);
    if (SQLITE_DONE != fetch.step())
      return false;
    sqlite_utils::SQLStatement update;
    update.prepare(load_dbhandle_, "INSERT INTO models (model_id, "
        "last_download_timestamp, initial_sync_ended) VALUES (?, ?, ?)");
    string bookmark_model_id = ModelTypeEnumToModelId(BOOKMARKS);
    update.bind_blob(0, bookmark_model_id.data(), bookmark_model_id.size());
    update.bind_int64(1, last_sync_timestamp);
    update.bind_bool(2, initial_sync_ended);
    if (SQLITE_DONE != update.step())
      return false;
  }

  // Drop the columns from the old share_info table via a temp table.
  const bool kCreateAsTempShareInfo = true;

  int result =
      CreateShareInfoTableVersion71(kCreateAsTempShareInfo);
  if (result != SQLITE_DONE)
    return false;
  result = ExecQuery(load_dbhandle_,
                     "INSERT INTO temp_share_info (id, name, store_birthday, "
                     "db_create_version, db_create_time, next_id, cache_guid) "
                     "SELECT id, name, store_birthday, db_create_version, "
                     "db_create_time, next_id, cache_guid FROM share_info");
  if (result != SQLITE_DONE)
    return false;
  SafeDropTable("share_info");
  result = ExecQuery(load_dbhandle_,
      "ALTER TABLE temp_share_info RENAME TO share_info");
  if (result != SQLITE_DONE)
    return false;
  SetVersion(71);
  return true;
}

bool DirectoryBackingStore::MigrateVersion71To72() {
  // Version 72 removed a table 'extended_attributes', whose
  // contents didn't matter.
  SafeDropTable("extended_attributes");
  SetVersion(72);
  return true;
}

bool DirectoryBackingStore::MigrateVersion72To73() {
  // Version 73 added one column to the table 'share_info': notification_state
  int result =
      ExecQuery(load_dbhandle_,
                "ALTER TABLE share_info ADD COLUMN notification_state BLOB");
  if (result != SQLITE_DONE)
    return false;
  SetVersion(73);
  return true;
}

bool DirectoryBackingStore::MigrateVersion73To74() {
  // Version 74 added the following columns to the table 'share_info':
  //   autofill_migration_state
  //   bookmarks_added_during_autofill_migration
  //   autofill_migration_time
  //   autofill_entries_added_during_migration
  //   autofill_profiles_added_during_migration

  int result =
      ExecQuery(load_dbhandle_,
                "ALTER TABLE share_info ADD COLUMN autofill_migration_state "
                "INT default 0");
  if (result != SQLITE_DONE)
    return false;

  result = ExecQuery(load_dbhandle_,
                "ALTER TABLE share_info ADD COLUMN "
                "bookmarks_added_during_autofill_migration "
                "INT default 0");

  if (result != SQLITE_DONE)
    return false;

  result = ExecQuery(load_dbhandle_,
                "ALTER TABLE share_info ADD COLUMN autofill_migration_time "
                "INT default 0");

  if (result != SQLITE_DONE)
    return false;

  result = ExecQuery(load_dbhandle_,
                "ALTER TABLE share_info ADD COLUMN "
                "autofill_entries_added_during_migration "
                "INT default 0");

  if (result != SQLITE_DONE)
    return false;

  result = ExecQuery(load_dbhandle_,
                "ALTER TABLE share_info ADD COLUMN "
                "autofill_profiles_added_during_migration "
                "INT default 0");

  if (result != SQLITE_DONE)
    return false;

  SetVersion(74);
  return true;
}

bool DirectoryBackingStore::MigrateVersion74To75() {
  // In version 74, there was a table 'models':
  //     blob model_id (entity specifics, primary key)
  //     int last_download_timestamp
  //     boolean initial_sync_ended
  // In version 75, we deprecated the integer-valued last_download_timestamp,
  // using insted a protobuf-valued progress_marker field:
  //     blob progress_marker
  // The progress_marker values are initialized from the value of
  // last_download_timestamp, thereby preserving the download state.

  // Move aside the old table and create a new empty one at the current schema.
  if (SQLITE_DONE != ExecQuery(load_dbhandle_,
          "ALTER TABLE models RENAME TO temp_models")) {
    return false;
  }
  if (!CreateModelsTable())
    return false;

  sqlite_utils::SQLStatement query;
  query.prepare(load_dbhandle_,
      "SELECT model_id, last_download_timestamp, initial_sync_ended "
      "FROM temp_models");
  while (SQLITE_ROW == query.step()) {
    ModelType type = ModelIdToModelTypeEnum(query.column_blob(0),
                                            query.column_bytes(0));
    if (type != UNSPECIFIED) {
      // Set the |timestamp_token_for_migration| on a new
      // DataTypeProgressMarker, using the old value of last_download_timestamp.
      // The server will turn this into a real token on our behalf the next
      // time we check for updates.
      sync_pb::DataTypeProgressMarker progress_marker;
      progress_marker.set_data_type_id(
          GetExtensionFieldNumberFromModelType(type));
      progress_marker.set_timestamp_token_for_migration(query.column_int64(1));
      std::string progress_blob;
      progress_marker.SerializeToString(&progress_blob);

      sqlite_utils::SQLStatement update;
      update.prepare(load_dbhandle_, "INSERT INTO models (model_id, "
          "progress_marker, initial_sync_ended) VALUES (?, ?, ?)");
      update.bind_blob(0, query.column_blob(0), query.column_bytes(0));
      update.bind_blob(1, progress_blob.data(), progress_blob.length());
      update.bind_bool(2, query.column_bool(2));
      if (SQLITE_DONE != update.step())
        return false;
    }
  }
  // Drop the old table.
  SafeDropTable("temp_models");

  SetVersion(75);
  return true;
}

bool DirectoryBackingStore::MigrateVersion75To76() {
  // This change removed five columns:
  //   autofill_migration_state
  //   bookmarks_added_during_autofill_migration
  //   autofill_migration_time
  //   autofill_entries_added_during_migration
  //   autofill_profiles_added_during_migration
  // No data migration is necessary, but we should do a column refresh.
  SetVersion(76);
  needs_column_refresh_ = true;
  return true;
}

bool DirectoryBackingStore::MigrateVersion76To77() {
  sqlite_utils::SQLStatement update_timestamps;
  // This change changes the format of stored timestamps to ms since
  // the Unix epoch.
#if defined(OS_WIN)
// On Windows, we used to store timestamps in FILETIME format (100s of
// ns since Jan 1, 1601).  Magic numbers taken from
// http://stackoverflow.com/questions/5398557/java-library-for-dealing-with-win32-filetime
// .
#define TO_UNIX_TIME_MS(x) #x " = " #x " / 10000 - 11644473600000"
#else
// On other platforms, we used to store timestamps in time_t format (s
// since the Unix epoch).
#define TO_UNIX_TIME_MS(x) #x " = " #x " * 1000"
#endif
  update_timestamps.prepare(
      load_dbhandle_,
      "UPDATE metas SET "
      TO_UNIX_TIME_MS(mtime) ", "
      TO_UNIX_TIME_MS(server_mtime) ", "
      TO_UNIX_TIME_MS(ctime) ", "
      TO_UNIX_TIME_MS(server_ctime));
#undef TO_UNIX_TIME_MS
  if (update_timestamps.step() != SQLITE_DONE)
    return false;
  SetVersion(77);
  return true;
}

int DirectoryBackingStore::CreateTables() {
  DVLOG(1) << "First run, creating tables";
  // Create two little tables share_version and share_info
  int result = ExecQuery(load_dbhandle_,
                         "CREATE TABLE share_version ("
                         "id VARCHAR(128) primary key, data INT)");
  if (result != SQLITE_DONE)
    return result;
  {
    sqlite_utils::SQLStatement statement;
    statement.prepare(load_dbhandle_, "INSERT INTO share_version VALUES(?, ?)");
    statement.bind_string(0, dir_name_);
    statement.bind_int(1, kCurrentDBVersion);
    result = statement.step();
  }
  if (result != SQLITE_DONE)
    return result;

  const bool kCreateAsTempShareInfo = false;
  result = CreateShareInfoTable(kCreateAsTempShareInfo);
  if (result != SQLITE_DONE)
    return result;
  {
    sqlite_utils::SQLStatement statement;
    statement.prepare(load_dbhandle_, "INSERT INTO share_info VALUES"
                                      "(?, "  // id
                                      "?, "   // name
                                      "?, "   // store_birthday
                                      "?, "   // db_create_version
                                      "?, "   // db_create_time
                                      "-2, "  // next_id
                                      "?, "   // cache_guid
                                      "?);");  // notification_state
    statement.bind_string(0, dir_name_);                   // id
    statement.bind_string(1, dir_name_);                   // name
    statement.bind_string(2, "");                          // store_birthday
    statement.bind_string(3, SYNC_ENGINE_VERSION_STRING);  // db_create_version
    statement.bind_int(4, static_cast<int32>(time(0)));    // db_create_time
    statement.bind_string(5, GenerateCacheGUID());         // cache_guid
    statement.bind_blob(11, NULL, 0);                      // notification_state
    result = statement.step();
  }
  if (result != SQLITE_DONE)
    return result;

  result = CreateModelsTable();
  if (result != SQLITE_DONE)
    return result;
  // Create the big metas table.
  result = CreateMetasTable(false);
  if (result != SQLITE_DONE)
    return result;
  {
    // Insert the entry for the root into the metas table.
    const int64 now = browser_sync::TimeToProtoTime(base::Time::Now());
    sqlite_utils::SQLStatement statement;
    statement.prepare(load_dbhandle_,
                      "INSERT INTO metas "
                      "( id, metahandle, is_dir, ctime, mtime) "
                      "VALUES ( \"r\", 1, 1, ?, ?)");
    statement.bind_int64(0, now);
    statement.bind_int64(1, now);
    result = statement.step();
  }
  return result;
}

sqlite3* DirectoryBackingStore::LazyGetSaveHandle() {
  if (!save_dbhandle_ && !OpenAndConfigureHandleHelper(&save_dbhandle_)) {
    NOTREACHED() << "Unable to open handle for saving";
    return NULL;
  }
  return save_dbhandle_;
}

int DirectoryBackingStore::CreateMetasTable(bool is_temporary) {
  const char* name = is_temporary ? "temp_metas" : "metas";
  string query = "CREATE TABLE ";
  query.append(name);
  query.append(ComposeCreateTableColumnSpecs());
  return ExecQuery(load_dbhandle_, query.c_str());
}

int DirectoryBackingStore::CreateV71ModelsTable() {
  // This is an old schema for the Models table, used from versions 71 to 74.
  return ExecQuery(load_dbhandle_,
      "CREATE TABLE models ("
      "model_id BLOB primary key, "
      "last_download_timestamp INT, "
      // Gets set if the syncer ever gets updates from the
      // server and the server returns 0.  Lets us detect the
      // end of the initial sync.
      "initial_sync_ended BOOLEAN default 0)");
}

int DirectoryBackingStore::CreateModelsTable() {
  // This is the current schema for the Models table, from version 75
  // onward.  If you change the schema, you'll probably want to double-check
  // the use of this function in the v74-v75 migration.
  return ExecQuery(load_dbhandle_,
      "CREATE TABLE models ("
      "model_id BLOB primary key, "
      "progress_marker BLOB, "
      // Gets set if the syncer ever gets updates from the
      // server and the server returns 0.  Lets us detect the
      // end of the initial sync.
      "initial_sync_ended BOOLEAN default 0)");
}

int DirectoryBackingStore::CreateShareInfoTable(bool is_temporary) {
  const char* name = is_temporary ? "temp_share_info" : "share_info";
  string query = "CREATE TABLE ";
  query.append(name);
  // This is the current schema for the ShareInfo table, from version 76
  // onward.
  query.append(" ("
      "id TEXT primary key, "
      "name TEXT, "
      "store_birthday TEXT, "
      "db_create_version TEXT, "
      "db_create_time INT, "
      "next_id INT default -2, "
      "cache_guid TEXT ");

  query.append(", notification_state BLOB");
  query.append(")");
  return ExecQuery(load_dbhandle_, query.c_str());
}

int DirectoryBackingStore::CreateShareInfoTableVersion71(
    bool is_temporary) {
  const char* name = is_temporary ? "temp_share_info" : "share_info";
  string query = "CREATE TABLE ";
  query.append(name);
  // This is the schema for the ShareInfo table used from versions 71 to 72.
  query.append(" ("
      "id TEXT primary key, "
      "name TEXT, "
      "store_birthday TEXT, "
      "db_create_version TEXT, "
      "db_create_time INT, "
      "next_id INT default -2, "
      "cache_guid TEXT )");
  return ExecQuery(load_dbhandle_, query.c_str());
}
}  // namespace syncable
