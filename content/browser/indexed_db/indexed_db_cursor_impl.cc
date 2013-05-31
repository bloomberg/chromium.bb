// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_cursor_impl.h"

#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks_wrapper.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_database_impl.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/common/indexed_db/indexed_db_key_range.h"

namespace content {

class IndexedDBCursorImpl::CursorIterationOperation
    : public IndexedDBTransaction::Operation {
 public:
  CursorIterationOperation(scoped_refptr<IndexedDBCursorImpl> cursor,
                           scoped_ptr<IndexedDBKey> key,
                           scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : cursor_(cursor), key_(key.Pass()), callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBCursorImpl> cursor_;
  scoped_ptr<IndexedDBKey> key_;
  scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

class IndexedDBCursorImpl::CursorAdvanceOperation
    : public IndexedDBTransaction::Operation {
 public:
  CursorAdvanceOperation(scoped_refptr<IndexedDBCursorImpl> cursor,
                         unsigned long count,
                         scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : cursor_(cursor), count_(count), callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBCursorImpl> cursor_;
  unsigned long count_;
  scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

class IndexedDBCursorImpl::CursorPrefetchIterationOperation
    : public IndexedDBTransaction::Operation {
 public:
  CursorPrefetchIterationOperation(
      scoped_refptr<IndexedDBCursorImpl> cursor,
      int number_to_fetch,
      scoped_refptr<IndexedDBCallbacksWrapper> callbacks)
      : cursor_(cursor),
        number_to_fetch_(number_to_fetch),
        callbacks_(callbacks) {}
  virtual void Perform(IndexedDBTransaction* transaction) OVERRIDE;

 private:
  scoped_refptr<IndexedDBCursorImpl> cursor_;
  int number_to_fetch_;
  scoped_refptr<IndexedDBCallbacksWrapper> callbacks_;
};

IndexedDBCursorImpl::IndexedDBCursorImpl(
    scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
    indexed_db::CursorType cursor_type,
    IndexedDBDatabase::TaskType task_type,
    IndexedDBTransaction* transaction,
    int64 object_store_id)
    : task_type_(task_type),
      cursor_type_(cursor_type),
      transaction_(transaction),
      object_store_id_(object_store_id),
      cursor_(cursor.Pass()),
      closed_(false) {
  transaction_->RegisterOpenCursor(this);
}

IndexedDBCursorImpl::~IndexedDBCursorImpl() {
  transaction_->UnregisterOpenCursor(this);
}

void IndexedDBCursorImpl::ContinueFunction(
    scoped_ptr<IndexedDBKey> key,
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBCursorImpl::continue");

  transaction_->ScheduleTask(
      task_type_, new CursorIterationOperation(this, key.Pass(), callbacks));
}

void IndexedDBCursorImpl::Advance(
    unsigned long count,
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBCursorImpl::advance");

  transaction_->ScheduleTask(
      new CursorAdvanceOperation(this, count, callbacks));
}

void IndexedDBCursorImpl::CursorAdvanceOperation::Perform(
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("CursorAdvanceOperation");
  if (!cursor_->cursor_ || !cursor_->cursor_->Advance(count_)) {
    cursor_->cursor_.reset();
    callbacks_->OnSuccess(static_cast<std::vector<char>*>(NULL));
    return;
  }

  callbacks_->OnSuccess(
      cursor_->key(), cursor_->primary_key(), cursor_->Value());
}

void IndexedDBCursorImpl::CursorIterationOperation::Perform(
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("CursorIterationOperation");
  if (!cursor_->cursor_ || !cursor_->cursor_->ContinueFunction(key_.get())) {
    cursor_->cursor_.reset();
    callbacks_->OnSuccess(static_cast<std::vector<char>*>(NULL));
    return;
  }

  callbacks_->OnSuccess(
      cursor_->key(), cursor_->primary_key(), cursor_->Value());
}

void IndexedDBCursorImpl::DeleteFunction(
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBCursorImpl::delete");
  DCHECK_NE(transaction_->mode(), indexed_db::TRANSACTION_READ_ONLY);
  scoped_ptr<IndexedDBKeyRange> key_range =
      make_scoped_ptr(new IndexedDBKeyRange(cursor_->primary_key()));
  transaction_->database()->DeleteRange(
      transaction_->id(), object_store_id_, key_range.Pass(), callbacks);
}

void IndexedDBCursorImpl::PrefetchContinue(
    int number_to_fetch,
    scoped_refptr<IndexedDBCallbacksWrapper> callbacks) {
  IDB_TRACE("IndexedDBCursorImpl::prefetch_continue");

  transaction_->ScheduleTask(
      task_type_,
      new CursorPrefetchIterationOperation(this, number_to_fetch, callbacks));
}

void IndexedDBCursorImpl::CursorPrefetchIterationOperation::Perform(
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("CursorPrefetchIterationOperation");

  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBKey> found_primary_keys;
  std::vector<std::vector<char> > found_values;

  if (cursor_->cursor_)
    cursor_->saved_cursor_.reset(cursor_->cursor_->Clone());
  const size_t max_size_estimate = 10 * 1024 * 1024;
  size_t size_estimate = 0;

  for (int i = 0; i < number_to_fetch_; ++i) {
    if (!cursor_->cursor_ || !cursor_->cursor_->ContinueFunction(0)) {
      cursor_->cursor_.reset();
      break;
    }

    found_keys.push_back(cursor_->cursor_->key());
    found_primary_keys.push_back(cursor_->cursor_->primary_key());

    switch (cursor_->cursor_type_) {
      case indexed_db::CURSOR_KEY_ONLY:
        found_values.push_back(std::vector<char>());
        break;
      case indexed_db::CURSOR_KEY_AND_VALUE: {
        std::vector<char> value;
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
    callbacks_->OnSuccess(static_cast<std::vector<char>*>(NULL));
    return;
  }

  callbacks_->OnSuccessWithPrefetch(
      found_keys, found_primary_keys, found_values);
}

void IndexedDBCursorImpl::PrefetchReset(int used_prefetches, int) {
  IDB_TRACE("IndexedDBCursorImpl::prefetch_reset");
  cursor_.swap(saved_cursor_);
  saved_cursor_.reset();

  if (closed_)
    return;
  if (cursor_) {
    for (int i = 0; i < used_prefetches; ++i) {
      bool ok = cursor_->ContinueFunction();
      DCHECK(ok);
    }
  }
}

void IndexedDBCursorImpl::Close() {
  IDB_TRACE("IndexedDBCursorImpl::close");
  closed_ = true;
  cursor_.reset();
  saved_cursor_.reset();
}

}  // namespace content
