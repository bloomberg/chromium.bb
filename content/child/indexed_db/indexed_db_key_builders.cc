// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_key_builders.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using WebKit::WebIDBKey;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBKeyTypeArray;
using WebKit::WebIDBKeyTypeDate;
using WebKit::WebIDBKeyTypeInvalid;
using WebKit::WebIDBKeyTypeMin;
using WebKit::WebIDBKeyTypeNull;
using WebKit::WebIDBKeyTypeNumber;
using WebKit::WebIDBKeyTypeString;
using WebKit::WebVector;
using WebKit::WebString;

static content::IndexedDBKey::KeyArray CopyKeyArray(const WebIDBKey& other) {
  content::IndexedDBKey::KeyArray result;
  if (other.keyType() == WebIDBKeyTypeArray) {
    const WebVector<WebIDBKey>& array = other.array();
    for (size_t i = 0; i < array.size(); ++i)
      result.push_back(content::IndexedDBKeyBuilder::Build(array[i]));
  }
  return result;
}

static std::vector<string16> CopyArray(
    const WebVector<WebString>& array) {
  std::vector<string16> copy(array.size());
  for (size_t i = 0; i < array.size(); ++i)
    copy[i] = array[i];
  return copy;
}


namespace content {

IndexedDBKey IndexedDBKeyBuilder::Build(const WebKit::WebIDBKey& key) {
  switch (key.keyType()) {
    case WebIDBKeyTypeArray:
      return IndexedDBKey(CopyKeyArray(key));
    case WebIDBKeyTypeString:
      return IndexedDBKey(key.string());
    case WebIDBKeyTypeDate:
      return IndexedDBKey(key.date(), WebIDBKeyTypeDate);
    case WebIDBKeyTypeNumber:
      return IndexedDBKey(key.number(), WebIDBKeyTypeNumber);
    case WebIDBKeyTypeNull:
    case WebIDBKeyTypeInvalid:
      return IndexedDBKey(key.keyType());
    case WebIDBKeyTypeMin:
      NOTREACHED();
      return IndexedDBKey();
  }
  NOTREACHED();
  return IndexedDBKey();
}

WebIDBKey WebIDBKeyBuilder::Build(const IndexedDBKey& key) {
  switch (key.type()) {
    case WebIDBKeyTypeArray: {
      const IndexedDBKey::KeyArray& array = key.array();
      WebKit::WebVector<WebIDBKey> web_array(array.size());
      for (size_t i = 0; i < array.size(); ++i) {
        web_array[i] = Build(array[i]);
      }
      return WebIDBKey::createArray(web_array);
    }
    case WebIDBKeyTypeString:
      return WebIDBKey::createString(key.string());
    case WebIDBKeyTypeDate:
      return WebIDBKey::createDate(key.date());
    case WebIDBKeyTypeNumber:
      return WebIDBKey::createNumber(key.number());
    case WebIDBKeyTypeInvalid:
      return WebIDBKey::createInvalid();
    case WebIDBKeyTypeNull:
      return WebIDBKey::createNull();
    case WebIDBKeyTypeMin:
      NOTREACHED();
      return WebIDBKey::createInvalid();
  }
  NOTREACHED();
  return WebIDBKey::createInvalid();
}

IndexedDBKeyRange IndexedDBKeyRangeBuilder::Build(
    const WebIDBKeyRange& key_range) {
  return IndexedDBKeyRange(
    IndexedDBKeyBuilder::Build(key_range.lower()),
    IndexedDBKeyBuilder::Build(key_range.upper()),
    key_range.lowerOpen(),
    key_range.upperOpen());
}

IndexedDBKeyPath IndexedDBKeyPathBuilder::Build(
    const WebKit::WebIDBKeyPath& key_path) {
  switch (key_path.keyPathType()) {
    case WebKit::WebIDBKeyPathTypeString:
      return IndexedDBKeyPath(key_path.string());
    case WebKit::WebIDBKeyPathTypeArray:
      return IndexedDBKeyPath(CopyArray(key_path.array()));
    case WebKit::WebIDBKeyPathTypeNull:
      return IndexedDBKeyPath();
  }
  NOTREACHED();
  return IndexedDBKeyPath();
}

WebKit::WebIDBKeyPath WebIDBKeyPathBuilder::Build(
    const IndexedDBKeyPath& key_path) {
  switch (key_path.type()) {
    case WebKit::WebIDBKeyPathTypeString:
      return WebKit::WebIDBKeyPath::create(WebString(key_path.string()));
    case WebKit::WebIDBKeyPathTypeArray:
      return WebKit::WebIDBKeyPath::create(CopyArray(key_path.array()));
    case WebKit::WebIDBKeyPathTypeNull:
      return WebKit::WebIDBKeyPath::createNull();
  }
  NOTREACHED();
  return WebKit::WebIDBKeyPath::createNull();
}

}  // namespace content
