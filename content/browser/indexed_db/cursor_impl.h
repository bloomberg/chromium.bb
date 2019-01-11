// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_CURSOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_CURSOR_IMPL_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

class IndexedDBCursor;
class IndexedDBDispatcherHost;

// Expected to be constructed, called, and destructed on the IO thread.
class CursorImpl : public blink::mojom::IDBCursor {
 public:
  CursorImpl(std::unique_ptr<IndexedDBCursor> cursor,
             const url::Origin& origin,
             IndexedDBDispatcherHost* dispatcher_host,
             scoped_refptr<base::SequencedTaskRunner> idb_runner);
  ~CursorImpl() override;

  // blink::mojom::IDBCursor implementation
  void Advance(uint32_t count,
               blink::mojom::IDBCursor::AdvanceCallback callback) override;
  void CursorContinue(
      const blink::IndexedDBKey& key,
      const blink::IndexedDBKey& primary_key,
      blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks) override;
  void Prefetch(int32_t count,
                blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks) override;
  void PrefetchReset(int32_t used_prefetches,
                     int32_t unused_prefetches) override;

 private:
  class IDBSequenceHelper;

  IDBSequenceHelper* helper_;
  // This raw pointer is safe because all CursorImpl instances are owned by an
  // IndexedDBDispatcherHost.
  IndexedDBDispatcherHost* dispatcher_host_;
  const url::Origin origin_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;

  DISALLOW_COPY_AND_ASSIGN(CursorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_CURSOR_IMPL_H_
