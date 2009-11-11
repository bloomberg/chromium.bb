// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/thumbnail_database.h"

#include "app/gfx/codec/jpeg_codec.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/file_util.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif
#include "base/time.h"
#include "base/string_util.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/history/history_publisher.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/thumbnail_score.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace history {

// Version number of the database.
static const int kCurrentVersionNumber = 3;
static const int kCompatibleVersionNumber = 3;

ThumbnailDatabase::ThumbnailDatabase() : history_publisher_(NULL) {
}

ThumbnailDatabase::~ThumbnailDatabase() {
  // The DBCloseScoper will delete the DB and the cache.
}

InitStatus ThumbnailDatabase::Init(const FilePath& db_name,
                                   const HistoryPublisher* history_publisher) {
  history_publisher_ = history_publisher;

  // Set the exceptional sqlite error handler.
  db_.set_error_delegate(GetErrorHandlerForThumbnailDb());

  // Set the database page size to something  larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192. We use a bigger
  // one because we're storing larger data (4-16K) in it, so we want a few
  // blocks per element.
  db_.set_page_size(4096);

  // The UI is generally designed to work well when the thumbnail database is
  // slow, so we can tolerate much less caching. The file is also very large
  // and so caching won't save a significant percentage of it for us,
  // reducing the benefit of caching in the first place. With the default cache
  // size of 2000 pages, it will take >8MB of memory, so reducing it can be a
  // big savings.
  db_.set_cache_size(64);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  if (!db_.Open(db_name))
    return INIT_FAILURE;

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(&db_);
  transaction.Begin();

#if defined(OS_MACOSX)
  // Exclude the thumbnails file and its journal from backups.
  mac_util::SetFileBackupExclusion(db_name, true);
  FilePath::StringType db_name_string(db_name.value());
  db_name_string += "-journal";
  FilePath db_journal_name(db_name_string);
  mac_util::SetFileBackupExclusion(db_journal_name, true);
#endif

  // Create the tables.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber) ||
      !InitThumbnailTable() ||
      !InitFavIconsTable(false)) {
    db_.Close();
    return INIT_FAILURE;
  }
  InitFavIconsIndex();

  // Version check. We should not encounter a database too old for us to handle
  // in the wild, so we try to continue in that case.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Thumbnail database is too new.";
    return INIT_TOO_NEW;
  }

  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 2) {
    if (!UpgradeToVersion3()) {
      LOG(WARNING) << "Unable to update to thumbnail database to version 3.";
      db_.Close();
      return INIT_FAILURE;
    }
    ++cur_version;
  }

  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Thumbnail database version " << cur_version << " is too old to handle.";

  // Initialization is complete.
  if (!transaction.Commit()) {
    db_.Close();
    return INIT_FAILURE;
  }

  // The following code is useful in debugging the thumbnail database. Upon
  // startup, it will spit out a file for each thumbnail in the database so you
  // can open them in an external viewer. Insert the path name on your system
  // into the string below (I recommend using a blank directory since there may
  // be a lot of files).
#if 0
  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT id, image_data FROM favicons"));
  while (statement.Step()) {
    int idx = statement.ColumnInt(0);
    std::vector<unsigned char> data;
    statement.ColumnBlobAsVector(1, &data);

    char filename[256];
    sprintf(filename, "<<< YOUR PATH HERE >>>\\%d.png", idx);
    if (!data.empty()) {
      file_util::WriteFile(ASCIIToWide(std::string(filename)),
                           reinterpret_cast<char*>(&data[0]),
                           data.size());
    }
  }
#endif

  return INIT_OK;
}

bool ThumbnailDatabase::InitThumbnailTable() {
  if (!db_.DoesTableExist("thumbnails")) {
    if (!db_.Execute("CREATE TABLE thumbnails ("
        "url_id INTEGER PRIMARY KEY,"
        "boring_score DOUBLE DEFAULT 1.0,"
        "good_clipping INTEGER DEFAULT 0,"
        "at_top INTEGER DEFAULT 0,"
        "last_updated INTEGER DEFAULT 0,"
        "data BLOB)"))
      return false;
  }
  return true;
}

bool ThumbnailDatabase::UpgradeToVersion3() {
  // sqlite doesn't like the "ALTER TABLE xxx ADD (column_one, two,
  // three)" syntax, so list out the commands we need to execute:
  const char* alterations[] = {
    "ALTER TABLE thumbnails ADD boring_score DOUBLE DEFAULT 1.0",
    "ALTER TABLE thumbnails ADD good_clipping INTEGER DEFAULT 0",
    "ALTER TABLE thumbnails ADD at_top INTEGER DEFAULT 0",
    "ALTER TABLE thumbnails ADD last_updated INTEGER DEFAULT 0",
    NULL
  };

  for (int i = 0; alterations[i] != NULL; ++i) {
    if (!db_.Execute(alterations[i])) {
      NOTREACHED();
      return false;
    }
  }

  meta_table_.SetVersionNumber(3);
  meta_table_.SetCompatibleVersionNumber(std::min(3, kCompatibleVersionNumber));
  return true;
}

