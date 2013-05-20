// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key_range.h"

#include "base/logging.h"

namespace content {

using WebKit::WebIDBKeyRange;
using WebKit::WebIDBKey;

IndexedDBKeyRange::IndexedDBKeyRange()
    : lower_(WebIDBKey::NullType),
      upper_(WebIDBKey::NullType),
      lower_open_(false),
      upper_open_(false) {}

IndexedDBKeyRange::IndexedDBKeyRange(const WebIDBKeyRange& key_range)
    : lower_(key_range.lower()),
      upper_(key_range.upper()),
      lower_open_(key_range.lowerOpen()),
      upper_open_(key_range.upperOpen()) {}

IndexedDBKeyRange::IndexedDBKeyRange(const IndexedDBKey& lower,
                                     const IndexedDBKey& upper,
                                     bool lower_open,
                                     bool upper_open)
    : lower_(lower),
      upper_(upper),
      lower_open_(lower_open),
      upper_open_(upper_open) {}

IndexedDBKeyRange::IndexedDBKeyRange(const IndexedDBKey& key)
    : lower_(key), upper_(key), lower_open_(false), upper_open_(false) {}

IndexedDBKeyRange::~IndexedDBKeyRange() {}

bool IndexedDBKeyRange::IsOnlyKey() const {
  if (lower_open_ || upper_open_)
    return false;

  return lower_.IsEqual(upper_);
}

IndexedDBKeyRange::operator WebIDBKeyRange() const {
  return WebIDBKeyRange(lower_, upper_, lower_open_, upper_open_);
}

}  // namespace content
