// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key_path.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"

namespace content {

using WebKit::WebIDBKeyPath;
using WebKit::WebString;
using WebKit::WebVector;

IndexedDBKeyPath::IndexedDBKeyPath()
    : type_(WebIDBKeyPath::NullType) {
}

IndexedDBKeyPath::IndexedDBKeyPath(const WebIDBKeyPath& key) {
  Set(key);
}

IndexedDBKeyPath::~IndexedDBKeyPath() {
}

void IndexedDBKeyPath::SetNull() {
  type_ = WebIDBKeyPath::NullType;
}

void IndexedDBKeyPath::SetArray(const std::vector<string16>& array) {
  type_ = WebIDBKeyPath::ArrayType;
  array_ = array;
}

void IndexedDBKeyPath::SetString(const string16& string) {
  type_ = WebIDBKeyPath::StringType;
  string_ = string;
}

void IndexedDBKeyPath::Set(const WebIDBKeyPath& keyPath) {
  type_ = keyPath.type();
  array_.clear();
  string_.clear();
  switch (type_) {
    case WebIDBKeyPath::ArrayType: {
      WebVector<WebString> array = keyPath.array();
      for (size_t i = 0; i < array.size(); ++i)
        array_.push_back(static_cast<string16>(array[i]));
      break;
    }
    case WebIDBKeyPath::NullType:
      break;
    case WebIDBKeyPath::StringType:
      string_ = static_cast<string16>(keyPath.string());
  }
}

bool IndexedDBKeyPath::IsValid() const {
  WebIDBKeyPath key_path = *this;
  return key_path.isValid();
}

IndexedDBKeyPath::operator WebIDBKeyPath() const {
  switch (type_) {
    case WebIDBKeyPath::ArrayType:
      return WebIDBKeyPath::create(array_);
    case WebIDBKeyPath::StringType:
      return WebIDBKeyPath::create(WebString(string_));
    case WebIDBKeyPath::NullType:
      return WebIDBKeyPath::createNull();
  }
  NOTREACHED();
  return WebIDBKeyPath::createNull();
}

}  // namespace content