bool ThumbnailDatabase::RecreateThumbnailTable() {
  if (!db_.Execute("DROP TABLE thumbnails"))
    return false;
  return InitThumbnailTable();
}

bool ThumbnailDatabase::InitFavIconsTable(bool is_temporary) {
  // Note: if you update the schema, don't forget to update
  // CopyToTemporaryFaviconTable as well.
  const char* name = is_temporary ? "temp_favicons" : "favicons";
  if (!db_.DoesTableExist(name)) {
    std::string sql;
    sql.append("CREATE TABLE ");
    sql.append(name);
    sql.append("("
               "id INTEGER PRIMARY KEY,"
               "url LONGVARCHAR NOT NULL,"
               "last_updated INTEGER DEFAULT 0,"
               "image_data BLOB)");
    if (!db_.Execute(sql.c_str()))
      return false;
  }
  return true;
}

void ThumbnailDatabase::InitFavIconsIndex() {
  // Add an index on the url column. We ignore errors. Since this is always
  // called during startup, the index will normally already exist.
  db_.Execute("CREATE INDEX favicons_url ON favicons(url)");
}

void ThumbnailDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void ThumbnailDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

void ThumbnailDatabase::Vacuum() {
  DCHECK(db_.transaction_nesting() == 0) <<
      "Can not have a transaction when vacuuming.";
  db_.Execute("VACUUM");
}

void ThumbnailDatabase::SetPageThumbnail(
    const GURL& url,
    URLID id,
    const SkBitmap& thumbnail,
    const ThumbnailScore& score,
    base::Time time) {
  if (!thumbnail.isNull()) {
    bool add_thumbnail = true;
    ThumbnailScore current_score;
    if (ThumbnailScoreForId(id, &current_score)) {
      add_thumbnail = ShouldReplaceThumbnailWith(current_score, score);
    }

    if (add_thumbnail) {
      sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
          "INSERT OR REPLACE INTO thumbnails "
          "(url_id, boring_score, good_clipping, at_top, last_updated, data) "
          "VALUES (?,?,?,?,?,?)"));
      if (!statement)
        return;

      // We use 90 quality (out of 100) which is pretty high, because
      // we're very sensitive to artifacts for these small sized,
      // highly detailed images.
      std::vector<unsigned char> jpeg_data;
      SkAutoLockPixels thumbnail_lock(thumbnail);
      bool encoded = gfx::JPEGCodec::Encode(
          reinterpret_cast<unsigned char*>(thumbnail.getAddr32(0, 0)),
          gfx::JPEGCodec::FORMAT_BGRA, thumbnail.width(),
          thumbnail.height(),
          static_cast<int>(thumbnail.rowBytes()), 90,
          &jpeg_data);

      if (encoded) {
        statement.BindInt64(0, id);
        statement.BindDouble(1, score.boring_score);
        statement.BindBool(2, score.good_clipping);
        statement.BindBool(3, score.at_top);
        statement.BindInt64(4, score.time_at_snapshot.ToTimeT());
        statement.BindBlob(5, &jpeg_data[0],
                           static_cast<int>(jpeg_data.size()));
        if (!statement.Run())
          NOTREACHED() << db_.GetErrorMessage();
      }

      // Publish the thumbnail to any indexers listening to us.
      // The tests may send an invalid url. Hence avoid publishing those.
      if (url.is_valid() && history_publisher_ != NULL)
        history_publisher_->PublishPageThumbnail(jpeg_data, url, time);
    }
  } else {
    if (!DeleteThumbnail(id) )
      DLOG(WARNING) << "Unable to delete thumbnail";
  }
}

bool ThumbnailDatabase::GetPageThumbnail(URLID id,
                                         std::vector<unsigned char>* data) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT data FROM thumbnails WHERE url_id=?"));
  if (!statement)
    return false;

  statement.BindInt64(0, id);
  if (!statement.Step())
    return false;  // don't have a thumbnail for this ID

  statement.ColumnBlobAsVector(0, data);
  return true;
}

bool ThumbnailDatabase::DeleteThumbnail(URLID id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM thumbnails WHERE url_id = ?"));
  if (!statement)
    return false;

  statement.BindInt64(0, id);
  return statement.Run();
}

