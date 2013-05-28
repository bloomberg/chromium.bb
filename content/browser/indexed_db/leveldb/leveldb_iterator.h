// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_

#include "content/browser/indexed_db/leveldb/leveldb_slice.h"

namespace content {

class LevelDBIterator {
 public:
  virtual ~LevelDBIterator() {}
  virtual bool IsValid() const = 0;
  virtual void SeekToLast() = 0;
  virtual void Seek(const LevelDBSlice& target) = 0;
  virtual void Next() = 0;
  virtual void Prev() = 0;
  virtual LevelDBSlice Key() const = 0;
  virtual LevelDBSlice Value() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_
