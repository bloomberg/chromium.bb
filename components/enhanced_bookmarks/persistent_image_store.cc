// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/persistent_image_store.h"

#include "base/files/file.h"
#include "base/logging.h"
#include "components/enhanced_bookmarks/image_store_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace {
// Current version number. Databases are written at the "current" version
// number, but any previous version that can read the "compatible" one can make
// do with our database without *too* many bad effects.
const int kCurrentVersionNumber = 2;
const int kCompatibleVersionNumber = 1;

bool InitTables(sql::Connection& db) {
  const char kTableSql[] =
      "CREATE TABLE IF NOT EXISTS images_by_url ("
      "page_url LONGVARCHAR NOT NULL,"
      "image_url LONGVARCHAR NOT NULL,"
      "image_data BLOB,"
      "width INTEGER,"
      "height INTEGER,"
      "image_dominant_color INTEGER"
      ")";
  if (!db.Execute(kTableSql))
    return false;
  return true;
}

bool InitIndices(sql::Connection& db) {
  const char kIndexSql[] =
      "CREATE INDEX IF NOT EXISTS images_by_url_idx ON images_by_url(page_url)";
  if (!db.Execute(kIndexSql))
    return false;
  return true;
}

// V1 didn't store the dominant color of an image. Creates the column to store
// a dominant color in the database. The value will be filled when queried for a
// one time cost.
bool MigrateImagesWithNoDominantColor(sql::Connection& db) {
  if (!db.DoesTableExist("images_by_url")) {
    NOTREACHED() << "images_by_url table should exist before migration.";
    return false;
  }

  if (!db.DoesColumnExist("images_by_url", "image_dominant_color")) {
    // The initial version doesn't have the image_dominant_color column, it is
    // added to the table here.
    if (!db.Execute(
            "ALTER TABLE images_by_url "
            "ADD COLUMN image_dominant_color INTEGER")) {
      return false;
    }
  }
  return true;
}

sql::InitStatus EnsureCurrentVersion(sql::Connection& db,
                                     sql::MetaTable& meta_table) {
  // 1- Newer databases than designed for can't be read.
  if (meta_table.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Image DB is too new.";
    return sql::INIT_TOO_NEW;
  }

  int cur_version = meta_table.GetVersionNumber();

  // 2- Put migration code here.

  if (cur_version == 1) {
    if (!MigrateImagesWithNoDominantColor(db)) {
      LOG(WARNING) << "Unable to update image DB to version 1.";
      return sql::INIT_FAILURE;
    }
    ++cur_version;
    meta_table.SetVersionNumber(cur_version);
    meta_table.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  // 3- When the version is too old, just try to continue anyway, there should
  // not be a released product that makes a database too old to handle.
  LOG_IF(WARNING, cur_version < kCurrentVersionNumber)
      << "Image DB version " << cur_version << " is too old to handle.";

  return sql::INIT_OK;
}

sql::InitStatus OpenDatabaseImpl(sql::Connection& db,
                                 sql::MetaTable& meta_table,
                                 const base::FilePath& db_path) {
  DCHECK(!db.is_open());

  db.set_histogram_tag("BookmarkImages");
  // TODO(noyau): Set page and cache sizes?
  // TODO(noyau): Set error callback?

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while running, and this will give somewhat improved performance.
  db.set_exclusive_locking();

  if (!db.Open(db_path))
    return sql::INIT_FAILURE;

  // Scope initialization in a transaction so it can't be partially initialized.
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return sql::INIT_FAILURE;

  // Initialize the meta table.
  int cur_version = meta_table.DoesTableExist(&db)
                        ? kCurrentVersionNumber
                        : 1;  // Only v1 didn't have a meta table.
  if (!meta_table.Init(&db, cur_version,
                       std::min(cur_version, kCompatibleVersionNumber)))
    return sql::INIT_FAILURE;

  // Create the tables.
  if (!InitTables(db) || !InitIndices(db))
    return sql::INIT_FAILURE;

  // Check the version.
  sql::InitStatus version_status = EnsureCurrentVersion(db, meta_table);
  if (version_status != sql::INIT_OK)
    return version_status;

  // Initialization is complete.
  if (!transaction.Commit())
    return sql::INIT_FAILURE;

  return sql::INIT_OK;
}

}  // namespace

const char PersistentImageStore::kBookmarkImageStoreDb[] =
    "BookmarkImageAndUrlStore.db";

PersistentImageStore::PersistentImageStore(const base::FilePath& path)
    : ImageStore(),
      path_(path.Append(
          base::FilePath::FromUTF8Unsafe(kBookmarkImageStoreDb))) {
}

bool PersistentImageStore::HasKey(const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (OpenDatabase() != sql::INIT_OK)
    return false;

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT COUNT(*) FROM images_by_url WHERE page_url = ?"));
  statement.BindString(0, page_url.possibly_invalid_spec());

  int count = statement.Step() ? statement.ColumnInt(0) : 0;

  return !!count;
}

