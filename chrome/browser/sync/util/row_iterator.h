// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handy type for iterating through query results.
//
// Just define your Traits type with
//
//   RowType
//   Extract(statement*, RowType);
//
// and then pass an sqlite3_stmt into the constructor for begin,
// and use the no-arg constructor for an end iterator.  Ex:
//
//   for (RowIterator<SomeTraits> i(statement), end; i != end; ++i)
//     ...

#ifndef CHROME_BROWSER_SYNC_UTIL_ROW_ITERATOR_H_
#define CHROME_BROWSER_SYNC_UTIL_ROW_ITERATOR_H_

#include "base/logging.h"
#include "third_party/sqlite/preprocessed/sqlite3.h"

template <typename ColumnType, int index = 0>
struct SingleColumnTraits {
  typedef ColumnType RowType;
  inline void Extract(sqlite3_stmt* statement, ColumnType* x) const {
    GetColumn(statement, index, x);
  }
};

template <typename RowTraits>
class RowIterator : public std::iterator<std::input_iterator_tag,
                                         const typename RowTraits::RowType> {
 public:
  typedef typename RowTraits::RowType RowType;
  // Statement must have been prepared, but not yet stepped:
  RowIterator(sqlite3_stmt* statement, RowTraits traits = RowTraits()) {
    kernel_ = new Kernel;
    kernel_->done = false;
    kernel_->refcount = 1;
    kernel_->statement = statement;
    kernel_->row_traits = traits;
    ++(*this);
  }
  RowIterator() : kernel_(NULL) { }  // creates end iterator

  RowIterator(const RowIterator& i)
    : kernel_(NULL) {
    *this = i;
  }

  ~RowIterator() {
    if (kernel_ && 0 == --(kernel_->refcount)) {
      sqlite3_finalize(kernel_->statement);
      delete kernel_;
    }
  }

  RowIterator& operator = (const RowIterator& i) {
    if (kernel_ && (0 == --(kernel_->refcount))) {
      sqlite3_finalize(kernel_->statement);
      delete kernel_;
    }
    kernel_ = i.kernel_;
    if (kernel_)
      kernel_->refcount += 1;
    return *this;
  }

  RowIterator operator ++(int) {
    RowIterator i(*this);
    return ++i;
  }

  RowIterator& operator ++() {
    DCHECK(NULL != kernel_);
    if (SQLITE_ROW == sqlite3_step(kernel_->statement)) {
      kernel_->row_traits.Extract(kernel_->statement, &kernel_->row);
    } else {
      kernel_->done = true;
    }
    return *this;
  }

  const RowType& operator *() const {
    return *(operator -> ());
  }

  const RowType* operator ->() const {
    DCHECK(NULL != kernel_);
    DCHECK(!kernel_->done);
    return &(kernel_->row);
  }

  bool operator == (const RowIterator& i) const {
    if (kernel_ == i.kernel_)
      return true;
    if (NULL == kernel_ && i.kernel_->done)
      return true;
    if (NULL == i.kernel_ && kernel_->done)
      return true;
    return false;
  }

  bool operator != (const RowIterator& i) const {
    return !(*this == i);
  }

 protected:
  struct Kernel {
    int refcount;
    bool done;
    RowType row;
    sqlite3_stmt* statement;
    RowTraits row_traits;
  };

  Kernel* kernel_;
};

#endif  // CHROME_BROWSER_SYNC_UTIL_ROW_ITERATOR_H_
