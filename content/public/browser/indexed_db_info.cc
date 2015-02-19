// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/indexed_db_info.h"

namespace content {

IndexedDBInfo::IndexedDBInfo(const GURL& origin,
                             int64 size,
                             const base::Time& last_modified,
                             size_t connection_count)
    : origin_(origin),
      size_(size),
      last_modified_(last_modified),
      connection_count_(connection_count) {}

IndexedDBInfo::IndexedDBInfo(const IndexedDBInfo& other) = default;
IndexedDBInfo::~IndexedDBInfo() = default;
IndexedDBInfo& IndexedDBInfo::operator=(const IndexedDBInfo& other) = default;

}  // namespace content
