// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_

#include <vector>

#include "content/browser/indexed_db/indexed_db_backing_store.h"

namespace content {

class IndexedDBFakeBackingStore : public IndexedDBBackingStore {
 public:
  IndexedDBFakeBackingStore()
      : IndexedDBBackingStore(std::string(),
                              scoped_ptr<LevelDBDatabase>(),
                              scoped_ptr<LevelDBComparator>()) {}
  virtual std::vector<string16> GetDatabaseNames() OVERRIDE;
  virtual bool GetIDBDatabaseMetaData(const string16& name,
                                      IndexedDBDatabaseMetadata*,
                                      bool* found) OVERRIDE;
  virtual bool CreateIDBDatabaseMetaData(const string16& name,
                                         const string16& version,
                                         int64 int_version,
                                         int64* row_id) OVERRIDE;
  virtual bool UpdateIDBDatabaseMetaData(Transaction*,
                                         int64 row_id,
                                         const string16& version) OVERRIDE;
  virtual bool UpdateIDBDatabaseIntVersion(Transaction*,
                                           int64 row_id,
                                           int64 version) OVERRIDE;
  virtual bool DeleteDatabase(const string16& name) OVERRIDE;

  virtual bool CreateObjectStore(Transaction*,
                                 int64 database_id,
                                 int64 object_store_id,
                                 const string16& name,
                                 const IndexedDBKeyPath&,
                                 bool auto_increment) OVERRIDE;

  virtual bool ClearObjectStore(Transaction*,
                                int64 database_id,
                                int64 object_store_id) OVERRIDE;
  virtual bool DeleteRecord(Transaction*,
                            int64 database_id,
                            int64 object_store_id,
                            const RecordIdentifier&) OVERRIDE;
  virtual bool GetKeyGeneratorCurrentNumber(Transaction*,
                                            int64 database_id,
                                            int64 object_store_id,
                                            int64* current_number) OVERRIDE;
  virtual bool MaybeUpdateKeyGeneratorCurrentNumber(Transaction*,
                                                    int64 database_id,
                                                    int64 object_store_id,
                                                    int64 new_number,
                                                    bool check_current)
      OVERRIDE;
  virtual bool KeyExistsInObjectStore(Transaction*,
                                      int64 database_id,
                                      int64 object_store_id,
                                      const IndexedDBKey&,
                                      RecordIdentifier* found_record_identifier,
                                      bool* found) OVERRIDE;

  virtual bool CreateIndex(Transaction*,
                           int64 database_id,
                           int64 object_store_id,
                           int64 index_id,
                           const string16& name,
                           const IndexedDBKeyPath&,
                           bool is_unique,
                           bool is_multi_entry) OVERRIDE;
  virtual bool DeleteIndex(Transaction*,
                           int64 database_id,
                           int64 object_store_id,
                           int64 index_id) OVERRIDE;
  virtual bool PutIndexDataForRecord(Transaction*,
                                     int64 database_id,
                                     int64 object_store_id,
                                     int64 index_id,
                                     const IndexedDBKey&,
                                     const RecordIdentifier&) OVERRIDE;

  virtual scoped_ptr<Cursor> OpenObjectStoreKeyCursor(
      Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection) OVERRIDE;
  virtual scoped_ptr<Cursor> OpenObjectStoreCursor(
      Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection) OVERRIDE;
  virtual scoped_ptr<Cursor> OpenIndexKeyCursor(
      Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection) OVERRIDE;
  virtual scoped_ptr<Cursor> OpenIndexCursor(Transaction* transaction,
                                             int64 database_id,
                                             int64 object_store_id,
                                             int64 index_id,
                                             const IndexedDBKeyRange& key_range,
                                             indexed_db::CursorDirection)
      OVERRIDE;

 protected:
  friend class base::RefCounted<IndexedDBFakeBackingStore>;
  virtual ~IndexedDBFakeBackingStore();
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FAKE_BACKING_STORE_H_
