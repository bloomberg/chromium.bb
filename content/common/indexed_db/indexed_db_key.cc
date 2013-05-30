// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key.h"

#include <string>
#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

using WebKit::WebIDBKey;
using WebKit::WebVector;

namespace {
size_t CalculateKeySize(const WebIDBKey& key);

template <typename T> static size_t CalculateSize(const T& keys) {
  size_t size(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    size += CalculateKeySize(keys[i]);
  }
  return size;
}

size_t CalculateKeySize(const WebIDBKey& key) {
  switch (key.type()) {
    case WebIDBKey::ArrayType:
      return CalculateSize(key.array());

    case WebIDBKey::StringType:
      return key.string().length() * sizeof(string16::value_type);

    case WebIDBKey::DateType:
    case WebIDBKey::NumberType:
      return sizeof(double);
    default:
      return 0;
  }
  NOTREACHED();
  return 0;
}

template <typename T>
static IndexedDBKey::KeyArray
CopyKeyArray(const T& array) {
  IndexedDBKey::KeyArray result;
  result.reserve(array.size());
  for (size_t i = 0; i < array.size(); ++i) {
    result.push_back(IndexedDBKey(array[i]));
  }
  return result;
}

static IndexedDBKey::KeyArray CopyKeyArray(const WebIDBKey& other) {
  IndexedDBKey::KeyArray result;
  if (other.type() == WebIDBKey::ArrayType) {
    result = CopyKeyArray(other.array());
  }
  return result;
}
}  // namespace

IndexedDBKey::IndexedDBKey()
    : type_(WebIDBKey::NullType),
      date_(0),
      number_(0),
      size_estimate_(kOverheadSize) {}

IndexedDBKey::IndexedDBKey(WebIDBKey::Type type)
    : type_(type), date_(0), number_(0), size_estimate_(kOverheadSize) {
  DCHECK(type == WebIDBKey::NullType || type == WebIDBKey::InvalidType);
}

IndexedDBKey::IndexedDBKey(double number, WebIDBKey::Type type)
    : type_(type),
      date_(number),
      number_(number),
      size_estimate_(kOverheadSize + sizeof(double)) {
  DCHECK(type == WebIDBKey::NumberType || type == WebIDBKey::DateType);
}

IndexedDBKey::IndexedDBKey(const KeyArray& keys)
    : type_(WebIDBKey::ArrayType),
      array_(CopyKeyArray(keys)),
      date_(0),
      number_(0),
      size_estimate_(kOverheadSize + CalculateSize(keys)) {}

IndexedDBKey::IndexedDBKey(const WebIDBKey& key)
    : type_(key.type()),
      array_(CopyKeyArray(key)),
      string_(key.type() == WebIDBKey::StringType
                  ? static_cast<string16>(key.string())
                  : string16()),
      date_(key.type() == WebIDBKey::DateType ? key.date() : 0),
      number_(key.type() == WebIDBKey::NumberType ? key.number() : 0),
      size_estimate_(kOverheadSize + CalculateKeySize(key)) {}

IndexedDBKey::IndexedDBKey(const string16& key)
    : type_(WebIDBKey::StringType), string_(key) {}

IndexedDBKey::~IndexedDBKey() {}

int IndexedDBKey::Compare(const IndexedDBKey& other) const {
  DCHECK(IsValid());
  DCHECK(other.IsValid());
  if (type_ != other.type_)
    return type_ > other.type_ ? -1 : 1;

  switch (type_) {
    case WebIDBKey::ArrayType:
      for (size_t i = 0; i < array_.size() && i < other.array_.size(); ++i) {
        if (int result = array_[i].Compare(other.array_[i]))
          return result;
      }
      if (array_.size() < other.array_.size())
        return -1;
      if (array_.size() > other.array_.size())
        return 1;
      return 0;
    case WebIDBKey::StringType:
      return -other.string_.compare(string_);
    case WebIDBKey::DateType:
      return (date_ < other.date_) ? -1 : (date_ > other.date_) ? 1 : 0;
    case WebIDBKey::NumberType:
      return (number_ < other.number_) ? -1 : (number_ > other.number_) ? 1 : 0;
    case WebIDBKey::InvalidType:
    case WebIDBKey::NullType:
    case WebIDBKey::MinType:
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
  if (type_ == WebIDBKey::InvalidType || type_ == WebIDBKey::NullType)
    return false;

  if (type_ == WebIDBKey::ArrayType) {
    for (size_t i = 0; i < array_.size(); i++) {
      if (!array_[i].IsValid())
        return false;
    }
  }

  return true;
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
    case WebIDBKey::MinType:
      NOTREACHED();
      return WebIDBKey::createInvalid();
  }
  NOTREACHED();
  return WebIDBKey::createInvalid();
}

}  // namespace content
