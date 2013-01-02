// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/thumbnail_database.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/history/history_publisher.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/thumbnail_score.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/gfx/image/image_util.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

// Description of database tables:
//
// icon_mapping
//   id               Unique ID.
//   page_url         Page URL which has one or more associated favicons.
//   icon_id          The ID of favicon that this mapping maps to.
//
// favicons           This table associates a row to each favicon for a
//                    |page_url| in the |icon_mapping| table. This is the
//                    default favicon |page_url|/favicon.ico plus any favicons
//                    associated via <link rel="icon_type" href="url">.
//                    The |id| matches the |icon_id| field in the appropriate
//                    row in the icon_mapping table.
//
//   id               Unique ID.
//   url              The URL at which the favicon file is located.
//   icon_type        The type of the favicon specified in the rel attribute of
//                    the link tag. The FAVICON type is used for the default
//                    favicon.ico favicon.
//   sizes            Sizes is a listing of all the sizes at which the favicon
//                    at |url| is available from the web. Note that this may
//                    include sizes for which a bitmap is not stored in the
//                    favicon_bitmaps table. Each widthxheight pair is
//                    separated by a space. Width and height are separated by
//                    a space as well. For instance, if |icon_id| represents a
//                    .ico file containing 16x16 and 32x32 bitmaps, |sizes|
//                    would be "16 16 32 32".
//
// favicon_bitmaps    This table contains the PNG encoded bitmap data of the
//                    favicons. There is a separate row for every size in a
//                    multi resolution bitmap. The bitmap data is associated
//                    to the favicon via the |icon_id| field which matches
//                    the |id| field in the appropriate row in the |favicons|
//                    table. There is not necessarily a row for each size
//                    specified in the sizes attribute.
//
//  id                Unique ID.
//  icon_id           The ID of the favicon that the bitmap is associated to.
//  last_updated      The time at which this favicon was inserted into the
//                    table. This is used to determine if it needs to be
//                    redownloaded from the web.
//  image_data        PNG encoded data of the favicon.
//  width             Pixel width of |image_data|.
//  height            Pixel height of |image_data|.

static void FillIconMapping(const sql::Statement& statement,
                            const GURL& page_url,
                            history::IconMapping* icon_mapping) {
  icon_mapping->mapping_id = statement.ColumnInt64(0);
  icon_mapping->icon_id = statement.ColumnInt64(1);
  icon_mapping->icon_type =
      static_cast<history::IconType>(statement.ColumnInt(2));
  icon_mapping->icon_url = GURL(statement.ColumnString(3));
  icon_mapping->page_url = page_url;
}

namespace history {

// Version number of the database.
static const int kCurrentVersionNumber = 6;
static const int kCompatibleVersionNumber = 6;

// Use 90 quality (out of 100) which is pretty high, because we're very
// sensitive to artifacts for these small sized, highly detailed images.
static const int kImageQuality = 90;

ThumbnailDatabase::IconMappingEnumerator::IconMappingEnumerator() {
}

ThumbnailDatabase::IconMappingEnumerator::~IconMappingEnumerator() {
}

bool ThumbnailDatabase::IconMappingEnumerator::GetNextIconMapping(
    IconMapping* icon_mapping) {
  if (!statement_.Step())
    return false;
  FillIconMapping(statement_, GURL(statement_.ColumnString(4)), icon_mapping);
  return true;
}

ThumbnailDatabase::ThumbnailDatabase()
    : history_publisher_(NULL),
      use_top_sites_(false) {
}

sql::InitStatus ThumbnailDatabase::CantUpgradeToVersion(int cur_version) {
  LOG(WARNING) << "Unable to update to thumbnail database to version " <<
               cur_version << ".";
  db_.Close();
  return sql::INIT_FAILURE;
}

ThumbnailDatabase::~ThumbnailDatabase() {
  // The DBCloseScoper will delete the DB and the cache.
}

sql::InitStatus ThumbnailDatabase::Init(
    const FilePath& db_name,
    const HistoryPublisher* history_publisher,
    URLDatabase* url_db) {
  history_publisher_ = history_publisher;
  sql::InitStatus status = OpenDatabase(&db_, db_name);
  if (status != sql::INIT_OK)
    return status;

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(&db_);
  transaction.Begin();

#if defined(OS_MACOSX)
  // Exclude the thumbnails file from backups.
  base::mac::SetFileBackupExclusion(db_name);
#endif

  // Create the tables.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber) ||
      !InitThumbnailTable() ||
      !InitFaviconBitmapsTable(&db_, false) ||
      !InitFaviconBitmapsIndex() ||
      !InitFaviconsTable(&db_, false) ||
      !InitFaviconsIndex() ||
      !InitIconMappingTable(&db_, false) ||
      !InitIconMappingIndex()) {
    db_.Close();
    return sql::INIT_FAILURE;
  }

