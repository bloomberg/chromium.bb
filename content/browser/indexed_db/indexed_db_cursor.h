// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/common/indexed_db/indexed_db_key_range.h"

namespace content {

class IndexedDBDatabase;
class IndexedDBTransaction;

class CONTENT_EXPORT IndexedDBCursor
    : NON_EXPORTED_BASE(public base::RefCounted<IndexedDBCursor>) {
 public:
  static scoped_refptr<IndexedDBCursor> Create(
      scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
      indexed_db::CursorType cursor_type,
      IndexedDBTransaction* transaction) {
    return make_scoped_refptr(
        new IndexedDBCursor(cursor.Pass(),
                            cursor_type,
                            IndexedDBDatabase::NORMAL_TASK,
                            transaction));
  }
  static scoped_refptr<IndexedDBCursor> Create(
      scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
      indexed_db::CursorType cursor_type,
      IndexedDBDatabase::TaskType task_type,
      IndexedDBTransaction* transaction) {
    return make_scoped_refptr(new IndexedDBCursor(
        cursor.Pass(), cursor_type, task_type, transaction));
  }

  // IndexedDBCursor
  void Advance(uint32 count,
               scoped_refptr<IndexedDBCallbacksWrapper> callbacks);
  void ContinueFunction(scoped_ptr<IndexedDBKey> key,
                        scoped_refptr<IndexedDBCallbacksWrapper> callbacks);
  void PrefetchContinue(int number_to_fetch,
                        scoped_refptr<IndexedDBCallbacksWrapper> callbacks);
  void PrefetchReset(int used_prefetches, int unused_prefetches);

  const IndexedDBKey& key() const { return cursor_->key(); }
  const IndexedDBKey& primary_key() const { return cursor_->primary_key(); }
  std::vector<char>* Value() const {
    return (cursor_type_ == indexed_db::CURSOR_KEY_ONLY) ? NULL
                                                         : cursor_->Value();
  }
  void Close();

 private:
  friend class base::RefCounted<IndexedDBCursor>;

  IndexedDBCursor(scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
                  indexed_db::CursorType cursor_type,
                  IndexedDBDatabase::TaskType task_type,
                  IndexedDBTransaction* transaction);
  ~IndexedDBCursor();

  class CursorIterationOperation;
  class CursorAdvanceOperation;
  class CursorPrefetchIterationOperation;

  IndexedDBDatabase::TaskType task_type_;
  indexed_db::CursorType cursor_type_;
  const scoped_refptr<IndexedDBTransaction> transaction_;

  // Must be destroyed before transaction_.
  scoped_ptr<IndexedDBBackingStore::Cursor> cursor_;
  // Must be destroyed before transaction_.
  scoped_ptr<IndexedDBBackingStore::Cursor> saved_cursor_;

  bool closed_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
