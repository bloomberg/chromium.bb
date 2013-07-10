// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_

#include "base/strings/string_piece.h"

namespace content {

class LevelDBIterator {
 public:
  virtual ~LevelDBIterator() {}
  virtual bool IsValid() const = 0;
  virtual void SeekToLast() = 0;
  virtual void Seek(const base::StringPiece& target) = 0;
  virtual void Next() = 0;
  virtual void Prev() = 0;
  virtual base::StringPiece Key() const = 0;
  virtual base::StringPiece Value() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ITERATOR_H_