  // Version check. We should not encounter a database too old for us to handle
  // in the wild, so we try to continue in that case.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Thumbnail database is too new.";
    return sql::INIT_TOO_NEW;
  }

  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 2) {
    ++cur_version;
    if (!UpgradeToVersion3())
      return CantUpgradeToVersion(cur_version);
  }

  if (cur_version == 3) {
    ++cur_version;
    if (!UpgradeToVersion4() || !MigrateIconMappingData(url_db))
      return CantUpgradeToVersion(cur_version);
  }

  if (cur_version == 4) {
    ++cur_version;
    if (!UpgradeToVersion5())
      return CantUpgradeToVersion(cur_version);
  }

  if (cur_version == 5) {
    ++cur_version;
    if (!UpgradeToVersion6())
      return CantUpgradeToVersion(cur_version);
  }

  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Thumbnail database version " << cur_version << " is too old to handle.";

  // Initialization is complete.
  if (!transaction.Commit()) {
    db_.Close();
    return sql::INIT_FAILURE;
  }

  // Raze the database if the structure of the favicons database is not what
  // it should be. This error cannot be detected via the SQL error code because
  // the error code for running SQL statements against a database with missing
  // columns is SQLITE_ERROR which is not unique enough to act upon.
  // TODO(pkotwicz): Revisit this in M27 and see if the razing can be removed.
  // (crbug.com/166453)
  if (IsFaviconDBStructureIncorrect()) {
    LOG(ERROR) << "Raze thumbnail database because of invalid favicon db"
               << "structure.";
    UMA_HISTOGRAM_BOOLEAN("History.InvalidFaviconsDBStructure", true);

    db_.Raze();
    db_.Close();
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

sql::InitStatus ThumbnailDatabase::OpenDatabase(sql::Connection* db,
                                                const FilePath& db_name) {
  // Set the exceptional sqlite error handler.
  db->set_error_delegate(GetErrorHandlerForThumbnailDb());

  // Thumbnails db now only stores favicons, so we don't need that big a page
  // size or cache.
  db->set_page_size(2048);
  db->set_cache_size(32);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db->set_exclusive_locking();

  if (!db->Open(db_name))
    return sql::INIT_FAILURE;

  return sql::INIT_OK;
}

bool ThumbnailDatabase::InitThumbnailTable() {
  if (!db_.DoesTableExist("thumbnails")) {
    use_top_sites_ = true;
  }
  return true;
}

bool ThumbnailDatabase::UpgradeToVersion3() {
  if (use_top_sites_) {
    meta_table_.SetVersionNumber(3);
    meta_table_.SetCompatibleVersionNumber(
        std::min(3, kCompatibleVersionNumber));
    return true;  // Not needed after migration to TopSites.
  }

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
      return false;
    }
  }

  meta_table_.SetVersionNumber(3);
  meta_table_.SetCompatibleVersionNumber(std::min(3, kCompatibleVersionNumber));
  return true;
}

bool ThumbnailDatabase::RecreateThumbnailTable() {
  if (use_top_sites_)
    return true;  // Not needed after migration to TopSites.

  if (!db_.Execute("DROP TABLE thumbnails"))
    return false;
  return InitThumbnailTable();
}

bool ThumbnailDatabase::InitFaviconsTable(sql::Connection* db,
                                          bool is_temporary) {
  // Note: if you update the schema, don't forget to update
  // CopyFaviconAndFaviconBitmapsToTemporaryTables as well.
  const char* name = is_temporary ? "temp_favicons" : "favicons";
  if (!db->DoesTableExist(name)) {
    std::string sql;
    sql.append("CREATE TABLE ");
    sql.append(name);
    sql.append("("
               "id INTEGER PRIMARY KEY,"
               "url LONGVARCHAR NOT NULL,"
               // Set the default icon_type as FAVICON to be consistent with
               // table upgrade in UpgradeToVersion4().
               "icon_type INTEGER DEFAULT 1,"
               "sizes LONGVARCHAR)");
    if (!db->Execute(sql.c_str()))
      return false;
  }
  return true;
}

bool ThumbnailDatabase::InitFaviconsIndex() {
  // Add an index on the url column.
  return
      db_.Execute("CREATE INDEX IF NOT EXISTS favicons_url ON favicons(url)");
}

