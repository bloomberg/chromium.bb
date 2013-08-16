// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/thumbnail_database.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_publisher.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/dump_without_crashing.h"
#include "chrome/common/thumbnail_score.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"
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
//
// favicon_bitmaps    This table contains the PNG encoded bitmap data of the
//                    favicons. There is a separate row for every size in a
//                    multi resolution bitmap. The bitmap data is associated
//                    to the favicon via the |icon_id| field which matches
//                    the |id| field in the appropriate row in the |favicons|
//                    table.
//
//  id                Unique ID.
//  icon_id           The ID of the favicon that the bitmap is associated to.
//  last_updated      The time at which this favicon was inserted into the
//                    table. This is used to determine if it needs to be
//                    redownloaded from the web.
//  image_data        PNG encoded data of the favicon.
//  width             Pixel width of |image_data|.
//  height            Pixel height of |image_data|.

namespace {

void FillIconMapping(const sql::Statement& statement,
                     const GURL& page_url,
                     history::IconMapping* icon_mapping) {
  icon_mapping->mapping_id = statement.ColumnInt64(0);
  icon_mapping->icon_id = statement.ColumnInt64(1);
  icon_mapping->icon_type =
      static_cast<chrome::IconType>(statement.ColumnInt(2));
  icon_mapping->icon_url = GURL(statement.ColumnString(3));
  icon_mapping->page_url = page_url;
}

enum InvalidStructureType {
  // NOTE(shess): Intentionally skip bucket 0 to account for
  // conversion from a boolean histogram.
  STRUCTURE_EVENT_FAVICON = 1,
  STRUCTURE_EVENT_VERSION4,
  STRUCTURE_EVENT_VERSION5,

