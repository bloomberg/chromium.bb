// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
#define CONTENT_RENDERER_INDEXED_DB_WEBIDBDATABASE_IMPL_H_

#include <stdint.h>

#include <set>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database.h"

namespace blink {
class WebBlobInfo;
class WebIDBCallbacks;
class WebString;
}

namespace content {

class IndexedDBCallbacksImpl;

class CONTENT_EXPORT WebIDBDatabaseImpl : public blink::WebIDBDatabase {
 public:
  WebIDBDatabaseImpl(blink::mojom::IDBDatabaseAssociatedPtrInfo database);
  ~WebIDBDatabaseImpl() override;

  // blink::WebIDBDatabase
  void CreateObjectStore(long long transaction_id,
                         long long objectstore_id,
                         const blink::WebString& name,
                         const blink::WebIDBKeyPath&,
                         bool auto_increment) override;
  void DeleteObjectStore(long long transaction_id,
                         long long object_store_id) override;
  void RenameObjectStore(long long transaction_id,
                         long long object_store_id,
                         const blink::WebString& new_name) override;
  void CreateTransaction(long long transaction_id,
                         const blink::WebVector<long long>& scope,
                         blink::WebIDBTransactionMode mode) override;

  void Close() override;
  void VersionChangeIgnored() override;

  void AddObserver(long long transaction_id,
                   int32_t observer_id,
                   bool include_transaction,
                   bool no_records,
                   bool values,
                   const std::bitset<blink::kWebIDBOperationTypeCount>&
                       operation_types) override;
  void RemoveObservers(
      const blink::WebVector<int32_t>& observer_ids_to_remove) override;

  void Get(long long transaction_id,
           long long object_store_id,
           long long index_id,
           const blink::WebIDBKeyRange&,
           bool key_only,
           blink::WebIDBCallbacks*) override;
  void GetAll(long long transaction_id,
              long long object_store_id,
              long long index_id,
              const blink::WebIDBKeyRange&,
              long long max_count,
              bool key_only,
              blink::WebIDBCallbacks*) override;
  void Put(long long transaction_id,
           long long object_store_id,
           const blink::WebData& value,
           const blink::WebVector<blink::WebBlobInfo>&,
           blink::WebIDBKeyView primary_key,
           blink::WebIDBPutMode,
           blink::WebIDBCallbacks*,
           const blink::WebVector<blink::WebIDBIndexKeys>&) override;
  void SetIndexKeys(long long transaction_id,
                    long long object_store_id,
                    blink::WebIDBKeyView primary_key,
                    const blink::WebVector<blink::WebIDBIndexKeys>&) override;
  void SetIndexesReady(long long transaction_id,
                       long long object_store_id,
                       const blink::WebVector<long long>& index_ids) override;
  void OpenCursor(long long transaction_id,
                  long long object_store_id,
                  long long index_id,
                  const blink::WebIDBKeyRange&,
                  blink::WebIDBCursorDirection direction,
                  bool key_only,
                  blink::WebIDBTaskType,
                  blink::WebIDBCallbacks*) override;
  void Count(long long transaction_id,
             long long object_store_id,
             long long index_id,
             const blink::WebIDBKeyRange&,
             blink::WebIDBCallbacks*) override;
  void Delete(long long transaction_id,
              long long object_store_id,
              blink::WebIDBKeyView primary_key,
              blink::WebIDBCallbacks*) override;
  void DeleteRange(long long transaction_id,
                   long long object_store_id,
                   const blink::WebIDBKeyRange&,
                   blink::WebIDBCallbacks*) override;
  void Clear(long long transaction_id,
             long long object_store_id,
             blink::WebIDBCallbacks*) override;
  void CreateIndex(long long transaction_id,
                   long long object_store_id,
                   long long index_id,
                   const blink::WebString& name,
                   const blink::WebIDBKeyPath&,
                   bool unique,
                   bool multi_entry) override;
  void DeleteIndex(long long transaction_id,
                   long long object_store_id,
                   long long index_id) override;
  void RenameIndex(long long transaction_id,
                   long long object_store_id,
                   long long index_id,
                   const blink::WebString& new_name) override;
  void Abort(long long transaction_id) override;
  void Commit(long long transaction_id) override;

 private:
  blink::mojom::IDBCallbacksAssociatedPtrInfo GetCallbacksProxy(
      std::unique_ptr<IndexedDBCallbacksImpl> callbacks);

  FRIEND_TEST_ALL_PREFIXES(WebIDBDatabaseImplTest, ValueSizeTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBDatabaseImplTest, KeyAndValueSizeTest);

  // Maximum size (in bytes) of value/key pair allowed for put requests. Any
  // requests larger than this size will be rejected.
  // Used by unit tests to exercise behavior without allocating huge chunks
  // of memory.
  size_t max_put_value_size_ =
      blink::mojom::kIDBMaxMessageSize - blink::mojom::kIDBMaxMessageOverhead;

  std::set<int32_t> observer_ids_;
  blink::mojom::IDBDatabaseAssociatedPtr database_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
