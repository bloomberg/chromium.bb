// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key.h"

#include <string>
#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebIDBKey.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

using WebKit::WebIDBKey;
using WebKit::WebIDBKeyType;
using WebKit::WebIDBKeyTypeArray;
using WebKit::WebIDBKeyTypeDate;
using WebKit::WebIDBKeyTypeInvalid;
using WebKit::WebIDBKeyTypeMin;
using WebKit::WebIDBKeyTypeNull;
using WebKit::WebIDBKeyTypeNumber;
using WebKit::WebIDBKeyTypeString;
using WebKit::WebVector;

namespace {

// Very rough estimate of minimum key size overhead.
const size_t kOverheadSize = 16;

static size_t CalculateArraySize(const IndexedDBKey::KeyArray& keys) {
  size_t size(0);
  for (size_t i = 0; i < keys.size(); ++i)
    size += keys[i].size_estimate();
  return size;
}

static size_t CalculateKeySize(const WebIDBKey& key) {
  switch (key.keyType()) {
    case WebIDBKeyTypeArray: {
      const WebVector<WebIDBKey>& array = key.array();
      size_t total = 0;
      for (size_t i = 0; i < array.size(); ++i)
        total += CalculateKeySize(array[i]);
      return kOverheadSize + total;
    }
    case WebIDBKeyTypeString:
      return kOverheadSize +
             (key.string().length() * sizeof(string16::value_type));

    case WebIDBKeyTypeDate:
    case WebIDBKeyTypeNumber:
      return kOverheadSize + sizeof(double);

    default:
      return kOverheadSize;
  }
  NOTREACHED();
  return 0;
}

template <typename T>
static IndexedDBKey::KeyArray CopyKeyArray(const T& array) {
  IndexedDBKey::KeyArray result;
  result.reserve(array.size());
  for (size_t i = 0; i < array.size(); ++i) {
    result.push_back(IndexedDBKey(array[i]));
  }
  return result;
}

static IndexedDBKey::KeyArray CopyKeyArray(const WebIDBKey& other) {
  IndexedDBKey::KeyArray result;
  if (other.keyType() == WebIDBKeyTypeArray) {
    result = CopyKeyArray(other.array());
  }
  return result;
}
}  // namespace

IndexedDBKey::IndexedDBKey()
    : type_(WebIDBKeyTypeNull),
      date_(0),
      number_(0),
      size_estimate_(kOverheadSize) {}

IndexedDBKey::IndexedDBKey(WebIDBKeyType type)
    : type_(type), date_(0), number_(0), size_estimate_(kOverheadSize) {
  DCHECK(type == WebIDBKeyTypeNull || type == WebIDBKeyTypeInvalid);
}

IndexedDBKey::IndexedDBKey(double number, WebIDBKeyType type)
    : type_(type),
      date_(number),
      number_(number),
      size_estimate_(kOverheadSize + sizeof(number)) {
  DCHECK(type == WebIDBKeyTypeNumber || type == WebIDBKeyTypeDate);
}

IndexedDBKey::IndexedDBKey(const KeyArray& keys)
    : type_(WebIDBKeyTypeArray),
      array_(CopyKeyArray(keys)),
      date_(0),
      number_(0),
      size_estimate_(kOverheadSize + CalculateArraySize(keys)) {}

IndexedDBKey::IndexedDBKey(const string16& key)
    : type_(WebIDBKeyTypeString),
      string_(key),
      size_estimate_(kOverheadSize +
                     (key.length() * sizeof(string16::value_type))) {}

IndexedDBKey::IndexedDBKey(const WebIDBKey& key)
    : type_(key.keyType()),
      array_(CopyKeyArray(key)),
      string_(key.keyType() == WebIDBKeyTypeString
                  ? static_cast<string16>(key.string())
                  : string16()),
      date_(key.keyType() == WebIDBKeyTypeDate ? key.date() : 0),
      number_(key.keyType() == WebIDBKeyTypeNumber ? key.number() : 0),
      size_estimate_(CalculateKeySize(key)) {}

IndexedDBKey::~IndexedDBKey() {}

int IndexedDBKey::Compare(const IndexedDBKey& other) const {
  DCHECK(IsValid());
  DCHECK(other.IsValid());
  if (type_ != other.type_)
    return type_ > other.type_ ? -1 : 1;

  switch (type_) {
    case WebIDBKeyTypeArray:
      for (size_t i = 0; i < array_.size() && i < other.array_.size(); ++i) {
        if (int result = array_[i].Compare(other.array_[i]))
          return result;
      }
      if (array_.size() < other.array_.size())
        return -1;
      if (array_.size() > other.array_.size())
        return 1;
      return 0;
    case WebIDBKeyTypeString:
      return -other.string_.compare(string_);
    case WebIDBKeyTypeDate:
      return (date_ < other.date_) ? -1 : (date_ > other.date_) ? 1 : 0;
    case WebIDBKeyTypeNumber:
      return (number_ < other.number_) ? -1 : (number_ > other.number_) ? 1 : 0;
    case WebIDBKeyTypeInvalid:
    case WebIDBKeyTypeNull:
    case WebIDBKeyTypeMin:
      NOTREACHED();
      return 0;
  }
  NOTREACHED();
  return 0;
}

bool IndexedDBKey::IsLessThan(const IndexedDBKey& other) const {
  return Compare(other) < 0;
}

bool IndexedDBKey::IsEqual(const IndexedDBKey& other) const {
  return !Compare(other);
}

bool IndexedDBKey::IsValid() const {
  if (type_ == WebIDBKeyTypeInvalid || type_ == WebIDBKeyTypeNull)
    return false;

  if (type_ == WebIDBKeyTypeArray) {
    for (size_t i = 0; i < array_.size(); i++) {
      if (!array_[i].IsValid())
        return false;
    }
  }

  return true;
}

IndexedDBKey::operator WebIDBKey() const {
  switch (type_) {
    case WebIDBKeyTypeArray:
      return WebIDBKey::createArray(array_);
    case WebIDBKeyTypeString:
      return WebIDBKey::createString(string_);
    case WebIDBKeyTypeDate:
      return WebIDBKey::createDate(date_);
    case WebIDBKeyTypeNumber:
      return WebIDBKey::createNumber(number_);
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

}  // namespace content