  // Always keep this at the end.
  STRUCTURE_EVENT_MAX,
};

void RecordInvalidStructure(InvalidStructureType invalid_type) {
  UMA_HISTOGRAM_ENUMERATION("History.InvalidFaviconsDBStructure",
                            invalid_type, STRUCTURE_EVENT_MAX);
}

// Attempt to pass 2000 bytes of |debug_info| into a crash dump.
void DumpWithoutCrashing2000(const std::string& debug_info) {
  char debug_buf[2000];
  base::strlcpy(debug_buf, debug_info.c_str(), arraysize(debug_buf));
  base::debug::Alias(&debug_buf);

  logging::DumpWithoutCrashing();
}

void ReportCorrupt(sql::Connection* db, size_t startup_kb) {
  // Buffer for accumulating debugging info about the error.  Place
  // more-relevant information earlier, in case things overflow the
  // fixed-size buffer.
  std::string debug_info;

  base::StringAppendF(&debug_info, "SQLITE_CORRUPT, integrity_check:\n");

  // Check files up to 8M to keep things from blocking too long.
  const size_t kMaxIntegrityCheckSize = 8192;
  if (startup_kb > kMaxIntegrityCheckSize) {
    base::StringAppendF(&debug_info, "too big %" PRIuS "\n", startup_kb);
  } else {
    std::vector<std::string> messages;

    const base::TimeTicks before = base::TimeTicks::Now();
    db->IntegrityCheck(&messages);
    base::StringAppendF(&debug_info, "# %" PRIx64 " ms, %" PRIuS " records\n",
                        (base::TimeTicks::Now() - before).InMilliseconds(),
                        messages.size());

    // SQLite returns up to 100 messages by default, trim deeper to
    // keep close to the 2000-character size limit for dumping.
    //
    // TODO(shess): If the first 20 tend to be actionable, test if
    // passing the count to integrity_check makes it exit earlier.  In
    // that case it may be possible to greatly ease the size
    // restriction.
    const size_t kMaxMessages = 20;
    for (size_t i = 0; i < kMaxMessages && i < messages.size(); ++i) {
      base::StringAppendF(&debug_info, "%s\n", messages[i].c_str());
    }
  }

  DumpWithoutCrashing2000(debug_info);
}

void ReportError(sql::Connection* db, int error) {
  // Buffer for accumulating debugging info about the error.  Place
  // more-relevant information earlier, in case things overflow the
  // fixed-size buffer.
  std::string debug_info;

  // The error message from the failed operation.
  base::StringAppendF(&debug_info, "db error: %d/%s\n",
                      db->GetErrorCode(), db->GetErrorMessage());

  // System errno information.
  base::StringAppendF(&debug_info, "errno: %d\n", db->GetLastErrno());

  // SQLITE_ERROR reports seem to be attempts to upgrade invalid
  // schema, try to log that info.
  if (error == SQLITE_ERROR) {
    const char* kVersionSql = "SELECT value FROM meta WHERE key = 'version'";
    if (db->IsSQLValid(kVersionSql)) {
      sql::Statement statement(db->GetUniqueStatement(kVersionSql));
      if (statement.Step()) {
        debug_info += "version: ";
        debug_info += statement.ColumnString(0);
        debug_info += '\n';
      } else if (statement.Succeeded()) {
        debug_info += "version: none\n";
      } else {
        debug_info += "version: error\n";
      }
    } else {
      debug_info += "version: invalid\n";
    }

    debug_info += "schema:\n";

    // sqlite_master has columns:
    //   type - "index" or "table".
    //   name - name of created element.
    //   tbl_name - name of element, or target table in case of index.
    //   rootpage - root page of the element in database file.
    //   sql - SQL to create the element.
    // In general, the |sql| column is sufficient to derive the other
    // columns.  |rootpage| is not interesting for debugging, without
    // the contents of the database.  The COALESCE is because certain
    // automatic elements will have a |name| but no |sql|,
    const char* kSchemaSql = "SELECT COALESCE(sql, name) FROM sqlite_master";
    sql::Statement statement(db->GetUniqueStatement(kSchemaSql));
    while (statement.Step()) {
      debug_info += statement.ColumnString(0);
      debug_info += '\n';
    }
    if (!statement.Succeeded())
      debug_info += "error\n";
  }

  // TODO(shess): Think of other things to log.  Not logging the
  // statement text because the backtrace should suffice in most
  // cases.  The database schema is a possibility, but the
  // likelihood of recursive error callbacks makes that risky (same
  // reasoning applies to other data fetched from the database).

  DumpWithoutCrashing2000(debug_info);
}

// TODO(shess): If this proves out, perhaps lift the code out to
// chrome/browser/diagnostics/sqlite_diagnostics.{h,cc}.
void DatabaseErrorCallback(sql::Connection* db,
                           size_t startup_kb,
                           int error,
                           sql::Statement* stmt) {
  // TODO(shess): Assert that this is running on a safe thread.
  // AFAICT, should be the history thread, but at this level I can't
  // see how to reach that.

  // Infrequently report information about the error up to the crash
  // server.
  static const uint64 kReportsPerMillion = 50000;

  // TODO(shess): For now, don't report on beta or stable so as not to
  // overwhelm the crash server.  Once the big fish are fried,
  // consider reporting at a reduced rate on the bigger channels.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();

  // Since some/most errors will not resolve themselves, only report
  // once per Chrome run.
  static bool reported = false;

  if (channel != chrome::VersionInfo::CHANNEL_STABLE &&
      channel != chrome::VersionInfo::CHANNEL_BETA &&
      !reported) {
    uint64 rand = base::RandGenerator(1000000);
    if (error == SQLITE_CORRUPT) {
      // Once the database is known to be corrupt, it will generate a
      // stream of errors until someone fixes it, so give one chance.
      // Set first in case of errors in generating the report.
      reported = true;

      // Corrupt cases currently dominate, report them very infrequently.
      static const uint64 kCorruptReportsPerMillion = 10000;
      if (rand < kCorruptReportsPerMillion)
        ReportCorrupt(db, startup_kb);
    } else if (error == SQLITE_READONLY) {
      // SQLITE_READONLY appears similar to SQLITE_CORRUPT - once it
      // is seen, it is almost guaranteed to be seen again.
      reported = true;

      if (rand < kReportsPerMillion)
        ReportError(db, error);
    } else {
      // Only set the flag when making a report.  This should allow
      // later (potentially different) errors in a stream of errors to
      // be reported.
      //
      // TODO(shess): Would it be worthwile to audit for which cases
      // want once-only handling?  Sqlite.Error.Thumbnail shows
      // CORRUPT and READONLY as almost 95% of all reports on these
      // channels, so probably easier to just harvest from the field.
      if (rand < kReportsPerMillion) {
        reported = true;
        ReportError(db, error);
      }
    }
  }

  // The default handling is to assert on debug and to ignore on release.
  DLOG(FATAL) << db->GetErrorMessage();
}

}  // namespace

namespace history {

// Version number of the database.
static const int kCurrentVersionNumber = 7;
static const int kCompatibleVersionNumber = 7;

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
    const base::FilePath& db_name,
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
      !InitFaviconBitmapsTable(&db_) ||
      !InitFaviconBitmapsIndex() ||
      !InitFaviconsTable(&db_) ||
      !InitFaviconsIndex() ||
      !InitIconMappingTable(&db_) ||
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

