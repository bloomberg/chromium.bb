// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebIDBCursor.h"

namespace content {
class IndexedDBCursor;

class WebIDBCursorImpl : public WebKit::WebIDBCursor {
 public:
  explicit WebIDBCursorImpl(scoped_refptr<IndexedDBCursor> cursor);
  virtual ~WebIDBCursorImpl();

  virtual void advance(unsigned long, WebKit::WebIDBCallbacks* callbacks);
  virtual void continueFunction(const WebKit::WebIDBKey& key,
                                WebKit::WebIDBCallbacks* callbacks);
  virtual void deleteFunction(WebKit::WebIDBCallbacks* callbacks);
  virtual void prefetchContinue(int number_to_fetch,
                                WebKit::WebIDBCallbacks* callbacks);
  virtual void prefetchReset(int used_prefetches, int unused_prefetches);

 private:
  scoped_refptr<IndexedDBCursor> idb_cursor_backend_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_