bool ThumbnailDatabase::ThumbnailScoreForId(URLID id,
                                            ThumbnailScore* score) {
  // Fetch the current thumbnail's information to make sure we
  // aren't replacing a good thumbnail with one that's worse.
  sql::Statement select_statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT boring_score, good_clipping, at_top, last_updated "
      "FROM thumbnails WHERE url_id=?"));
  if (!select_statement) {
    NOTREACHED() << "Couldn't build select statement!";
  } else {
    select_statement.BindInt64(0, id);
    if (select_statement.Step()) {
      double current_boring_score = select_statement.ColumnDouble(0);
      bool current_clipping = select_statement.ColumnBool(1);
      bool current_at_top = select_statement.ColumnBool(2);
      base::Time last_updated =
          base::Time::FromTimeT(select_statement.ColumnInt64(3));
      *score = ThumbnailScore(current_boring_score, current_clipping,
                              current_at_top, last_updated);
      return true;
    }
  }

  return false;
}

bool ThumbnailDatabase::SetFavIcon(URLID icon_id,
                                   const std::vector<unsigned char>& icon_data,
                                   base::Time time) {
  DCHECK(icon_id);
  if (icon_data.size()) {
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
        "UPDATE favicons SET image_data=?, last_updated=? WHERE id=?"));
    if (!statement)
      return 0;

    statement.BindBlob(0, &icon_data.front(),
                       static_cast<int>(icon_data.size()));
    statement.BindInt64(1, time.ToTimeT());
    statement.BindInt64(2, icon_id);
    return statement.Run();
  } else {
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
        "UPDATE favicons SET image_data=NULL, last_updated=? WHERE id=?"));
    if (!statement)
      return 0;

    statement.BindInt64(0, time.ToTimeT());
    statement.BindInt64(1, icon_id);
    return statement.Run();
  }
}

bool ThumbnailDatabase::SetFavIconLastUpdateTime(FavIconID icon_id,
                                                 base::Time time) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE favicons SET last_updated=? WHERE id=?"));
  if (!statement)
    return 0;

  statement.BindInt64(0, time.ToTimeT());
  statement.BindInt64(1, icon_id);
  return statement.Run();
}

FavIconID ThumbnailDatabase::GetFavIconIDForFavIconURL(const GURL& icon_url) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id FROM favicons WHERE url=?"));
  if (!statement)
    return 0;

  statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));
  if (!statement.Step())
    return 0;  // not cached

  return statement.ColumnInt64(0);
}

bool ThumbnailDatabase::GetFavIcon(
    FavIconID icon_id,
    base::Time* last_updated,
    std::vector<unsigned char>* png_icon_data,
    GURL* icon_url) {
  DCHECK(icon_id);

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT last_updated, image_data, url FROM favicons WHERE id=?"));
  if (!statement)
    return 0;

  statement.BindInt64(0, icon_id);

  if (!statement.Step())
    return false;  // No entry for the id.

  *last_updated = base::Time::FromTimeT(statement.ColumnInt64(0));
  if (statement.ColumnByteLength(1) > 0)
    statement.ColumnBlobAsVector(1, png_icon_data);
  if (icon_url)
    *icon_url = GURL(statement.ColumnString(2));

  return true;
}

FavIconID ThumbnailDatabase::AddFavIcon(const GURL& icon_url) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO favicons (url) VALUES (?)"));
  if (!statement)
    return 0;

  statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));
  if (!statement.Run())
    return 0;
  return db_.GetLastInsertRowId();
}

bool ThumbnailDatabase::DeleteFavIcon(FavIconID id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM favicons WHERE id = ?"));
  if (!statement)
    return false;
  statement.BindInt64(0, id);
  return statement.Run();
}

FavIconID ThumbnailDatabase::CopyToTemporaryFavIconTable(FavIconID source) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO temp_favicons (url, last_updated, image_data)"
      "SELECT url, last_updated, image_data "
      "FROM favicons WHERE id = ?"));
  if (!statement)
    return 0;
  statement.BindInt64(0, source);
  if (!statement.Run())
    return 0;

  // We return the ID of the newly inserted favicon.
  return db_.GetLastInsertRowId();
}

bool ThumbnailDatabase::CommitTemporaryFavIconTable() {
  // Delete the old favicons table.
  if (!db_.Execute("DROP TABLE favicons"))
    return false;

  // Rename the temporary one.
  if (!db_.Execute("ALTER TABLE temp_favicons RENAME TO favicons"))
    return false;

  // The renamed table needs the index (the temporary table doesn't have one).
  InitFavIconsIndex();
  return true;
}

}  // namespace history
