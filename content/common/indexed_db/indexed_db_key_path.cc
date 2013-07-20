// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key_path.h"

#include "base/logging.h"

namespace content {

using WebKit::WebIDBKeyPathTypeArray;
using WebKit::WebIDBKeyPathTypeNull;
using WebKit::WebIDBKeyPathTypeString;

IndexedDBKeyPath::IndexedDBKeyPath() : type_(WebIDBKeyPathTypeNull) {}

IndexedDBKeyPath::IndexedDBKeyPath(const string16& string)
    : type_(WebIDBKeyPathTypeString), string_(string) {}

IndexedDBKeyPath::IndexedDBKeyPath(const std::vector<string16>& array)
    : type_(WebIDBKeyPathTypeArray), array_(array) {}

IndexedDBKeyPath::~IndexedDBKeyPath() {}

const std::vector<string16>& IndexedDBKeyPath::array() const {
  DCHECK(type_ == WebKit::WebIDBKeyPathTypeArray);
  return array_;
}

const string16& IndexedDBKeyPath::string() const {
  DCHECK(type_ == WebKit::WebIDBKeyPathTypeString);
  return string_;
}

bool IndexedDBKeyPath::operator==(const IndexedDBKeyPath& other) const {
  if (type_ != other.type_)
    return false;

  switch (type_) {
    case WebIDBKeyPathTypeNull:
      return true;
    case WebIDBKeyPathTypeString:
      return string_ == other.string_;
    case WebIDBKeyPathTypeArray:
      return array_ == other.array_;
  }
  NOTREACHED();
  return false;
}

}  // namespace content
