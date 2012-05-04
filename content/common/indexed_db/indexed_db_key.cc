// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

namespace content {

using WebKit::WebIDBKey;
using WebKit::WebVector;

IndexedDBKey::IndexedDBKey()
    : type_(WebIDBKey::NullType),
      date_(0),
      number_(0) {
}

IndexedDBKey::IndexedDBKey(const WebIDBKey& key) {
  Set(key);
}

IndexedDBKey::~IndexedDBKey() {
}

void IndexedDBKey::SetInvalid() {
  type_ = WebIDBKey::InvalidType;
}

void IndexedDBKey::SetNull() {
  type_ = WebIDBKey::NullType;
}

void IndexedDBKey::SetArray(const std::vector<IndexedDBKey>& array) {
  type_ = WebIDBKey::ArrayType;
  array_ = array;
}

void IndexedDBKey::SetString(const string16& string) {
  type_ = WebIDBKey::StringType;
  string_ = string;
}

void IndexedDBKey::SetDate(double date) {
  type_ = WebIDBKey::DateType;
  date_ = date;
}

void IndexedDBKey::SetNumber(double number) {
  type_ = WebIDBKey::NumberType;
  number_ = number;
}

void IndexedDBKey::Set(const WebIDBKey& key) {
  type_ = key.type();
  array_.clear();
  if (key.type() == WebIDBKey::ArrayType) {
    WebVector<WebIDBKey> array = key.array();
    for (size_t i = 0; i < array.size(); ++i) {
      array_.push_back(IndexedDBKey(array[i]));
    }
  }
  string_ = key.type() == WebIDBKey::StringType ?
                static_cast<string16>(key.string()) : string16();
  number_ = key.type() == WebIDBKey::NumberType ? key.number() : 0;
  date_ = key.type() == WebIDBKey::DateType ? key.date() : 0;
}

IndexedDBKey::operator WebIDBKey() const {
  switch (type_) {
    case WebIDBKey::ArrayType:
      return WebIDBKey::createArray(array_);
    case WebIDBKey::StringType:
      return WebIDBKey::createString(string_);
    case WebIDBKey::DateType:
      return WebIDBKey::createDate(date_);
    case WebIDBKey::NumberType:
      return WebIDBKey::createNumber(number_);
    case WebIDBKey::InvalidType:
      return WebIDBKey::createInvalid();
    case WebIDBKey::NullType:
      return WebIDBKey::createNull();
  }
  NOTREACHED();
  return WebIDBKey::createInvalid();
}

}  // namespace content
