// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_CURSOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_CURSOR_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/common/indexed_db/indexed_db.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class IndexedDBCursor;
class IndexedDBDispatcherHost;
class IndexedDBKey;

class CursorImpl : public ::indexed_db::mojom::Cursor {
 public:
  CursorImpl(scoped_refptr<IndexedDBCursor> cursor,
             const url::Origin& origin,
             scoped_refptr<IndexedDBDispatcherHost> dispatcher_host);
  ~CursorImpl() override;

  // ::indexed_db::mojom::Cursor implementation
  void Advance(
      uint32_t count,
      ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks) override;
  void Continue(
      const IndexedDBKey& key,
      const IndexedDBKey& primary_key,
      ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks) override;
  void Prefetch(
      int32_t count,
      ::indexed_db::mojom::CallbacksAssociatedPtrInfo callbacks) override;
  void PrefetchReset(
      int32_t used_prefetches,
      int32_t unused_prefetches,
      const std::vector<std::string>& unused_blob_uuids) override;

 private:
  class IDBThreadHelper;

  IDBThreadHelper* helper_;
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  const url::Origin origin_;
  scoped_refptr<base::SingleThreadTaskRunner> idb_runner_;

  DISALLOW_COPY_AND_ASSIGN(CursorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_CURSOR_IMPL_H_
