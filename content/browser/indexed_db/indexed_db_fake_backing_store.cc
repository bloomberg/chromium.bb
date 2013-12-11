// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"

#include "base/memory/scoped_ptr.h"

namespace content {

IndexedDBFakeBackingStore::~IndexedDBFakeBackingStore() {}

std::vector<base::string16> IndexedDBFakeBackingStore::GetDatabaseNames() {
  return std::vector<base::string16>();
}
bool IndexedDBFakeBackingStore::GetIDBDatabaseMetaData(
    const base::string16& name,
    IndexedDBDatabaseMetadata*,
    bool* found) {
  return true;
}

bool IndexedDBFakeBackingStore::CreateIDBDatabaseMetaData(
    const base::string16& name,
    const base::string16& version,
    int64 int_version,
    int64* row_id) {
  return true;
}
bool IndexedDBFakeBackingStore::UpdateIDBDatabaseIntVersion(Transaction*,
                                                            int64 row_id,
                                                            int64 version) {
  return false;
}
bool IndexedDBFakeBackingStore::DeleteDatabase(const base::string16& name) {
  return true;
}

bool IndexedDBFakeBackingStore::CreateObjectStore(Transaction*,
                                                  int64 database_id,
                                                  int64 object_store_id,
                                                  const base::string16& name,
                                                  const IndexedDBKeyPath&,
                                                  bool auto_increment) {
  return false;
}

bool IndexedDBFakeBackingStore::ClearObjectStore(Transaction*,
                                                 int64 database_id,
                                                 int64 object_store_id) {
  return false;
}
bool IndexedDBFakeBackingStore::DeleteRecord(Transaction*,
                                             int64 database_id,
                                             int64 object_store_id,
                                             const RecordIdentifier&) {
  return false;
}
bool IndexedDBFakeBackingStore::GetKeyGeneratorCurrentNumber(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    int64* current_number) {
  return true;
}
bool IndexedDBFakeBackingStore::MaybeUpdateKeyGeneratorCurrentNumber(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    int64 new_number,
    bool check_current) {
  return true;
}
bool IndexedDBFakeBackingStore::KeyExistsInObjectStore(
    Transaction*,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKey&,
    RecordIdentifier* found_record_identifier,
    bool* found) {
  return true;
}

bool IndexedDBFakeBackingStore::CreateIndex(Transaction*,
                                            int64 database_id,
                                            int64 object_store_id,
                                            int64 index_id,
                                            const base::string16& name,
                                            const IndexedDBKeyPath&,
                                            bool is_unique,
                                            bool is_multi_entry) {
  return false;
}

bool IndexedDBFakeBackingStore::DeleteIndex(Transaction*,
                                            int64 database_id,
                                            int64 object_store_id,
                                            int64 index_id) {
  return false;
}
bool IndexedDBFakeBackingStore::PutIndexDataForRecord(Transaction*,
                                                      int64 database_id,
                                                      int64 object_store_id,
                                                      int64 index_id,
                                                      const IndexedDBKey&,
                                                      const RecordIdentifier&) {
  return false;
}

scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKeyRange& key_range,
    indexed_db::CursorDirection) {
  return scoped_ptr<IndexedDBBackingStore::Cursor>();
}
scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    const IndexedDBKeyRange& key_range,
    indexed_db::CursorDirection) {
  return scoped_ptr<IndexedDBBackingStore::Cursor>();
}
scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    indexed_db::CursorDirection) {
  return scoped_ptr<IndexedDBBackingStore::Cursor>();
}
scoped_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64 database_id,
    int64 object_store_id,
    int64 index_id,
    const IndexedDBKeyRange& key_range,
    indexed_db::CursorDirection) {
  return scoped_ptr<IndexedDBBackingStore::Cursor>();
}

IndexedDBFakeBackingStore::FakeTransaction::FakeTransaction(bool result)
    : IndexedDBBackingStore::Transaction(NULL), result_(result) {}
void IndexedDBFakeBackingStore::FakeTransaction::Begin() {}
bool IndexedDBFakeBackingStore::FakeTransaction::Commit() { return result_; }
void IndexedDBFakeBackingStore::FakeTransaction::Rollback() {}

}  // namespace content