  if (!db_.DoesColumnExist("favicons", "icon_type")) {
    LOG(ERROR) << "Raze because of missing favicon.icon_type";
    RecordInvalidStructure(STRUCTURE_EVENT_VERSION4);

    db_.RazeAndClose();
    return sql::INIT_FAILURE;
  }

  if (cur_version == 4) {
    ++cur_version;
    if (!UpgradeToVersion5())
      return CantUpgradeToVersion(cur_version);
  }

  if (cur_version < 7 && !db_.DoesColumnExist("favicons", "sizes")) {
    LOG(ERROR) << "Raze because of missing favicon.sizes";
    RecordInvalidStructure(STRUCTURE_EVENT_VERSION5);

    db_.RazeAndClose();
    return sql::INIT_FAILURE;
  }

  if (cur_version == 5) {
    ++cur_version;
    if (!UpgradeToVersion6())
      return CantUpgradeToVersion(cur_version);
  }

  if (cur_version == 6) {
    ++cur_version;
    if (!UpgradeToVersion7())
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
    LOG(ERROR) << "Raze because of invalid favicon db structure.";
    RecordInvalidStructure(STRUCTURE_EVENT_FAVICON);

    db_.RazeAndClose();
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

sql::InitStatus ThumbnailDatabase::OpenDatabase(sql::Connection* db,
                                                const base::FilePath& db_name) {
  size_t startup_kb = 0;
  int64 size_64;
  if (file_util::GetFileSize(db_name, &size_64))
    startup_kb = static_cast<size_t>(size_64 / 1024);

  db->set_histogram_tag("Thumbnail");
  db->set_error_callback(base::Bind(&DatabaseErrorCallback, db, startup_kb));

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

void ThumbnailDatabase::ComputeDatabaseMetrics() {
  sql::Statement favicon_count(
      db_.GetCachedStatement(SQL_FROM_HERE, "SELECT COUNT(*) FROM favicons"));
  UMA_HISTOGRAM_COUNTS_10000(
      "History.NumFaviconsInDB",
      favicon_count.Step() ? favicon_count.ColumnInt(0) : 0);
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

bool ThumbnailDatabase::InitFaviconsTable(sql::Connection* db) {
  const char kSql[] =
      "CREATE TABLE IF NOT EXISTS favicons"
      "("
      "id INTEGER PRIMARY KEY,"
      "url LONGVARCHAR NOT NULL,"
      // Set the default icon_type as FAVICON to be consistent with
      // table upgrade in UpgradeToVersion4().
      "icon_type INTEGER DEFAULT 1"
      ")";
  return db->Execute(kSql);
}

bool ThumbnailDatabase::InitFaviconsIndex() {
  // Add an index on the url column.
  return
      db_.Execute("CREATE INDEX IF NOT EXISTS favicons_url ON favicons(url)");
}

bool ThumbnailDatabase::InitFaviconBitmapsTable(sql::Connection* db) {
  const char kSql[] =
      "CREATE TABLE IF NOT EXISTS favicon_bitmaps"
      "("
      "id INTEGER PRIMARY KEY,"
      "icon_id INTEGER NOT NULL,"
      "last_updated INTEGER DEFAULT 0,"
      "image_data BLOB,"
      "width INTEGER DEFAULT 0,"
      "height INTEGER DEFAULT 0"
      ")";
  return db->Execute(kSql);
}

bool ThumbnailDatabase::InitFaviconBitmapsIndex() {
  // Add an index on the icon_id column.
  return db_.Execute("CREATE INDEX IF NOT EXISTS favicon_bitmaps_icon_id ON "
                     "favicon_bitmaps(icon_id)");
}

bool ThumbnailDatabase::IsFaviconDBStructureIncorrect() {
  return !db_.IsSQLValid("SELECT id, url, icon_type FROM favicons");
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

void ThumbnailDatabase::TrimMemory(bool aggressively) {
  db_.TrimMemory(aggressively);
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
    chrome::FaviconID icon_id,
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
    chrome::FaviconID icon_id,
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
    chrome::FaviconID icon_id,
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

bool ThumbnailDatabase::SetFaviconBitmapLastUpdateTime(
    FaviconBitmapID bitmap_id,
    base::Time time) {
  DCHECK(bitmap_id);
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE favicon_bitmaps SET last_updated=? WHERE id=?"));
  statement.BindInt64(0, time.ToInternalValue());
  statement.BindInt64(1, bitmap_id);
  return statement.Run();
}

bool ThumbnailDatabase::DeleteFaviconBitmapsForFavicon(
    chrome::FaviconID icon_id) {
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

bool ThumbnailDatabase::SetFaviconOutOfDate(chrome::FaviconID icon_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE favicon_bitmaps SET last_updated=? WHERE icon_id=?"));
  statement.BindInt64(0, 0);
  statement.BindInt64(1, icon_id);

  return statement.Run();
}

chrome::FaviconID ThumbnailDatabase::GetFaviconIDForFaviconURL(
    const GURL& icon_url,
    int required_icon_type,
    chrome::IconType* icon_type) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, icon_type FROM favicons WHERE url=? AND (icon_type & ? > 0) "
      "ORDER BY icon_type DESC"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));
  statement.BindInt(1, required_icon_type);

  if (!statement.Step())
    return 0;  // not cached

  if (icon_type)
    *icon_type = static_cast<chrome::IconType>(statement.ColumnInt(1));
  return statement.ColumnInt64(0);
}

bool ThumbnailDatabase::GetFaviconHeader(chrome::FaviconID icon_id,
                                         GURL* icon_url,
                                         chrome::IconType* icon_type) {
  DCHECK(icon_id);

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT url, icon_type FROM favicons WHERE id=?"));
  statement.BindInt64(0, icon_id);

  if (!statement.Step())
    return false;  // No entry for the id.

  if (icon_url)
    *icon_url = GURL(statement.ColumnString(0));
  if (icon_type)
    *icon_type = static_cast<chrome::IconType>(statement.ColumnInt(1));

  return true;
}

chrome::FaviconID ThumbnailDatabase::AddFavicon(
    const GURL& icon_url,
    chrome::IconType icon_type) {

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO favicons (url, icon_type) VALUES (?, ?)"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));
  statement.BindInt(1, icon_type);

  if (!statement.Run())
    return 0;
  return db_.GetLastInsertRowId();
}

chrome::FaviconID ThumbnailDatabase::AddFavicon(
    const GURL& icon_url,
    chrome::IconType icon_type,
    const scoped_refptr<base::RefCountedMemory>& icon_data,
    base::Time time,
    const gfx::Size& pixel_size) {
  chrome::FaviconID icon_id = AddFavicon(icon_url, icon_type);
  if (!icon_id || !AddFaviconBitmap(icon_id, icon_data, time, pixel_size))
    return 0;

  return icon_id;
}

bool ThumbnailDatabase::DeleteFavicon(chrome::FaviconID id) {
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

bool ThumbnailDatabase::UpdateIconMapping(IconMappingID mapping_id,
                                          chrome::FaviconID icon_id) {
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

bool ThumbnailDatabase::HasMappingFor(chrome::FaviconID id) {
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
    chrome::IconType type,
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

bool ThumbnailDatabase::RetainDataForPageUrls(
    const std::vector<GURL>& urls_to_keep) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  // temp.icon_id_mapping generates new icon ids as consecutive
  // integers starting from 1, and maps them to the old icon ids.
  {
    const char kIconMappingCreate[] =
        "CREATE TEMP TABLE icon_id_mapping "
        "("
        "new_icon_id INTEGER PRIMARY KEY,"
        "old_icon_id INTEGER NOT NULL UNIQUE"
        ")";
    if (!db_.Execute(kIconMappingCreate))
      return false;

    // Insert the icon ids for retained urls, skipping duplicates.
    const char kIconMappingSql[] =
        "INSERT OR IGNORE INTO temp.icon_id_mapping (old_icon_id) "
        "SELECT icon_id FROM icon_mapping WHERE page_url = ?";
    sql::Statement statement(db_.GetUniqueStatement(kIconMappingSql));
    for (std::vector<GURL>::const_iterator
             i = urls_to_keep.begin(); i != urls_to_keep.end(); ++i) {
      statement.BindString(0, URLDatabase::GURLToDatabaseURL(*i));
      if (!statement.Run())
        return false;
    }
  }

  {
    const char kRenameIconMappingTable[] =
        "ALTER TABLE icon_mapping RENAME TO old_icon_mapping";
    const char kCopyIconMapping[] =
        "INSERT INTO icon_mapping (page_url, icon_id) "
        "SELECT old.page_url, mapping.new_icon_id "
        "FROM old_icon_mapping AS old "
        "JOIN temp.icon_id_mapping AS mapping "
        "ON (old.icon_id = mapping.old_icon_id)";
    const char kDropOldIconMappingTable[] = "DROP TABLE old_icon_mapping";
    if (!db_.Execute(kRenameIconMappingTable) ||
        !InitIconMappingTable(&db_) ||
        !db_.Execute(kCopyIconMapping) ||
        !db_.Execute(kDropOldIconMappingTable)) {
      return false;
    }
  }

  {
    const char kRenameFaviconsTable[] =
        "ALTER TABLE favicons RENAME TO old_favicons";
    const char kCopyFavicons[] =
        "INSERT INTO favicons (id, url, icon_type) "
        "SELECT mapping.new_icon_id, old.url, old.icon_type "
        "FROM old_favicons AS old "
        "JOIN temp.icon_id_mapping AS mapping "
        "ON (old.id = mapping.old_icon_id)";
    const char kDropOldFaviconsTable[] = "DROP TABLE old_favicons";
    if (!db_.Execute(kRenameFaviconsTable) ||
        !InitFaviconsTable(&db_) ||
        !db_.Execute(kCopyFavicons) ||
        !db_.Execute(kDropOldFaviconsTable)) {
      return false;
    }
  }

  {
    const char kRenameFaviconBitmapsTable[] =
        "ALTER TABLE favicon_bitmaps RENAME TO old_favicon_bitmaps";
    const char kCopyFaviconBitmaps[] =
        "INSERT INTO favicon_bitmaps "
        "  (icon_id, last_updated, image_data, width, height) "
        "SELECT mapping.new_icon_id, old.last_updated, "
        "    old.image_data, old.width, old.height "
        "FROM old_favicon_bitmaps AS old "
        "JOIN temp.icon_id_mapping AS mapping "
        "ON (old.icon_id = mapping.old_icon_id)";
    const char kDropOldFaviconBitmapsTable[] =
        "DROP TABLE old_favicon_bitmaps";
    if (!db_.Execute(kRenameFaviconBitmapsTable) ||
        !InitFaviconBitmapsTable(&db_) ||
        !db_.Execute(kCopyFaviconBitmaps) ||
        !db_.Execute(kDropOldFaviconBitmapsTable)) {
      return false;
    }
  }

  // Renaming the tables adjusts the indices to reference the new
  // name, BUT DOES NOT RENAME THE INDICES.  The DROP will drop the
  // indices, now re-create them against the new tables.
  if (!InitIconMappingIndex() ||
      !InitFaviconsIndex() ||
      !InitFaviconBitmapsIndex()) {
    return false;
  }

  const char kIconMappingDrop[] = "DROP TABLE temp.icon_id_mapping";
  if (!db_.Execute(kIconMappingDrop))
    return false;

  return transaction.Commit();
}

bool ThumbnailDatabase::NeedsMigrationToTopSites() {
  return !use_top_sites_;
}

bool ThumbnailDatabase::RenameAndDropThumbnails(
    const base::FilePath& old_db_file,
    const base::FilePath& new_db_file) {
  // Init favicons tables - same schema as the thumbnails.
  sql::Connection favicons;
  if (OpenDatabase(&favicons, new_db_file) != sql::INIT_OK)
    return false;

  if (!InitFaviconBitmapsTable(&favicons) ||
      !InitFaviconsTable(&favicons) ||
      !InitIconMappingTable(&favicons)) {
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

  sql::Connection::Delete(old_db_file);

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

bool ThumbnailDatabase::InitIconMappingTable(sql::Connection* db) {
  const char kSql[] =
      "CREATE TABLE IF NOT EXISTS icon_mapping"
      "("
      "id INTEGER PRIMARY KEY,"
      "page_url LONGVARCHAR NOT NULL,"
      "icon_id INTEGER"
      ")";
  return db->Execute(kSql);
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
                                                chrome::FaviconID icon_id) {
  const char kSql[] =
      "INSERT INTO icon_mapping (page_url, icon_id) VALUES (?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
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

bool ThumbnailDatabase::UpgradeToVersion7() {
  bool success =
      db_.Execute("CREATE TABLE temp_favicons ("
                  "id INTEGER PRIMARY KEY,"
                  "url LONGVARCHAR NOT NULL,"
                  "icon_type INTEGER DEFAULT 1)") &&
      db_.Execute("INSERT INTO temp_favicons (id, url, icon_type) "
                  "SELECT id, url, icon_type FROM favicons") &&
      db_.Execute("DROP TABLE favicons") &&
      db_.Execute("ALTER TABLE temp_favicons RENAME TO favicons") &&
      db_.Execute("CREATE INDEX IF NOT EXISTS favicons_url ON favicons(url)");

  if (!success)
    return false;

  meta_table_.SetVersionNumber(7);
  meta_table_.SetCompatibleVersionNumber(std::min(7, kCompatibleVersionNumber));
  return true;
}

}  // namespace history
