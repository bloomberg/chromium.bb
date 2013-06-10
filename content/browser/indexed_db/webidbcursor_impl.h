// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_

#include "base/memory/ref_counted.h"

namespace content {
class IndexedDBCursor;
class IndexedDBCallbacksBase;
class IndexedDBKey;

class WebIDBCursorImpl {
 public:
  explicit WebIDBCursorImpl(scoped_refptr<IndexedDBCursor> cursor);
  virtual ~WebIDBCursorImpl();

  virtual void advance(unsigned long, IndexedDBCallbacksBase* callbacks);
  virtual void continueFunction(const IndexedDBKey& key,
                                IndexedDBCallbacksBase* callbacks);
  virtual void prefetchContinue(int number_to_fetch,
                                IndexedDBCallbacksBase* callbacks);
  virtual void prefetchReset(int used_prefetches, int unused_prefetches);

 private:
  scoped_refptr<IndexedDBCursor> idb_cursor_backend_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_
