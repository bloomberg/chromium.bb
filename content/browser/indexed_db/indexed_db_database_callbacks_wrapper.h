// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_WRAPPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_WRAPPER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/common/content_export.h"

namespace content {
class IndexedDBDatabaseCallbacks;

class CONTENT_EXPORT IndexedDBDatabaseCallbacksWrapper
    : public base::RefCounted<IndexedDBDatabaseCallbacksWrapper> {
 public:
  static scoped_refptr<IndexedDBDatabaseCallbacksWrapper> Create(
      IndexedDBDatabaseCallbacks* callbacks) {
    return make_scoped_refptr(new IndexedDBDatabaseCallbacksWrapper(callbacks));
  }

  virtual void OnForcedClose();
  virtual void OnVersionChange(int64 old_version, int64 new_version);

  virtual void OnAbort(int64 transaction_id,
                       const IndexedDBDatabaseError& error);
  virtual void OnComplete(int64 transaction_id);

 protected:
  explicit IndexedDBDatabaseCallbacksWrapper(
      IndexedDBDatabaseCallbacks* callbacks);
  virtual ~IndexedDBDatabaseCallbacksWrapper();

 private:
  friend class base::RefCounted<IndexedDBDatabaseCallbacksWrapper>;

  scoped_ptr<IndexedDBDatabaseCallbacks> callbacks_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_WRAPPER_H_
