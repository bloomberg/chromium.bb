// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_DATABASE_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_DATABASE_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/media_gallery/media_gallery_database_types.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

namespace chrome {

// Encapsulates the SQL database that stores pre-parsed media gallery metadata
// for search and retrieval.
class MediaGalleryDatabase {
 public:
  MediaGalleryDatabase();
  virtual ~MediaGalleryDatabase();

  // Must call this function to complete initialization. Will return true on
  // success. On false, no other function should be called.
  sql::InitStatus Init(const FilePath& database_path);

  // Returns the current version that we will generate media gallery databases
  // with.
  static int GetCurrentVersion();

  // Sets the id field of the input collection_row to the generated unique
  // key value and returns the same. On failure, returns zero.
  int CreateCollectionRow(CollectionRow* collection_row);

  // Finds the row with the specified id and fills its data into the collection
  // pointer passed by the caller. Returns true on success.
  bool GetCollectionRow(CollectionId id, CollectionRow* collection);

 protected:
  virtual sql::Connection& GetDB();

  // Initializes an already open database.
  sql::InitStatus InitInternal(sql::Connection* db);
  static bool DoesCollectionsTableExist(sql::Connection* db);

 private:
  static bool CreateCollectionsTable(sql::Connection* db);
  sql::InitStatus EnsureCurrentVersion();
  void FillCollectionRow(const sql::Statement& statement, CollectionRow* row);

  sql::Connection db_;
  sql::MetaTable meta_table_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleryDatabase);
};

}  // namespace chrome

std::ostream& operator<<(std::ostream& out, const chrome::CollectionRow& row);

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERY_DATABASE_H_
