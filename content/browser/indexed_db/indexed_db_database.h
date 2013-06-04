// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"

namespace content {

class IndexedDBCallbacksWrapper;
class IndexedDBDatabaseCallbacksWrapper;
struct IndexedDBDatabaseMetadata;

// This is implemented by IndexedDBDatabaseImpl and optionally others (in order
// to proxy calls across process barriers). All calls to these classes should be
// non-blocking and trigger work on a background thread if necessary.
class IndexedDBDatabase : public base::RefCounted<IndexedDBDatabase> {
 public:
  virtual void CreateObjectStore(int64 transaction_id,
                                 int64 object_store_id,
                                 const string16& name,
                                 const IndexedDBKeyPath& key_path,
                                 bool auto_increment) = 0;
  virtual void DeleteObjectStore(int64 transaction_id,
                                 int64 object_store_id) = 0;
  virtual void CreateTransaction(
      int64 transaction_id,
      scoped_refptr<IndexedDBDatabaseCallbacksWrapper> callbacks,
      const std::vector<int64>& object_store_ids,
      uint16 mode) = 0;
  virtual void Close(
      scoped_refptr<IndexedDBDatabaseCallbacksWrapper> callbacks) = 0;

  // Transaction-specific operations.
  virtual void Commit(int64 transaction_id) = 0;
  virtual void Abort(int64 transaction_id) = 0;
  virtual void Abort(int64 transaction_id,
                     const IndexedDBDatabaseError& error) = 0;

  virtual void CreateIndex(int64 transaction_id,
                           int64 object_store_id,
                           int64 index_id,
                           const string16& name,
                           const IndexedDBKeyPath& key_path,
                           bool unique,
                           bool multi_entry) = 0;
  virtual void DeleteIndex(int64 transaction_id,
                           int64 object_store_id,
                           int64 index_id) = 0;

  enum TaskType {
    NORMAL_TASK = 0,
    PREEMPTIVE_TASK
  };

  enum PutMode {
    ADD_OR_UPDATE,
    ADD_ONLY,
    CURSOR_UPDATE
  };

  static const int64 kMinimumIndexId = 30;

  typedef std::vector<IndexedDBKey> IndexKeys;

  virtual void Get(int64 transaction_id,
                   int64 object_store_id,
                   int64 index_id,
                   scoped_ptr<IndexedDBKeyRange> key_range,
                   bool key_only,
                   scoped_refptr<IndexedDBCallbacksWrapper> callbacks) = 0;
  // This will swap() with value.
  virtual void Put(int64 transaction_id,
                   int64 object_store_id,
                   std::vector<char>* value,
                   scoped_ptr<IndexedDBKey> key,
                   PutMode mode,
                   scoped_refptr<IndexedDBCallbacksWrapper> callbacks,
                   const std::vector<int64>& index_ids,
                   const std::vector<IndexKeys>& index_keys) = 0;
  virtual void SetIndexKeys(int64 transaction_id,
                            int64 object_store_id,
                            scoped_ptr<IndexedDBKey> primary_key,
                            const std::vector<int64>& index_ids,
                            const std::vector<IndexKeys>& index_keys) = 0;
  virtual void SetIndexesReady(int64 transaction_id,
                               int64 object_store_id,
                               const std::vector<int64>& index_ids) = 0;
  virtual void OpenCursor(
      int64 transaction_id,
      int64 object_store_id,
      int64 index_id,
      scoped_ptr<IndexedDBKeyRange> key_range,
      indexed_db::CursorDirection direction,
      bool key_only,
      TaskType task_type,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks) = 0;
  virtual void Count(int64 transaction_id,
                     int64 object_store_id,
                     int64 index_id,
                     scoped_ptr<IndexedDBKeyRange> key_range,
                     scoped_refptr<IndexedDBCallbacksWrapper> callbacks) = 0;
  virtual void DeleteRange(
      int64 transaction_id,
      int64 object_store_id,
      scoped_ptr<IndexedDBKeyRange> key_range,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks) = 0;
  virtual void Clear(int64 transaction_id,
                     int64 object_store_id,
                     scoped_refptr<IndexedDBCallbacksWrapper> callbacks) = 0;

 protected:
  virtual ~IndexedDBDatabase() {}
  friend class base::RefCounted<IndexedDBDatabase>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
