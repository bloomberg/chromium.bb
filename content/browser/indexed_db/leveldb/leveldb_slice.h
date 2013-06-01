// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_SLICE_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_SLICE_H_

#include <vector>

#include "base/logging.h"

namespace content {

class LevelDBSlice {
 public:
  LevelDBSlice(const char* begin, const char* end) : begin_(begin), end_(end) {
    DCHECK_GE(end_, begin_);
  }

  explicit LevelDBSlice(const std::vector<char>& v)
      : begin_(v.empty() ? NULL : &*v.begin()), end_(begin_ + v.size()) {
    DCHECK_GE(end_, begin_);
  }

  ~LevelDBSlice() {}

  const char* begin() const { return begin_; }
  const char* end() const { return end_; }

 private:
  const char* begin_;
  const char* end_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_SLICE_H_