bool ThumbnailDatabase::InitFaviconBitmapsTable(sql::Connection* db,
                                                bool is_temporary) {
  // Note: if you update the schema, don't forget to update
  // CopyFaviconAndFaviconBitmapsToTemporaryTables as well.
  const char* name = is_temporary ? "temp_favicon_bitmaps" : "favicon_bitmaps";
  if (!db->DoesTableExist(name)) {
    std::string sql;
    sql.append("CREATE TABLE ");
    sql.append(name);
    sql.append("("
               "id INTEGER PRIMARY KEY,"
               "icon_id INTEGER NOT NULL,"
               "last_updated INTEGER DEFAULT 0,"
               "image_data BLOB,"
               "width INTEGER DEFAULT 0,"
               "height INTEGER DEFAULT 0)");
    if (!db->Execute(sql.c_str()))
      return false;
  }
  return true;
}

bool ThumbnailDatabase::InitFaviconBitmapsIndex() {
  // Add an index on the icon_id column.
  return db_.Execute("CREATE INDEX IF NOT EXISTS favicon_bitmaps_icon_id ON "
                     "favicon_bitmaps(icon_id)");
}

bool ThumbnailDatabase::IsFaviconDBStructureIncorrect() {
  return !db_.IsSQLValid("SELECT id, url, icon_type, sizes FROM favicons");
}

void ThumbnailDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void ThumbnailDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

void ThumbnailDatabase::RollbackTransaction() {
  db_.RollbackTransaction();
}

void ThumbnailDatabase::Vacuum() {
  DCHECK(db_.transaction_nesting() == 0) <<
      "Can not have a transaction when vacuuming.";
  ignore_result(db_.Execute("VACUUM"));
}

bool ThumbnailDatabase::SetPageThumbnail(
    const GURL& url,
    URLID id,
    const gfx::Image* thumbnail,
    const ThumbnailScore& score,
    base::Time time) {
  if (use_top_sites_) {
    LOG(WARNING) << "Use TopSites instead.";
    return false;  // Not possible after migration to TopSites.
  }

  if (!thumbnail)
    return DeleteThumbnail(id);

  bool add_thumbnail = true;
  ThumbnailScore current_score;
  if (ThumbnailScoreForId(id, &current_score)) {
    add_thumbnail = ShouldReplaceThumbnailWith(current_score, score);
  }

  if (!add_thumbnail)
    return true;

  std::vector<unsigned char> jpeg_data;
  bool encoded = gfx::JPEG1xEncodedDataFromImage(
      *thumbnail, kImageQuality, &jpeg_data);
  if (encoded) {
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
        "INSERT OR REPLACE INTO thumbnails "
        "(url_id, boring_score, good_clipping, at_top, last_updated, data) "
        "VALUES (?,?,?,?,?,?)"));
    statement.BindInt64(0, id);
    statement.BindDouble(1, score.boring_score);
    statement.BindBool(2, score.good_clipping);
    statement.BindBool(3, score.at_top);
    statement.BindInt64(4, score.time_at_snapshot.ToInternalValue());
    statement.BindBlob(5, &jpeg_data[0],
                       static_cast<int>(jpeg_data.size()));

    if (!statement.Run())
      return false;
  }

  // Publish the thumbnail to any indexers listening to us.
  // The tests may send an invalid url. Hence avoid publishing those.
  if (url.is_valid() && history_publisher_ != NULL)
    history_publisher_->PublishPageThumbnail(jpeg_data, url, time);

  return true;
}

bool ThumbnailDatabase::GetPageThumbnail(URLID id,
                                         std::vector<unsigned char>* data) {
  if (use_top_sites_) {
    LOG(WARNING) << "Use TopSites instead.";
    return false;  // Not possible after migration to TopSites.
  }

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT data FROM thumbnails WHERE url_id=?"));
  statement.BindInt64(0, id);

  if (!statement.Step())
    return false;  // don't have a thumbnail for this ID

  statement.ColumnBlobAsVector(0, data);
  return true;
}

bool ThumbnailDatabase::DeleteThumbnail(URLID id) {
  if (use_top_sites_) {
    return true;  // Not possible after migration to TopSites.
  }

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM thumbnails WHERE url_id = ?"));
  statement.BindInt64(0, id);

  return statement.Run();
}

bool ThumbnailDatabase::ThumbnailScoreForId(URLID id,
                                            ThumbnailScore* score) {
  DCHECK(score);
  if (use_top_sites_) {
    LOG(WARNING) << "Use TopSites instead.";
    return false;  // Not possible after migration to TopSites.
  }

  // Fetch the current thumbnail's information to make sure we
  // aren't replacing a good thumbnail with one that's worse.
  sql::Statement select_statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT boring_score, good_clipping, at_top, last_updated "
      "FROM thumbnails WHERE url_id=?"));
  select_statement.BindInt64(0, id);

  if (!select_statement.Step())
    return false;

  double current_boring_score = select_statement.ColumnDouble(0);
  bool current_clipping = select_statement.ColumnBool(1);
  bool current_at_top = select_statement.ColumnBool(2);
  base::Time last_updated =
      base::Time::FromInternalValue(select_statement.ColumnInt64(3));
  *score = ThumbnailScore(current_boring_score, current_clipping,
                          current_at_top, last_updated);
  return true;
}

