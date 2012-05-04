// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key_range.h"

#include "base/logging.h"

namespace content {

using WebKit::WebIDBKeyRange;
using WebKit::WebIDBKey;

IndexedDBKeyRange::IndexedDBKeyRange()
    : lower_open_(false),
      upper_open_(false) {
  lower_.SetNull();
  upper_.SetNull();
}

IndexedDBKeyRange::IndexedDBKeyRange(const WebIDBKeyRange& key_range) {
  lower_.Set(key_range.lower());
  upper_.Set(key_range.upper());
  lower_open_ = key_range.lowerOpen();
  upper_open_ = key_range.upperOpen();
}

IndexedDBKeyRange::~IndexedDBKeyRange() {
}


void IndexedDBKeyRange::Set(const IndexedDBKey& lower,
                            const IndexedDBKey& upper,
                            bool lower_open, bool upper_open) {
  lower_.Set(lower);
  upper_.Set(upper);
  lower_open_ = lower_open;
  upper_open_ = upper_open;
}

IndexedDBKeyRange::operator WebIDBKeyRange() const {
  return WebIDBKeyRange(lower_, upper_, lower_open_, upper_open_);
}

}  // namespace content
