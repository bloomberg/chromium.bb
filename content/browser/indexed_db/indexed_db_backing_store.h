// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"

namespace content {

class LevelDBComparator;
class LevelDBDatabase;

class LevelDBFactory {
 public:
  virtual ~LevelDBFactory() {}
  virtual scoped_ptr<LevelDBDatabase> OpenLevelDB(
      const base::FilePath& file_name,
      const LevelDBComparator* comparator,
      bool* is_disk_full = NULL) = 0;
  virtual bool DestroyLevelDB(const base::FilePath& file_name) = 0;
};

class CONTENT_EXPORT IndexedDBBackingStore
    : public base::RefCounted<IndexedDBBackingStore> {
 public:
  class CONTENT_EXPORT Transaction;

  static scoped_refptr<IndexedDBBackingStore> Open(
      const string16& database_identifier,
      const base::FilePath& path_base,
      const string16& file_identifier);
  static scoped_refptr<IndexedDBBackingStore> Open(
      const string16& database_identifier,
      const base::FilePath& path_base,
      const string16& file_identifier,
      LevelDBFactory* factory);
  static scoped_refptr<IndexedDBBackingStore> OpenInMemory(
      const string16& identifier);
  static scoped_refptr<IndexedDBBackingStore> OpenInMemory(
      const string16& identifier,
      LevelDBFactory* factory);
  base::WeakPtr<IndexedDBBackingStore> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  virtual std::vector<string16> GetDatabaseNames();
  virtual bool GetIDBDatabaseMetaData(const string16& name,
                                      IndexedDBDatabaseMetadata* metadata,
                                      bool* success) WARN_UNUSED_RESULT;
  virtual bool CreateIDBDatabaseMetaData(const string16& name,
                                         const string16& version,
                                         int64 int_version,
                                         int64* row_id);
  virtual bool UpdateIDBDatabaseMetaData(
      IndexedDBBackingStore::Transaction* transaction,
      int64 row_id,
      const string16& version);
  virtual bool UpdateIDBDatabaseIntVersion(
      IndexedDBBackingStore::Transaction* transaction,
      int64 row_id,
      int64 int_version);
  virtual bool DeleteDatabase(const string16& name);

  bool GetObjectStores(int64 database_id,
                       IndexedDBDatabaseMetadata::ObjectStoreMap* map)
      WARN_UNUSED_RESULT;
  virtual bool CreateObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const string16& name,
      const IndexedDBKeyPath& key_path,
      bool auto_increment);
  virtual bool DeleteObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id) WARN_UNUSED_RESULT;

  class CONTENT_EXPORT RecordIdentifier {
   public:
    RecordIdentifier(const std::vector<char>& primary_key, int64 version);
    RecordIdentifier();
    ~RecordIdentifier();

    const std::vector<char>& primary_key() const { return primary_key_; }
    int64 version() const { return version_; }
    void Reset(const std::vector<char>& primary_key, int64 version) {
      primary_key_ = primary_key;
      version_ = version;
    }

   private:
    std::vector<char>
        primary_key_;  // TODO(jsbell): Make it more clear that this is
                       // the *encoded* version of the key.
    int64 version_;
    DISALLOW_COPY_AND_ASSIGN(RecordIdentifier);
  };

  virtual bool GetRecord(IndexedDBBackingStore::Transaction* transaction,
                         int64 database_id,
                         int64 object_store_id,
                         const IndexedDBKey& key,
                         std::vector<char>* record) WARN_UNUSED_RESULT;
  virtual bool PutRecord(IndexedDBBackingStore::Transaction* transaction,
                         int64 database_id,
                         int64 object_store_id,
                         const IndexedDBKey& key,
                         const std::vector<char>& value,
                         RecordIdentifier* record) WARN_UNUSED_RESULT;
  virtual bool ClearObjectStore(IndexedDBBackingStore::Transaction* transaction,
                                int64 database_id,
                                int64 object_store_id) WARN_UNUSED_RESULT;
  virtual bool DeleteRecord(IndexedDBBackingStore::Transaction* transaction,
                            int64 database_id,
                            int64 object_store_id,
                            const RecordIdentifier& record) WARN_UNUSED_RESULT;
  virtual bool GetKeyGeneratorCurrentNumber(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64* current_number) WARN_UNUSED_RESULT;
  virtual bool MaybeUpdateKeyGeneratorCurrentNumber(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 new_state,
      bool check_current) WARN_UNUSED_RESULT;
  virtual bool KeyExistsInObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKey& key,
      RecordIdentifier* found_record_identifier,
      bool* found) WARN_UNUSED_RESULT;

  virtual bool CreateIndex(IndexedDBBackingStore::Transaction* transaction,
                           int64 database_id,
                           int64 object_store_id,
                           int64 index_id,
                           const string16& name,
                           const IndexedDBKeyPath& key_path,
                           bool is_unique,
                           bool is_multi_entry) WARN_UNUSED_RESULT;
  virtual bool DeleteIndex(IndexedDBBackingStore::Transaction* transaction,
                           int64 database_id,
                           int64 object_store_id,
                           int64 index_id) WARN_UNUSED_RESULT;
  virtual bool PutIndexDataForRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKey& key,
      const RecordIdentifier& record) WARN_UNUSED_RESULT;
  virtual bool GetPrimaryKeyViaIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKey& key,
      scoped_ptr<IndexedDBKey>* primary_key) WARN_UNUSED_RESULT;
  virtual bool KeyExistsInIndex(IndexedDBBackingStore::Transaction* transaction,
                                int64 database_id,
                                int64 object_store_id,
                                int64 index_id,
                                const IndexedDBKey& key,
                                scoped_ptr<IndexedDBKey>* found_primary_key,
                                bool* exists) WARN_UNUSED_RESULT;

  class Cursor {
   public:
    virtual ~Cursor();

    enum IteratorState {
      READY = 0,
      SEEK
    };

    struct CursorOptions {
      CursorOptions();
      ~CursorOptions();
      int64 database_id;
      int64 object_store_id;
      int64 index_id;
      std::vector<char> low_key;
      bool low_open;
      std::vector<char> high_key;
      bool high_open;
      bool forward;
      bool unique;
    };

    const IndexedDBKey& key() const { return *current_key_; }
    bool ContinueFunction(const IndexedDBKey* = 0, IteratorState = SEEK);
    bool Advance(uint32 count);
    bool FirstSeek();

    virtual Cursor* Clone() = 0;
    virtual const IndexedDBKey& primary_key() const;
    virtual std::vector<char>* Value() = 0;
    virtual const RecordIdentifier& record_identifier() const;
    virtual bool LoadCurrentRow() = 0;

   protected:
    Cursor(LevelDBTransaction* transaction,
           const CursorOptions& cursor_options);
    explicit Cursor(const IndexedDBBackingStore::Cursor* other);

    virtual std::vector<char> EncodeKey(const IndexedDBKey& key) = 0;

    bool IsPastBounds() const;
    bool HaveEnteredRange() const;

    LevelDBTransaction* transaction_;
    const CursorOptions cursor_options_;
    scoped_ptr<LevelDBIterator> iterator_;
    scoped_ptr<IndexedDBKey> current_key_;
    IndexedDBBackingStore::RecordIdentifier record_identifier_;
  };

  virtual scoped_ptr<Cursor> OpenObjectStoreKeyCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection);
  virtual scoped_ptr<Cursor> OpenObjectStoreCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection);
  virtual scoped_ptr<Cursor> OpenIndexKeyCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection);
  virtual scoped_ptr<Cursor> OpenIndexCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64 database_id,
      int64 object_store_id,
      int64 index_id,
      const IndexedDBKeyRange& key_range,
      indexed_db::CursorDirection);

  class Transaction {
   public:
    explicit Transaction(IndexedDBBackingStore* backing_store);
    ~Transaction();
    void Begin();
    bool Commit();
    void Rollback();
    void Reset() {
      backing_store_ = NULL;
      transaction_ = NULL;
    }

    static LevelDBTransaction* LevelDBTransactionFrom(
        Transaction* transaction) {
      return transaction->transaction_.get();
    }

   private:
    IndexedDBBackingStore* backing_store_;
    scoped_refptr<LevelDBTransaction> transaction_;
  };

 protected:
  IndexedDBBackingStore(const string16& identifier,
                        scoped_ptr<LevelDBDatabase> db,
                        scoped_ptr<LevelDBComparator> comparator);
  virtual ~IndexedDBBackingStore();
  friend class base::RefCounted<IndexedDBBackingStore>;

 private:
  static scoped_refptr<IndexedDBBackingStore> Create(
      const string16& identifier,
      scoped_ptr<LevelDBDatabase> db,
      scoped_ptr<LevelDBComparator> comparator);

  bool FindKeyInIndex(IndexedDBBackingStore::Transaction* transaction,
                      int64 database_id,
                      int64 object_store_id,
                      int64 index_id,
                      const IndexedDBKey& key,
                      std::vector<char>* found_encoded_primary_key,
                      bool* found);
  bool GetIndexes(int64 database_id,
                  int64 object_store_id,
                  IndexedDBObjectStoreMetadata::IndexMap* map)
      WARN_UNUSED_RESULT;

  string16 identifier_;

  scoped_ptr<LevelDBDatabase> db_;
  scoped_ptr<LevelDBComparator> comparator_;
  base::WeakPtrFactory<IndexedDBBackingStore> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_