bool ThumbnailDatabase::GetFaviconBitmapIDSizes(
    FaviconID icon_id,
    std::vector<FaviconBitmapIDSize>* bitmap_id_sizes) {
  DCHECK(icon_id);
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, width, height FROM favicon_bitmaps WHERE icon_id=?"));
  statement.BindInt64(0, icon_id);

  bool result = false;
  while (statement.Step()) {
    result = true;
    if (!bitmap_id_sizes)
      return result;

    FaviconBitmapIDSize bitmap_id_size;
    bitmap_id_size.bitmap_id = statement.ColumnInt64(0);
    bitmap_id_size.pixel_size = gfx::Size(statement.ColumnInt(1),
                                          statement.ColumnInt(2));
    bitmap_id_sizes->push_back(bitmap_id_size);
  }
  return result;
}

bool ThumbnailDatabase::GetFaviconBitmaps(
    FaviconID icon_id,
    std::vector<FaviconBitmap>* favicon_bitmaps) {
  DCHECK(icon_id);
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, last_updated, image_data, width, height FROM favicon_bitmaps "
      "WHERE icon_id=?"));
  statement.BindInt64(0, icon_id);

  bool result = false;
  while (statement.Step()) {
    result = true;
    if (!favicon_bitmaps)
      return result;

    FaviconBitmap favicon_bitmap;
    favicon_bitmap.bitmap_id = statement.ColumnInt64(0);
    favicon_bitmap.icon_id = icon_id;
    favicon_bitmap.last_updated =
        base::Time::FromInternalValue(statement.ColumnInt64(1));
    if (statement.ColumnByteLength(2) > 0) {
      scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
      statement.ColumnBlobAsVector(2, &data->data());
      favicon_bitmap.bitmap_data = data;
    }
    favicon_bitmap.pixel_size = gfx::Size(statement.ColumnInt(3),
                                          statement.ColumnInt(4));
    favicon_bitmaps->push_back(favicon_bitmap);
  }
  return result;
}

bool ThumbnailDatabase::GetFaviconBitmap(
    FaviconBitmapID bitmap_id,
    base::Time* last_updated,
    scoped_refptr<base::RefCountedMemory>* png_icon_data,
    gfx::Size* pixel_size) {
  DCHECK(bitmap_id);
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT last_updated, image_data, width, height FROM favicon_bitmaps "
      "WHERE id=?"));
  statement.BindInt64(0, bitmap_id);

  if (!statement.Step())
    return false;

  if (last_updated)
    *last_updated = base::Time::FromInternalValue(statement.ColumnInt64(0));

  if (png_icon_data && statement.ColumnByteLength(1) > 0) {
    scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
    statement.ColumnBlobAsVector(1, &data->data());
    *png_icon_data = data;
  }

  if (pixel_size) {
    *pixel_size = gfx::Size(statement.ColumnInt(2),
                            statement.ColumnInt(3));
  }
  return true;
}

FaviconBitmapID ThumbnailDatabase::AddFaviconBitmap(
    FaviconID icon_id,
    const scoped_refptr<base::RefCountedMemory>& icon_data,
    base::Time time,
    const gfx::Size& pixel_size) {
  DCHECK(icon_id);
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO favicon_bitmaps (icon_id, image_data, last_updated, width, "
      "height) VALUES (?, ?, ?, ?, ?)"));
  statement.BindInt64(0, icon_id);
  if (icon_data.get() && icon_data->size()) {
    statement.BindBlob(1, icon_data->front(),
                       static_cast<int>(icon_data->size()));
  } else {
    statement.BindNull(1);
  }
  statement.BindInt64(2, time.ToInternalValue());
  statement.BindInt(3, pixel_size.width());
  statement.BindInt(4, pixel_size.height());

  if (!statement.Run())
    return 0;
  return db_.GetLastInsertRowId();
}

bool ThumbnailDatabase::SetFaviconBitmap(
    FaviconBitmapID bitmap_id,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    base::Time time) {
  DCHECK(bitmap_id);
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE favicon_bitmaps SET image_data=?, last_updated=? WHERE id=?"));
  if (bitmap_data.get() && bitmap_data->size()) {
    statement.BindBlob(0, bitmap_data->front(),
                       static_cast<int>(bitmap_data->size()));
  } else {
    statement.BindNull(0);
  }
  statement.BindInt64(1, time.ToInternalValue());
  statement.BindInt64(2, bitmap_id);

  return statement.Run();
}

