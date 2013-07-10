// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_cursor.h"

#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"

namespace content {

class IndexedDBCursor::CursorIterationOperation
    : public IndexedDBTransaction::Operation {
 public:
  CursorIterationOperation(scoped_refptr<IndexedDBCursor> cursor,
                           scoped_ptr<IndexedDBKey> key,
                           scoped_refptr<IndexedDBCallbacks> callbacks)
      : cursor_(cursor), key_(key.Pass()), callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBCursor> cursor_;
  scoped_ptr<IndexedDBKey> key_;
  scoped_refptr<IndexedDBCallbacks> callbacks_;
};

class IndexedDBCursor::CursorAdvanceOperation
    : public IndexedDBTransaction::Operation {
 public:
  CursorAdvanceOperation(scoped_refptr<IndexedDBCursor> cursor,
                         uint32 count,
                         scoped_refptr<IndexedDBCallbacks> callbacks)
      : cursor_(cursor), count_(count), callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBCursor> cursor_;
  uint32 count_;
  scoped_refptr<IndexedDBCallbacks> callbacks_;
};

class IndexedDBCursor::CursorPrefetchIterationOperation
    : public IndexedDBTransaction::Operation {
 public:
  CursorPrefetchIterationOperation(scoped_refptr<IndexedDBCursor> cursor,
                                   int number_to_fetch,
                                   scoped_refptr<IndexedDBCallbacks> callbacks)
      : cursor_(cursor),
        number_to_fetch_(number_to_fetch),
        callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBCursor> cursor_;
  int number_to_fetch_;
  scoped_refptr<IndexedDBCallbacks> callbacks_;
};

IndexedDBCursor::IndexedDBCursor(
    scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
    indexed_db::CursorType cursor_type,
    IndexedDBDatabase::TaskType task_type,
    IndexedDBTransaction* transaction)
    : task_type_(task_type),
      cursor_type_(cursor_type),
      transaction_(transaction),
      cursor_(cursor.Pass()),
      closed_(false) {
  transaction_->RegisterOpenCursor(this);
}

IndexedDBCursor::~IndexedDBCursor() {
  transaction_->UnregisterOpenCursor(this);
}

void IndexedDBCursor::Continue(scoped_ptr<IndexedDBKey> key,
                               scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::Continue");

  transaction_->ScheduleTask(
      task_type_, new CursorIterationOperation(this, key.Pass(), callbacks));
}

void IndexedDBCursor::Advance(uint32 count,
                              scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::Advance");

  transaction_->ScheduleTask(
      new CursorAdvanceOperation(this, count, callbacks));
}

void IndexedDBCursor::CursorAdvanceOperation::Perform(
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("CursorAdvanceOperation");
  if (!cursor_->cursor_ || !cursor_->cursor_->Advance(count_)) {
    cursor_->cursor_.reset();
    callbacks_->OnSuccess(static_cast<std::string*>(NULL));
    return;
  }

  callbacks_->OnSuccess(
      cursor_->key(), cursor_->primary_key(), cursor_->Value());
}

void IndexedDBCursor::CursorIterationOperation::Perform(
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("CursorIterationOperation");
  if (!cursor_->cursor_ ||
      !cursor_->cursor_->Continue(key_.get(),
                                  IndexedDBBackingStore::Cursor::SEEK)) {
    cursor_->cursor_.reset();
    callbacks_->OnSuccess(static_cast<std::string*>(NULL));
    return;
  }

  callbacks_->OnSuccess(
      cursor_->key(), cursor_->primary_key(), cursor_->Value());
}

void IndexedDBCursor::PrefetchContinue(
    int number_to_fetch,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::PrefetchContinue");

  transaction_->ScheduleTask(
      task_type_,
      new CursorPrefetchIterationOperation(this, number_to_fetch, callbacks));
}

void IndexedDBCursor::CursorPrefetchIterationOperation::Perform(
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("CursorPrefetchIterationOperation");

  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBKey> found_primary_keys;
  std::vector<std::string> found_values;

  if (cursor_->cursor_)
    cursor_->saved_cursor_.reset(cursor_->cursor_->Clone());
  const size_t max_size_estimate = 10 * 1024 * 1024;
  size_t size_estimate = 0;

  for (int i = 0; i < number_to_fetch_; ++i) {
    if (!cursor_->cursor_ || !cursor_->cursor_->Continue()) {
      cursor_->cursor_.reset();
      break;
    }

    found_keys.push_back(cursor_->cursor_->key());
    found_primary_keys.push_back(cursor_->cursor_->primary_key());

    switch (cursor_->cursor_type_) {
      case indexed_db::CURSOR_KEY_ONLY:
        found_values.push_back(std::string());
        break;
      case indexed_db::CURSOR_KEY_AND_VALUE: {
        std::string value;
        value.swap(*cursor_->cursor_->Value());
        size_estimate += value.size();
        found_values.push_back(value);
        break;
      }
      default:
        NOTREACHED();
    }
    size_estimate += cursor_->cursor_->key().size_estimate();
    size_estimate += cursor_->cursor_->primary_key().size_estimate();

    if (size_estimate > max_size_estimate)
      break;
  }

  if (!found_keys.size()) {
    callbacks_->OnSuccess(static_cast<std::string*>(NULL));
    return;
  }

  callbacks_->OnSuccessWithPrefetch(
      found_keys, found_primary_keys, found_values);
}

void IndexedDBCursor::PrefetchReset(int used_prefetches, int) {
  IDB_TRACE("IndexedDBCursor::PrefetchReset");
  cursor_.swap(saved_cursor_);
  saved_cursor_.reset();

  if (closed_)
    return;
  if (cursor_) {
    for (int i = 0; i < used_prefetches; ++i) {
      bool ok = cursor_->Continue();
      DCHECK(ok);
    }
  }
}

void IndexedDBCursor::Close() {
  IDB_TRACE("IndexedDBCursor::Close");
  closed_ = true;
  cursor_.reset();
  saved_cursor_.reset();
}

}  // namespace content
