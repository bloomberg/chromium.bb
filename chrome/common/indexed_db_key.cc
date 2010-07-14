// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/indexed_db_key.h"

#include "base/logging.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebIDBKey;

IndexedDBKey::IndexedDBKey()
    : type_(WebIDBKey::InvalidType) {
}

IndexedDBKey::IndexedDBKey(const WebIDBKey& key)
    : type_(key.type()),
      string_(key.type() == WebIDBKey::StringType
              ? static_cast<string16>(key.string()) : string16()),
      number_(key.type() == WebIDBKey::NumberType ? key.number() : 0) {
}

void IndexedDBKey::SetNull() {
  type_ = WebIDBKey::NullType;
}

void IndexedDBKey::SetInvalid() {
  type_ = WebIDBKey::InvalidType;
}

void IndexedDBKey::Set(const string16& string) {
  type_ = WebIDBKey::StringType;
  string_ = string;
}

void IndexedDBKey::Set(int32_t number) {
  type_ = WebIDBKey::NumberType;
  number_ = number;
}

IndexedDBKey::operator WebIDBKey() const {
  switch (type_) {
    case WebIDBKey::NullType:
      return WebIDBKey::createNull();
    case WebIDBKey::StringType:
      return WebIDBKey(string_);
    case WebIDBKey::NumberType:
      return WebIDBKey(number_);
    case WebIDBKey::InvalidType:
      return WebIDBKey::createInvalid();
  }
  NOTREACHED();
  return WebIDBKey::createInvalid();
}