bool ThumbnailDatabase::DeleteFaviconBitmapsForFavicon(FaviconID icon_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM favicon_bitmaps WHERE icon_id=?"));
  statement.BindInt64(0, icon_id);
  return statement.Run();
}

bool ThumbnailDatabase::DeleteFaviconBitmap(FaviconBitmapID bitmap_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM favicon_bitmaps WHERE id=?"));
  statement.BindInt64(0, bitmap_id);
  return statement.Run();
}

bool ThumbnailDatabase::SetFaviconSizes(FaviconID icon_id,
                                        const FaviconSizes& favicon_sizes) {
  std::string favicon_sizes_as_string;
  FaviconSizesToDatabaseString(favicon_sizes, &favicon_sizes_as_string);

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE favicons SET sizes=? WHERE id=?"));
  statement.BindString(0, favicon_sizes_as_string);
  statement.BindInt64(1, icon_id);

  return statement.Run();
}

bool ThumbnailDatabase::SetFaviconOutOfDate(FaviconID icon_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE favicon_bitmaps SET last_updated=? WHERE icon_id=?"));
  statement.BindInt64(0, 0);
  statement.BindInt64(1, icon_id);

  return statement.Run();
}

FaviconID ThumbnailDatabase::GetFaviconIDForFaviconURL(const GURL& icon_url,
                                                       int required_icon_type,
                                                       IconType* icon_type) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, icon_type FROM favicons WHERE url=? AND (icon_type & ? > 0) "
      "ORDER BY icon_type DESC"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));
  statement.BindInt(1, required_icon_type);

  if (!statement.Step())
    return 0;  // not cached

  if (icon_type)
    *icon_type = static_cast<IconType>(statement.ColumnInt(1));
  return statement.ColumnInt64(0);
}

bool ThumbnailDatabase::GetFaviconHeader(
    FaviconID icon_id,
    GURL* icon_url,
    IconType* icon_type,
    FaviconSizes* favicon_sizes) {
  DCHECK(icon_id);

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT url, icon_type, sizes FROM favicons WHERE id=?"));
  statement.BindInt64(0, icon_id);

  if (!statement.Step())
    return false;  // No entry for the id.

  if (icon_url)
    *icon_url = GURL(statement.ColumnString(0));
  if (icon_type)
    *icon_type = static_cast<history::IconType>(statement.ColumnInt(1));
  if (favicon_sizes)
    DatabaseStringToFaviconSizes(statement.ColumnString(2), favicon_sizes);

  return true;
}

FaviconID ThumbnailDatabase::AddFavicon(const GURL& icon_url,
                                        IconType icon_type,
                                        const FaviconSizes& favicon_sizes) {
  std::string favicon_sizes_as_string;
  FaviconSizesToDatabaseString(favicon_sizes, &favicon_sizes_as_string);

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO favicons (url, icon_type, sizes) VALUES (?, ?, ?)"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));
  statement.BindInt(1, icon_type);
  statement.BindString(2, favicon_sizes_as_string);

  if (!statement.Run())
    return 0;
  return db_.GetLastInsertRowId();
}

FaviconID ThumbnailDatabase::AddFavicon(
    const GURL& icon_url,
    IconType icon_type,
    const FaviconSizes& favicon_sizes,
    const scoped_refptr<base::RefCountedMemory>& icon_data,
    base::Time time,
    const gfx::Size& pixel_size) {
  FaviconID icon_id = AddFavicon(icon_url, icon_type, favicon_sizes);
  if (!icon_id || !AddFaviconBitmap(icon_id, icon_data, time, pixel_size))
    return 0;

  return icon_id;
}

bool ThumbnailDatabase::DeleteFavicon(FaviconID id) {
  sql::Statement statement;
  statement.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM favicons WHERE id = ?"));
  statement.BindInt64(0, id);
  if (!statement.Run())
    return false;

  statement.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM favicon_bitmaps WHERE icon_id = ?"));
  statement.BindInt64(0, id);
  return statement.Run();
}

bool ThumbnailDatabase::GetIconMappingsForPageURL(
    const GURL& page_url,
    int required_icon_types,
    std::vector<IconMapping>* filtered_mapping_data) {
  std::vector<IconMapping> mapping_data;
  if (!GetIconMappingsForPageURL(page_url, &mapping_data))
    return false;

  bool result = false;
  for (std::vector<IconMapping>::iterator m = mapping_data.begin();
       m != mapping_data.end(); ++m) {
    if (m->icon_type & required_icon_types) {
      result = true;
      if (!filtered_mapping_data)
        return result;

      // Restrict icon type of subsequent matches to |m->icon_type|.
      // |m->icon_type| is the largest IconType in |mapping_data| because
      // |mapping_data| is sorted in descending order of IconType.
      required_icon_types = m->icon_type;

      filtered_mapping_data->push_back(*m);
    }
  }
  return result;
}

