// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/common/indexed_db/indexed_db_key_range.h"

namespace content {

class IndexedDBDatabaseImpl;
class IndexedDBTransaction;

class IndexedDBCursorImpl : public IndexedDBCursor {
 public:
  static scoped_refptr<IndexedDBCursorImpl> Create(
      scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
      indexed_db::CursorType cursor_type,
      IndexedDBTransaction* transaction,
      int64 object_store_id) {
    return make_scoped_refptr(
        new IndexedDBCursorImpl(cursor.Pass(),
                                cursor_type,
                                IndexedDBDatabase::NORMAL_TASK,
                                transaction,
                                object_store_id));
  }
  static scoped_refptr<IndexedDBCursorImpl> Create(
      scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
      indexed_db::CursorType cursor_type,
      IndexedDBDatabase::TaskType task_type,
      IndexedDBTransaction* transaction,
      int64 object_store_id) {
    return make_scoped_refptr(new IndexedDBCursorImpl(
        cursor.Pass(), cursor_type, task_type, transaction, object_store_id));
  }

  // IndexedDBCursor
  virtual void Advance(unsigned long,
                       scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      OVERRIDE;
  virtual void ContinueFunction(
      scoped_ptr<IndexedDBKey> key,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks) OVERRIDE;
  virtual void DeleteFunction(
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks) OVERRIDE;
  virtual void PrefetchContinue(
      int number_to_fetch,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks) OVERRIDE;
  virtual void PrefetchReset(int used_prefetches, int unused_prefetches)
      OVERRIDE;
  virtual void PostSuccessHandlerCallback() OVERRIDE {}

  const IndexedDBKey& key() const { return cursor_->key(); }
  const IndexedDBKey& primary_key() const { return cursor_->primary_key(); }
  std::vector<char>* Value() const {
    return (cursor_type_ == indexed_db::CURSOR_KEY_ONLY) ? NULL
                                                         : cursor_->Value();
  }
  void Close();

 private:
  IndexedDBCursorImpl(scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
                      indexed_db::CursorType cursor_type,
                      IndexedDBDatabase::TaskType task_type,
                      IndexedDBTransaction* transaction,
                      int64 object_store_id);
  virtual ~IndexedDBCursorImpl();

  class CursorIterationOperation;
  class CursorAdvanceOperation;
  class CursorPrefetchIterationOperation;

  IndexedDBDatabase::TaskType task_type_;
  indexed_db::CursorType cursor_type_;
  const scoped_refptr<IndexedDBTransaction> transaction_;
  const int64 object_store_id_;

  // Must be destroyed before transaction_.
  scoped_ptr<IndexedDBBackingStore::Cursor> cursor_;
  // Must be destroyed before transaction_.
  scoped_ptr<IndexedDBBackingStore::Cursor> saved_cursor_;

  bool closed_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_IMPL_H_
