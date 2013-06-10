// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/webidbcursor_impl.h"

#include "content/browser/indexed_db/indexed_db_callbacks_wrapper.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/common/indexed_db/indexed_db_key.h"

namespace content {

WebIDBCursorImpl::WebIDBCursorImpl(
    scoped_refptr<IndexedDBCursor> idb_cursor_backend)
    : idb_cursor_backend_(idb_cursor_backend) {}

WebIDBCursorImpl::~WebIDBCursorImpl() {}

void WebIDBCursorImpl::advance(unsigned long count,
                               IndexedDBCallbacksBase* callbacks) {
  idb_cursor_backend_->Advance(count,
                               IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBCursorImpl::continueFunction(const IndexedDBKey& key,
                                        IndexedDBCallbacksBase* callbacks) {
  idb_cursor_backend_->ContinueFunction(
      make_scoped_ptr(new IndexedDBKey(key)),
      IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBCursorImpl::prefetchContinue(int number_to_fetch,
                                        IndexedDBCallbacksBase* callbacks) {
  idb_cursor_backend_->PrefetchContinue(
      number_to_fetch, IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBCursorImpl::prefetchReset(int used_prefetches,
                                     int unused_prefetches) {
  idb_cursor_backend_->PrefetchReset(used_prefetches, unused_prefetches);
}

}  // namespace content