bool ThumbnailDatabase::GetIconMappingsForPageURL(
    const GURL& page_url,
    std::vector<IconMapping>* mapping_data) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT icon_mapping.id, icon_mapping.icon_id, favicons.icon_type, "
      "favicons.url "
      "FROM icon_mapping "
      "INNER JOIN favicons "
      "ON icon_mapping.icon_id = favicons.id "
      "WHERE icon_mapping.page_url=? "
      "ORDER BY favicons.icon_type DESC"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(page_url));

  bool result = false;
  while (statement.Step()) {
    result = true;
    if (!mapping_data)
      return result;

    IconMapping icon_mapping;
    FillIconMapping(statement, page_url, &icon_mapping);
    mapping_data->push_back(icon_mapping);
  }
  return result;
}

IconMappingID ThumbnailDatabase::AddIconMapping(const GURL& page_url,
                                                FaviconID icon_id) {
  return AddIconMapping(page_url, icon_id, false);
}

bool ThumbnailDatabase::UpdateIconMapping(IconMappingID mapping_id,
                                          FaviconID icon_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE icon_mapping SET icon_id=? WHERE id=?"));
  statement.BindInt64(0, icon_id);
  statement.BindInt64(1, mapping_id);

  return statement.Run();
}

bool ThumbnailDatabase::DeleteIconMappings(const GURL& page_url) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM icon_mapping WHERE page_url = ?"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(page_url));

  return statement.Run();
}

bool ThumbnailDatabase::DeleteIconMapping(IconMappingID mapping_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM icon_mapping WHERE id=?"));
  statement.BindInt64(0, mapping_id);

  return statement.Run();
}

bool ThumbnailDatabase::HasMappingFor(FaviconID id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id FROM icon_mapping "
      "WHERE icon_id=?"));
  statement.BindInt64(0, id);

  return statement.Step();
}

bool ThumbnailDatabase::CloneIconMappings(const GURL& old_page_url,
                                          const GURL& new_page_url) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT icon_id FROM icon_mapping "
      "WHERE page_url=?"));
  if (!statement.is_valid())
    return false;

  // Do nothing if there are existing bindings
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(new_page_url));
  if (statement.Step())
    return true;

  statement.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO icon_mapping (page_url, icon_id) "
        "SELECT ?, icon_id FROM icon_mapping "
        "WHERE page_url = ?"));

  statement.BindString(0, URLDatabase::GURLToDatabaseURL(new_page_url));
  statement.BindString(1, URLDatabase::GURLToDatabaseURL(old_page_url));
  return statement.Run();
}

bool ThumbnailDatabase::InitIconMappingEnumerator(
    IconType type,
    IconMappingEnumerator* enumerator) {
  DCHECK(!enumerator->statement_.is_valid());
  enumerator->statement_.Assign(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT icon_mapping.id, icon_mapping.icon_id, favicons.icon_type, "
             "favicons.url, icon_mapping.page_url "
         "FROM icon_mapping JOIN favicons ON ("
              "icon_mapping.icon_id = favicons.id) "
         "WHERE favicons.icon_type = ?"));
  enumerator->statement_.BindInt(0, type);
  return enumerator->statement_.is_valid();
}

bool ThumbnailDatabase::MigrateIconMappingData(URLDatabase* url_db) {
  URLDatabase::IconMappingEnumerator e;
  if (!url_db->InitIconMappingEnumeratorForEverything(&e))
    return false;

  IconMapping info;
  while (e.GetNextIconMapping(&info)) {
    // TODO: Using bulk insert to improve the performance.
    if (!AddIconMapping(info.page_url, info.icon_id))
      return false;
  }
  return true;
}

bool ThumbnailDatabase::InitTemporaryTables() {
  return InitIconMappingTable(&db_, true) &&
         InitFaviconsTable(&db_, true) &&
         InitFaviconBitmapsTable(&db_, true);
}

