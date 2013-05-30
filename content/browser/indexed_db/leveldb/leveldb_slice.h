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
      : begin_(&*v.begin()), end_(&*v.rbegin() + 1) {
    DCHECK_GT(v.size(), static_cast<size_t>(0));
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
