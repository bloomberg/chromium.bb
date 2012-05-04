// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_RANGE_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_RANGE_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyRange.h"

namespace content {

class CONTENT_EXPORT IndexedDBKeyRange {
 public:
  IndexedDBKeyRange();
  explicit IndexedDBKeyRange(const WebKit::WebIDBKeyRange& key_range);
  ~IndexedDBKeyRange();

  const IndexedDBKey& lower() const { return lower_; }
  const IndexedDBKey& upper() const { return upper_; }
  bool lowerOpen() const { return lower_open_; }
  bool upperOpen() const { return upper_open_; }

  void Set(const IndexedDBKey& lower, const IndexedDBKey& upper,
           bool lower_open, bool upper_open);

  operator WebKit::WebIDBKeyRange() const;

 private:
  IndexedDBKey lower_;
  IndexedDBKey upper_;
  bool lower_open_;
  bool upper_open_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_RANGE_H_
