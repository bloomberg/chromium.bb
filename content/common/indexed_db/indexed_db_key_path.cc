// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key_path.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

using WebKit::WebIDBKeyPath;
using WebKit::WebIDBKeyPathTypeArray;
using WebKit::WebIDBKeyPathTypeNull;
using WebKit::WebIDBKeyPathTypeString;
using WebKit::WebString;
using WebKit::WebVector;

namespace {
static std::vector<string16> CopyArray(const WebVector<WebString>& array) {
  std::vector<string16> copy(array.size());
  for (size_t i = 0; i < array.size(); ++i)
    copy[i] = array[i];
  return copy;
}
}  // namespace

IndexedDBKeyPath::IndexedDBKeyPath() : type_(WebIDBKeyPathTypeNull) {}

IndexedDBKeyPath::IndexedDBKeyPath(const string16& string)
    : type_(WebIDBKeyPathTypeString), string_(string) {}

IndexedDBKeyPath::IndexedDBKeyPath(const std::vector<string16>& array)
    : type_(WebIDBKeyPathTypeArray), array_(array) {}

IndexedDBKeyPath::IndexedDBKeyPath(const WebIDBKeyPath& other)
    : type_(other.keyPathType()),
      string_(type_ == WebIDBKeyPathTypeString
              ? static_cast<string16>(other.string()) : string16()),
      array_(type_ == WebIDBKeyPathTypeArray
             ? CopyArray(other.array()) : std::vector<string16>()) {}

IndexedDBKeyPath::~IndexedDBKeyPath() {}

bool IndexedDBKeyPath::IsValid() const {
  WebIDBKeyPath key_path = *this;
  return key_path.isValid();
}

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

IndexedDBKeyPath::operator WebIDBKeyPath() const {
  switch (type_) {
    case WebIDBKeyPathTypeArray:
      return WebIDBKeyPath::create(array_);
    case WebIDBKeyPathTypeString:
      return WebIDBKeyPath::create(WebString(string_));
    case WebIDBKeyPathTypeNull:
      return WebIDBKeyPath::createNull();
  }
  NOTREACHED();
  return WebIDBKeyPath::createNull();
}

}  // namespace content