bool ThumbnailDatabase::CommitTemporaryTables() {
  const char* main_tables[] = { "icon_mapping",
                                "favicons",
                                "favicon_bitmaps" };
  const char* temporary_tables[] = { "temp_icon_mapping",
                                     "temp_favicons",
                                     "temp_favicon_bitmaps" };
  DCHECK_EQ(arraysize(main_tables), arraysize(temporary_tables));

  for (size_t i = 0; i < arraysize(main_tables); ++i) {
    // Delete the main table.
    std::string sql;
    sql.append("DROP TABLE ");
    sql.append(main_tables[i]);
    if (!db_.Execute(sql.c_str()))
      return false;

    // Rename the temporary table.
    sql.clear();
    sql.append("ALTER TABLE ");
    sql.append(temporary_tables[i]);
    sql.append(" RENAME TO ");
    sql.append(main_tables[i]);
    if (!db_.Execute(sql.c_str()))
      return false;
  }

  // The renamed tables needs indices (the temporary tables don't have any).
  return InitIconMappingIndex() &&
         InitFaviconsIndex() &&
         InitFaviconBitmapsIndex();
}

IconMappingID ThumbnailDatabase::AddToTemporaryIconMappingTable(
    const GURL& page_url, const FaviconID icon_id) {
  return AddIconMapping(page_url, icon_id, true);
}

FaviconID ThumbnailDatabase::CopyFaviconAndFaviconBitmapsToTemporaryTables(
    FaviconID source) {
  sql::Statement statement;
  statement.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO temp_favicons (url, icon_type, sizes) "
      "SELECT url, icon_type, sizes FROM favicons WHERE id = ?"));
  statement.BindInt64(0, source);

  if (!statement.Run())
    return 0;

  FaviconID new_favicon_id = db_.GetLastInsertRowId();

  statement.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO temp_favicon_bitmaps (icon_id, last_updated, image_data, "
      "width, height) "
      "SELECT ?, last_updated, image_data, width, height "
      "FROM favicon_bitmaps WHERE icon_id = ?"));
  statement.BindInt64(0, new_favicon_id);
  statement.BindInt64(1, source);
  if (!statement.Run())
    return 0;

  return new_favicon_id;
}

bool ThumbnailDatabase::NeedsMigrationToTopSites() {
  return !use_top_sites_;
}

bool ThumbnailDatabase::RenameAndDropThumbnails(const FilePath& old_db_file,
                                                const FilePath& new_db_file) {
  // Init favicons tables - same schema as the thumbnails.
  sql::Connection favicons;
  if (OpenDatabase(&favicons, new_db_file) != sql::INIT_OK)
    return false;

  if (!InitFaviconBitmapsTable(&favicons, false) ||
      !InitFaviconsTable(&favicons, false) ||
      !InitIconMappingTable(&favicons, false)) {
    favicons.Close();
    return false;
  }
  favicons.Close();

  // Can't attach within a transaction.
  if (transaction_nesting())
    CommitTransaction();

  // Attach new DB.
  {
    // This block is needed because otherwise the attach statement is
    // never cleared from cache and we can't close the DB :P
    sql::Statement attach(db_.GetUniqueStatement("ATTACH ? AS new_favicons"));
    if (!attach.is_valid()) {
      // Keep the transaction open, even though we failed.
      BeginTransaction();
      return false;
    }

#if defined(OS_POSIX)
    attach.BindString(0, new_db_file.value());
#else
    attach.BindString(0, WideToUTF8(new_db_file.value()));
#endif

    if (!attach.Run()) {
      BeginTransaction();
      return false;
    }
  }

  // Move favicons and favicon_bitmaps to new DB.
  bool successfully_moved_data =
     db_.Execute("INSERT OR REPLACE INTO new_favicons.favicon_bitmaps "
                 "SELECT * FROM favicon_bitmaps") &&
     db_.Execute("INSERT OR REPLACE INTO new_favicons.favicons "
                 "SELECT * FROM favicons");
  if (!successfully_moved_data) {
    DLOG(FATAL) << "Unable to copy favicons and favicon_bitmaps.";
    BeginTransaction();
    return false;
  }

  if (!db_.Execute("DETACH new_favicons")) {
    DLOG(FATAL) << "Unable to detach database.";
    BeginTransaction();
    return false;
  }

  db_.Close();

  // Reset the DB to point to new file.
  if (OpenDatabase(&db_, new_db_file) != sql::INIT_OK)
    return false;

  file_util::Delete(old_db_file, false);

  meta_table_.Reset();
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return false;

  if (!InitFaviconBitmapsIndex() || !InitFaviconsIndex())
    return false;

  // Reopen the transaction.
  BeginTransaction();
  use_top_sites_ = true;
  return true;
}

bool ThumbnailDatabase::InitIconMappingTable(sql::Connection* db,
                                             bool is_temporary) {
  const char* name = is_temporary ? "temp_icon_mapping" : "icon_mapping";
  if (!db->DoesTableExist(name)) {
    std::string sql;
    sql.append("CREATE TABLE ");
    sql.append(name);
    sql.append("("
               "id INTEGER PRIMARY KEY,"
               "page_url LONGVARCHAR NOT NULL,"
               "icon_id INTEGER)");
    if (!db->Execute(sql.c_str()))
      return false;
  }
  return true;
}

