// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_CALLBACKS_IMPL_H_
#define CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_CALLBACKS_IMPL_H_

#include "content/common/indexed_db/indexed_db.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

using blink::IndexedDBKey;

namespace blink {
class WebIDBCallbacks;
class WebIDBValue;
}

namespace content {

class WebIDBCursorImpl;

// Implements the child-process end of the pipe used to deliver callbacks. It
// is owned by the IO thread. |callback_runner_| is used to post tasks back to
// the thread which owns the blink::WebIDBCallbacks.
class IndexedDBCallbacksImpl : public indexed_db::mojom::Callbacks {
 public:
  enum : int64_t { kNoTransaction = -1 };

  static blink::WebIDBValue ConvertValue(
      const blink::mojom::IDBValuePtr& value);

  IndexedDBCallbacksImpl(std::unique_ptr<blink::WebIDBCallbacks> callbacks,
                         int64_t transaction_id,
                         const base::WeakPtr<WebIDBCursorImpl>& cursor);
  ~IndexedDBCallbacksImpl() override;

  // indexed_db::mojom::Callbacks implementation:
  void Error(int32_t code, const base::string16& message) override;
  void SuccessStringList(const std::vector<base::string16>& value) override;
  void Blocked(int64_t existing_version) override;
  void UpgradeNeeded(
      indexed_db::mojom::DatabaseAssociatedPtrInfo database_info,
      int64_t old_version,
      blink::WebIDBDataLoss data_loss,
      const std::string& data_loss_message,
      const content::IndexedDBDatabaseMetadata& metadata) override;
  void SuccessDatabase(
      indexed_db::mojom::DatabaseAssociatedPtrInfo database_info,
      const content::IndexedDBDatabaseMetadata& metadata) override;
  void SuccessCursor(indexed_db::mojom::CursorAssociatedPtrInfo cursor,
                     const IndexedDBKey& key,
                     const IndexedDBKey& primary_key,
                     blink::mojom::IDBValuePtr value) override;
  void SuccessValue(indexed_db::mojom::ReturnValuePtr value) override;
  void SuccessCursorContinue(const IndexedDBKey& key,
                             const IndexedDBKey& primary_key,
                             blink::mojom::IDBValuePtr value) override;
  void SuccessCursorPrefetch(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primary_keys,
      std::vector<blink::mojom::IDBValuePtr> values) override;
  void SuccessArray(
      std::vector<indexed_db::mojom::ReturnValuePtr> values) override;
  void SuccessKey(const IndexedDBKey& key) override;
  void SuccessInteger(int64_t value) override;
  void Success() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> callback_runner_;
  std::unique_ptr<blink::WebIDBCallbacks> callbacks_;
  base::WeakPtr<WebIDBCursorImpl> cursor_;
  int64_t transaction_id_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBCallbacksImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INDEXED_DB_INDEXED_DB_CALLBACKS_IMPL_H_
