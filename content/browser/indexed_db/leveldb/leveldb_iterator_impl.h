// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"

namespace content {

class CONTENT_EXPORT LevelDBIteratorImpl : public content::LevelDBIterator {
 public:
  virtual ~LevelDBIteratorImpl();
  virtual bool IsValid() const OVERRIDE;
  virtual leveldb::Status SeekToLast() OVERRIDE;
  virtual leveldb::Status Seek(const base::StringPiece& target) OVERRIDE;
  virtual leveldb::Status Next() OVERRIDE;
  virtual leveldb::Status Prev() OVERRIDE;
  virtual base::StringPiece Key() const OVERRIDE;
  virtual base::StringPiece Value() const OVERRIDE;

 protected:
  explicit LevelDBIteratorImpl(scoped_ptr<leveldb::Iterator> iterator);

 private:
  void CheckStatus();

  friend class IndexedDBClassFactory;
  friend class MockBrowserTestIndexedDBClassFactory;

  scoped_ptr<leveldb::Iterator> iterator_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBIteratorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_IMPL_H_
