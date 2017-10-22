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
#include "content/common/indexed_db/indexed_db.mojom.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCursor.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {
class WebBlobInfo;
class WebIDBCallbacks;
class WebString;
}

namespace content {

class CONTENT_EXPORT WebIDBDatabaseImpl : public blink::WebIDBDatabase {
 public:
  WebIDBDatabaseImpl(indexed_db::mojom::DatabaseAssociatedPtrInfo database,
                     scoped_refptr<base::SingleThreadTaskRunner> io_runner);
  ~WebIDBDatabaseImpl() override;

  // blink::WebIDBDatabase
  void CreateObjectStore(long long transaction_id,
                         long long objectstore_id,
                         const blink::WebString& name,
                         const blink::WebIDBKeyPath& key_path,
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

  void Get(long long transactionId,
           long long objectStoreId,
           long long indexId,
           const blink::WebIDBKeyRange&,
           bool keyOnly,
           blink::WebIDBCallbacks*) override;
  void GetAll(long long transactionId,
              long long objectStoreId,
              long long indexId,
              const blink::WebIDBKeyRange&,
              long long maxCount,
              bool keyOnly,
              blink::WebIDBCallbacks*) override;
  void Put(long long transactionId,
           long long objectStoreId,
           const blink::WebData& value,
           const blink::WebVector<blink::WebBlobInfo>& webBlobInfo,
           const blink::WebIDBKey&,
           blink::WebIDBPutMode,
           blink::WebIDBCallbacks*,
           const blink::WebVector<long long>& indexIds,
           const blink::WebVector<WebIndexKeys>&) override;
  void SetIndexKeys(long long transactionId,
                    long long objectStoreId,
                    const blink::WebIDBKey&,
                    const blink::WebVector<long long>& indexIds,
                    const blink::WebVector<WebIndexKeys>&) override;
  void SetIndexesReady(long long transactionId,
                       long long objectStoreId,
                       const blink::WebVector<long long>& indexIds) override;
  void OpenCursor(long long transactionId,
                  long long objectStoreId,
                  long long indexId,
                  const blink::WebIDBKeyRange&,
                  blink::WebIDBCursorDirection direction,
                  bool keyOnly,
                  blink::WebIDBTaskType,
                  blink::WebIDBCallbacks*) override;
  void Count(long long transactionId,
             long long objectStoreId,
             long long indexId,
             const blink::WebIDBKeyRange&,
             blink::WebIDBCallbacks*) override;
  void DeleteRange(long long transactionId,
                   long long objectStoreId,
                   const blink::WebIDBKeyRange&,
                   blink::WebIDBCallbacks*) override;
  void Clear(long long transactionId,
             long long objectStoreId,
             blink::WebIDBCallbacks*) override;
  void CreateIndex(long long transactionId,
                   long long objectStoreId,
                   long long indexId,
                   const blink::WebString& name,
                   const blink::WebIDBKeyPath&,
                   bool unique,
                   bool multiEntry) override;
  void DeleteIndex(long long transactionId,
                   long long objectStoreId,
                   long long indexId) override;
  void RenameIndex(long long transactionId,
                   long long objectStoreId,
                   long long indexId,
                   const blink::WebString& new_name) override;
  void Abort(long long transaction_id) override;
  void Commit(long long transaction_id) override;
  void AckReceivedBlobs(
      const blink::WebVector<blink::WebString>& uuids) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebIDBDatabaseImplTest, ValueSizeTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBDatabaseImplTest, KeyAndValueSizeTest);

  class IOThreadHelper;

  // Maximum size (in bytes) of value/key pair allowed for put requests. Any
  // requests larger than this size will be rejected.
  // Used by unit tests to exercise behavior without allocating huge chunks
  // of memory.
  size_t max_put_value_size_ = kMaxIDBMessageSizeInBytes;

  IOThreadHelper* helper_;
  std::set<int32_t> observer_ids_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