bool ThumbnailDatabase::InitIconMappingIndex() {
  // Add an index on the url column.
  return
      db_.Execute("CREATE INDEX IF NOT EXISTS icon_mapping_page_url_idx"
                  " ON icon_mapping(page_url)") &&
      db_.Execute("CREATE INDEX IF NOT EXISTS icon_mapping_icon_id_idx"
                  " ON icon_mapping(icon_id)");
}

IconMappingID ThumbnailDatabase::AddIconMapping(const GURL& page_url,
                                                FaviconID icon_id,
                                                bool is_temporary) {
  const char* name = is_temporary ? "temp_icon_mapping" : "icon_mapping";
  const char* statement_name =
      is_temporary ? "add_temp_icon_mapping" : "add_icon_mapping";

  std::string sql;
  sql.append("INSERT INTO ");
  sql.append(name);
  sql.append("(page_url, icon_id) VALUES (?, ?)");

  sql::Statement statement(
      db_.GetCachedStatement(sql::StatementID(statement_name), sql.c_str()));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(page_url));
  statement.BindInt64(1, icon_id);

  if (!statement.Run())
    return 0;

  return db_.GetLastInsertRowId();
}

bool ThumbnailDatabase::IsLatestVersion() {
  return meta_table_.GetVersionNumber() == kCurrentVersionNumber;
}

bool ThumbnailDatabase::UpgradeToVersion4() {
  // Set the default icon type as favicon, so the current data are set
  // correctly.
  if (!db_.Execute("ALTER TABLE favicons ADD icon_type INTEGER DEFAULT 1")) {
    return false;
  }
  meta_table_.SetVersionNumber(4);
  meta_table_.SetCompatibleVersionNumber(std::min(4, kCompatibleVersionNumber));
  return true;
}

bool ThumbnailDatabase::UpgradeToVersion5() {
  if (!db_.Execute("ALTER TABLE favicons ADD sizes LONGVARCHAR")) {
    return false;
  }
  meta_table_.SetVersionNumber(5);
  meta_table_.SetCompatibleVersionNumber(std::min(5, kCompatibleVersionNumber));
  return true;
}

bool ThumbnailDatabase::UpgradeToVersion6() {
  bool success =
      db_.Execute("INSERT INTO favicon_bitmaps (icon_id, last_updated, "
                  "image_data, width, height)"
                  "SELECT id, last_updated, image_data, 0, 0 FROM favicons") &&
      db_.Execute("CREATE TABLE temp_favicons ("
                  "id INTEGER PRIMARY KEY,"
                  "url LONGVARCHAR NOT NULL,"
                  "icon_type INTEGER DEFAULT 1,"
                  // Set the default icon_type as FAVICON to be consistent with
                  // table upgrade in UpgradeToVersion4().
                  "sizes LONGVARCHAR)") &&
      db_.Execute("INSERT INTO temp_favicons (id, url, icon_type) "
                  "SELECT id, url, icon_type FROM favicons") &&
      db_.Execute("DROP TABLE favicons") &&
      db_.Execute("ALTER TABLE temp_favicons RENAME TO favicons");
  if (!success)
    return false;

  meta_table_.SetVersionNumber(6);
  meta_table_.SetCompatibleVersionNumber(std::min(6, kCompatibleVersionNumber));
  return true;
}

// static
void ThumbnailDatabase::FaviconSizesToDatabaseString(
    const FaviconSizes& favicon_sizes,
    std::string* favicon_sizes_string) {
  std::vector<std::string> parts;
  for (FaviconSizes::const_iterator it = favicon_sizes.begin();
       it != favicon_sizes.end(); ++it) {
    parts.push_back(base::IntToString(it->width()));
    parts.push_back(base::IntToString(it->height()));
  }
  *favicon_sizes_string = JoinString(parts, ' ');
}

// static
void ThumbnailDatabase::DatabaseStringToFaviconSizes(
    const std::string& favicon_sizes_string,
    FaviconSizes* favicon_sizes) {
  bool parsing_errors = false;

  StringTokenizer t(favicon_sizes_string, " ");
  while (t.GetNext() && !parsing_errors) {
    int width, height = 0;
    parsing_errors |= !base::StringToInt(t.token(), &width);
    if (!t.GetNext()) {
      parsing_errors = true;
      break;
    }
    parsing_errors |= !base::StringToInt(t.token(), &height);
    favicon_sizes->push_back(gfx::Size(width, height));
  }

  if (parsing_errors)
    favicon_sizes->clear();
}

}  // namespace history
