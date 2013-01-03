// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_gallery_database.h"

#include <algorithm>
#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "sql/statement.h"
#include "sql/transaction.h"

#if defined(OS_WIN)
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#endif  // OS_WIN

#define MEDIA_GALLERY_COLLECTION_ROW_FIELDS \
    " collections.id, collections.path, collections.last_modified_time, " \
    "collections.entry_count, collections.all_parsed "

namespace chrome {

namespace {

const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;
const FilePath::CharType kMediaGalleryDatabaseName[] =
    FILE_PATH_LITERAL(".media_gallery.db");

}  // namespace

MediaGalleryDatabase::MediaGalleryDatabase() { }

MediaGalleryDatabase::~MediaGalleryDatabase() { }

sql::InitStatus MediaGalleryDatabase::Init(const FilePath& database_dir) {
  db_.set_error_histogram_name("Sqlite.MediaGallery.Error");

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  // Increase the cache size. The page size, plus a little extra, times this
  // value, tells us how much memory the cache will use maximum.
  // 6000 * 4KB = 24MB
  db_.set_cache_size(6000);

  if (!db_.Open(database_dir.Append(FilePath(kMediaGalleryDatabaseName))))
    return sql::INIT_FAILURE;

  return InitInternal(&db_);
}

sql::InitStatus MediaGalleryDatabase::InitInternal(sql::Connection* db) {
  // Wrap the rest of init in a tranaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  sql::Transaction committer(db);
  if (!committer.Begin())
    return sql::INIT_FAILURE;

  // Prime the cache.
  db->Preload();

  // Create the tables and indices.
  if (!meta_table_.Init(db, GetCurrentVersion(), kCompatibleVersionNumber))
    return sql::INIT_FAILURE;

  if (!CreateCollectionsTable(db))
    return sql::INIT_FAILURE;

  // Version check.
  sql::InitStatus version_status = EnsureCurrentVersion();
  if (version_status != sql::INIT_OK)
    return version_status;

  return committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

sql::Connection& MediaGalleryDatabase::GetDB() {
  return db_;
}

sql::InitStatus MediaGalleryDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    VLOG(1) << "Media Gallery database is too new.";
    return sql::INIT_TOO_NEW;
  }
  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 0) {
    DVLOG(1) << "Initializing the Media Gallery database.";

    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }
  return sql::INIT_OK;
}

// static
int MediaGalleryDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

CollectionId MediaGalleryDatabase::CreateCollectionRow(
    CollectionRow* row) {
  const char* sql = "INSERT INTO collections"
      "(path, last_modified_time, entry_count, all_parsed) "
      "VALUES(?, ?, ?, ?)";

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE, sql));
#if defined(OS_WIN)
  // We standardize on UTF8 encoding for paths.
  std::string path(base::SysWideToUTF8(row->path.value()));
#elif defined(OS_POSIX)
  std::string path(row->path.value());
#endif

#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  // We standardize on POSIX-style paths, since any platform can understand
  // them.
  ReplaceChars(path, "\\", "/", &path);
#endif  // FILE_PATH_USES_WIN_SEPARATORS

  statement.BindString(0, path.c_str());
  statement.BindInt64(1, row->last_modified_time.ToInternalValue());
  statement.BindInt(2, row->entry_count);
  statement.BindInt(3, row->all_parsed ? 1 : 0);

  if (!statement.Run()) {
    VLOG(0) << "Failed to add collection " << row->path.value()
            << " to table media_gallery.collections";
    return 0;
  }
  return row->id = GetDB().GetLastInsertRowId();
}

bool MediaGalleryDatabase::GetCollectionRow(CollectionId id,
                                            CollectionRow* row) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" MEDIA_GALLERY_COLLECTION_ROW_FIELDS
      "FROM collections WHERE id=?"));
  statement.BindInt64(0, id);

  if (statement.Step()) {
    FillCollectionRow(statement, row);
    return true;
  }
  return false;
}

// Convenience method to fill a row from a statement. Must be in sync with the
// columns in MEDIA_GALLERY_COLLECTION_ROW_FIELDS
void MediaGalleryDatabase::FillCollectionRow(const sql::Statement& statement,
                                             CollectionRow* row) {
  row->id = statement.ColumnInt64(0);
#if defined(OS_WIN)
  row->path = FilePath(base::SysUTF8ToWide(statement.ColumnString(1)));
#elif defined(OS_POSIX)
  row->path = FilePath(statement.ColumnString(1));
#endif  // OS_WIN
  row->last_modified_time = base::Time::FromInternalValue(
      statement.ColumnInt64(2));
  row->entry_count = statement.ColumnInt(3);
  row->all_parsed = statement.ColumnInt(4) != 0;
}

// static
bool MediaGalleryDatabase::DoesCollectionsTableExist(sql::Connection *db) {
  return db->DoesTableExist("collections");
}

// static
bool MediaGalleryDatabase::CreateCollectionsTable(sql::Connection* db) {
  if (DoesCollectionsTableExist(db))
    return true;

  // TODO(tpayne): Add unique constraint on path once we standardize on path
  // format.
  return db->Execute("CREATE TABLE collections"
                     "(id INTEGER PRIMARY KEY,"
                     " path LONGVARCHAR NOT NULL,"
                     " last_modified_time INTEGER NOT NULL,"
                     " entry_count INTEGER DEFAULT 0 NOT NULL,"
                     " all_parsed INTEGER DEFAULT 0 NOT NULL)");
}

}  // namespace chrome
