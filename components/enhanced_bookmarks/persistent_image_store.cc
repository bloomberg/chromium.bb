// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/persistent_image_store.h"

#include "base/files/file.h"
#include "components/enhanced_bookmarks/image_store_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace {

bool InitTables(sql::Connection& db) {
  const char kTableSql[] =
      "CREATE TABLE IF NOT EXISTS images_by_url ("
      "page_url LONGVARCHAR NOT NULL,"
      "image_url LONGVARCHAR NOT NULL,"
      "image_data BLOB,"
      "width INTEGER,"
      "height INTEGER"
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

sql::InitStatus OpenDatabaseImpl(sql::Connection& db,
                                 const base::FilePath& db_path) {
  DCHECK(!db.is_open());

  db.set_histogram_tag("BookmarkImages");
  // TODO(noyau): Set page and cache sizes?
  // TODO(noyau): Set error callback?

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db.set_exclusive_locking();

  if (!db.Open(db_path))
    return sql::INIT_FAILURE;

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return sql::INIT_FAILURE;

  // Create the tables.
  if (!InitTables(db) ||
      !InitIndices(db)) {
    return sql::INIT_FAILURE;
  }

  // Initialization is complete.
  if (!transaction.Commit())
    return sql::INIT_FAILURE;

  return sql::INIT_OK;
}

}  // namespace

PersistentImageStore::PersistentImageStore(const base::FilePath& path)
    : ImageStore(),
      path_(path.Append(
          base::FilePath::FromUTF8Unsafe("BookmarkImageAndUrlStore.db"))) {
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

void PersistentImageStore::Insert(const GURL& page_url,
                                  const GURL& image_url,
                                  const gfx::Image& image) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (OpenDatabase() != sql::INIT_OK)
    return;

  Erase(page_url);  // Remove previous image for this url, if any.
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO images_by_url "
      "(page_url, image_url, image_data, width, height)"
      "VALUES (?, ?, ?, ?, ?)"));

  statement.BindString(0, page_url.possibly_invalid_spec());
  statement.BindString(1, image_url.possibly_invalid_spec());

  scoped_refptr<base::RefCountedMemory> image_bytes =
        enhanced_bookmarks::BytesForImage(image);

  // Insert an empty image in case encoding fails.
  if (!image_bytes.get())
    image_bytes = enhanced_bookmarks::BytesForImage(gfx::Image());

  CHECK(image_bytes.get());

  statement.BindBlob(2, image_bytes->front(), (int)image_bytes->size());

  statement.BindInt(3, image.Size().width());
  statement.BindInt(4, image.Size().height());
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

std::pair<gfx::Image, GURL> PersistentImageStore::Get(const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (OpenDatabase() != sql::INIT_OK)
    return std::make_pair(gfx::Image(), GURL());

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT image_data, image_url FROM images_by_url WHERE page_url = ?"));

  statement.BindString(0, page_url.possibly_invalid_spec());

  while (statement.Step()) {
    if (statement.ColumnByteLength(0) > 0) {
      scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
      statement.ColumnBlobAsVector(0, &data->data());

      return std::make_pair(enhanced_bookmarks::ImageForBytes(data),
                            GURL(statement.ColumnString(1)));
    }
  }

  return std::make_pair(gfx::Image(), GURL());
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
    status = OpenDatabaseImpl(db_, path_);
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