void PersistentImageStore::Insert(
    const GURL& page_url,
    scoped_refptr<enhanced_bookmarks::ImageRecord> record) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (OpenDatabase() != sql::INIT_OK)
    return;

  Erase(page_url);  // Remove previous image for this url, if any.
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO images_by_url "
      "(page_url, image_url, image_data, width, height, image_dominant_color)"
      "VALUES (?, ?, ?, ?, ?, ?)"));

  statement.BindString(0, page_url.possibly_invalid_spec());
  statement.BindString(1, record->url.possibly_invalid_spec());

  scoped_refptr<base::RefCountedMemory> image_bytes =
      enhanced_bookmarks::BytesForImage(*record->image);

  // Insert an empty image in case encoding fails.
  if (!image_bytes.get())
    image_bytes = enhanced_bookmarks::BytesForImage(gfx::Image());

  CHECK(image_bytes.get());

  statement.BindBlob(2, image_bytes->front(), (int)image_bytes->size());

  statement.BindInt(3, record->image->Size().width());
  statement.BindInt(4, record->image->Size().height());
  statement.BindInt(5, record->dominant_color);
  statement.Run();
}

void PersistentImageStore::Erase(const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (OpenDatabase() != sql::INIT_OK)
    return;

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM images_by_url WHERE page_url = ?"));
  statement.BindString(0, page_url.possibly_invalid_spec());
  statement.Run();
}

scoped_refptr<enhanced_bookmarks::ImageRecord> PersistentImageStore::Get(
    const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  scoped_refptr<enhanced_bookmarks::ImageRecord> image_record(
      new enhanced_bookmarks::ImageRecord());
  if (OpenDatabase() != sql::INIT_OK)
    return image_record;

  bool stored_image_record_needs_update = false;

  // Scope the SELECT statement.
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT image_data, image_url, image_dominant_color FROM images_by_url "
        "WHERE page_url = ?"));

    statement.BindString(0, page_url.possibly_invalid_spec());

    if (!statement.Step())
      return image_record;

    // Image.
    if (statement.ColumnByteLength(0) > 0) {
      scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
      statement.ColumnBlobAsVector(0, &data->data());
      *image_record->image = enhanced_bookmarks::ImageForBytes(data);
    }

    // URL.
    image_record->url = GURL(statement.ColumnString(1));

    // Dominant color.
    if (statement.ColumnType(2) != sql::COLUMN_TYPE_NULL) {
      image_record->dominant_color = SkColor(statement.ColumnInt(2));
    } else {
      // The dominant color was not computed when the image was first
      // stored.
      // Compute it now.
      image_record->dominant_color =
          enhanced_bookmarks::DominantColorForImage(*image_record->image);
      stored_image_record_needs_update = true;
    }

    // Make sure there is only one record for page_url.
    DCHECK(!statement.Step());
  }

  if (stored_image_record_needs_update) {
    Erase(page_url);
    Insert(page_url, image_record);
  }

  return image_record;
}

gfx::Size PersistentImageStore::GetSize(const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (OpenDatabase() != sql::INIT_OK)
    return gfx::Size();

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT width, height FROM images_by_url WHERE page_url = ?"));

  statement.BindString(0, page_url.possibly_invalid_spec());

  while (statement.Step()) {
    if (statement.ColumnByteLength(0) > 0) {
      int width = statement.ColumnInt(0);
      int height = statement.ColumnInt(1);
      return gfx::Size(width, height);
    }
  }
  return gfx::Size();
}

void PersistentImageStore::GetAllPageUrls(std::set<GURL>* urls) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(urls->empty());
  if (OpenDatabase() != sql::INIT_OK)
    return;

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT page_url FROM images_by_url"));
  while (statement.Step())
    urls->insert(GURL(statement.ColumnString(0)));
}

void PersistentImageStore::ClearAll() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (OpenDatabase() != sql::INIT_OK)
    return;

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM images_by_url"));
  statement.Run();
}

int64 PersistentImageStore::GetStoreSizeInBytes() {
  base::File file(path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  return file.IsValid() ? file.GetLength() : -1;
}

PersistentImageStore::~PersistentImageStore() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

sql::InitStatus PersistentImageStore::OpenDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (db_.is_open())
    return sql::INIT_OK;

  const size_t kAttempts = 2;

  sql::InitStatus status = sql::INIT_FAILURE;
  for (size_t i = 0; i < kAttempts; ++i) {
    status = OpenDatabaseImpl(db_, meta_table_, path_);
    if (status == sql::INIT_OK)
      return status;

    // Can't open, raze().
    if (db_.is_open())
      db_.Raze();
    db_.Close();
  }

  DCHECK(false) << "Can't open image DB";
  return sql::INIT_FAILURE;
}
