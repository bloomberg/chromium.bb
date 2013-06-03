// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_

#include "base/memory/ref_counted.h"
#include "content/common/indexed_db/indexed_db_key.h"

namespace content {

class IndexedDBCallbacksWrapper;

class IndexedDBCursor : public base::RefCounted<IndexedDBCursor> {
 public:
  virtual void Advance(unsigned long count,
                       scoped_refptr<IndexedDBCallbacksWrapper> callbacks) = 0;
  virtual void ContinueFunction(
      scoped_ptr<IndexedDBKey> key,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks) = 0;
  virtual void PrefetchContinue(
      int number_to_fetch,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks) = 0;
  virtual void PrefetchReset(int used_prefetches, int unused_prefetches) = 0;
  virtual void PostSuccessHandlerCallback() = 0;

 protected:
  virtual ~IndexedDBCursor() {}
  friend class base::RefCounted<IndexedDBCursor>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
