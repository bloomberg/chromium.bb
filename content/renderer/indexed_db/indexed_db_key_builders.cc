// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/indexed_db/indexed_db_key_builders.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using blink::WebIDBKey;
using blink::WebIDBKeyRange;
using blink::kWebIDBKeyTypeArray;
using blink::kWebIDBKeyTypeBinary;
using blink::kWebIDBKeyTypeDate;
using blink::kWebIDBKeyTypeInvalid;
using blink::kWebIDBKeyTypeMin;
using blink::kWebIDBKeyTypeNull;
using blink::kWebIDBKeyTypeNumber;
using blink::kWebIDBKeyTypeString;
using blink::WebVector;
using blink::WebString;

static content::IndexedDBKey::KeyArray CopyKeyArray(const WebIDBKey& other) {
  content::IndexedDBKey::KeyArray result;
  if (other.KeyType() == kWebIDBKeyTypeArray) {
    const WebVector<WebIDBKey>& array = other.Array();
    for (size_t i = 0; i < array.size(); ++i)
      result.push_back(content::IndexedDBKeyBuilder::Build(array[i]));
  }
  return result;
}

static std::vector<base::string16> CopyArray(
    const WebVector<WebString>& array) {
  std::vector<base::string16> copy(array.size());
  for (size_t i = 0; i < array.size(); ++i)
    copy[i] = array[i].Utf16();
  return copy;
}

namespace content {

IndexedDBKey IndexedDBKeyBuilder::Build(const blink::WebIDBKey& key) {
  switch (key.KeyType()) {
    case kWebIDBKeyTypeArray:
      return IndexedDBKey(CopyKeyArray(key));
    case kWebIDBKeyTypeBinary: {
      const blink::WebData data = key.Binary();
      std::string key_string;
      key_string.reserve(data.size());

      data.ForEachSegment([&key_string](const char* segment,
                                        size_t segment_size,
                                        size_t segment_offset) {
        key_string.append(segment, segment_size);
        return true;
      });
      return IndexedDBKey(key_string);
    }
    case kWebIDBKeyTypeString:
      return IndexedDBKey(key.GetString().Utf16());
    case kWebIDBKeyTypeDate:
      return IndexedDBKey(key.Date(), kWebIDBKeyTypeDate);
    case kWebIDBKeyTypeNumber:
      return IndexedDBKey(key.Number(), kWebIDBKeyTypeNumber);
    case kWebIDBKeyTypeNull:
    case kWebIDBKeyTypeInvalid:
      return IndexedDBKey(key.KeyType());
    case kWebIDBKeyTypeMin:
    default:
      NOTREACHED();
      return IndexedDBKey();
  }
}

WebIDBKey WebIDBKeyBuilder::Build(const IndexedDBKey& key) {
  switch (key.type()) {
    case kWebIDBKeyTypeArray: {
      const IndexedDBKey::KeyArray& array = key.array();
      blink::WebVector<WebIDBKey> web_array(array.size());
      for (size_t i = 0; i < array.size(); ++i) {
        web_array[i] = Build(array[i]);
      }
      return WebIDBKey::CreateArray(web_array);
    }
    case kWebIDBKeyTypeBinary:
      return WebIDBKey::CreateBinary(key.binary());
    case kWebIDBKeyTypeString:
      return WebIDBKey::CreateString(WebString::FromUTF16(key.string()));
    case kWebIDBKeyTypeDate:
      return WebIDBKey::CreateDate(key.date());
    case kWebIDBKeyTypeNumber:
      return WebIDBKey::CreateNumber(key.number());
    case kWebIDBKeyTypeInvalid:
      return WebIDBKey::CreateInvalid();
    case kWebIDBKeyTypeNull:
      return WebIDBKey::CreateNull();
    case kWebIDBKeyTypeMin:
    default:
      NOTREACHED();
      return WebIDBKey::CreateInvalid();
  }
}

IndexedDBKeyRange IndexedDBKeyRangeBuilder::Build(
    const WebIDBKeyRange& key_range) {
  return IndexedDBKeyRange(IndexedDBKeyBuilder::Build(key_range.Lower()),
                           IndexedDBKeyBuilder::Build(key_range.Upper()),
                           key_range.LowerOpen(), key_range.UpperOpen());
}

WebIDBKeyRange WebIDBKeyRangeBuilder::Build(
    const IndexedDBKeyRange& key_range) {
  return WebIDBKeyRange(WebIDBKeyBuilder::Build(key_range.lower()),
                        WebIDBKeyBuilder::Build(key_range.upper()),
                        key_range.lower_open(), key_range.upper_open());
}

IndexedDBKeyPath IndexedDBKeyPathBuilder::Build(
    const blink::WebIDBKeyPath& key_path) {
  switch (key_path.KeyPathType()) {
    case blink::kWebIDBKeyPathTypeString:
      return IndexedDBKeyPath(key_path.GetString().Utf16());
    case blink::kWebIDBKeyPathTypeArray:
      return IndexedDBKeyPath(CopyArray(key_path.Array()));
    case blink::kWebIDBKeyPathTypeNull:
      return IndexedDBKeyPath();
    default:
      NOTREACHED();
      return IndexedDBKeyPath();
  }
}

blink::WebIDBKeyPath WebIDBKeyPathBuilder::Build(
    const IndexedDBKeyPath& key_path) {
  switch (key_path.type()) {
    case blink::kWebIDBKeyPathTypeString:
      return blink::WebIDBKeyPath::Create(
          WebString::FromUTF16(key_path.string()));
    case blink::kWebIDBKeyPathTypeArray: {
      WebVector<WebString> key_path_vector(key_path.array().size());
      std::transform(key_path.array().begin(), key_path.array().end(),
                     key_path_vector.begin(),
                     [](const typename base::string16& s) {
                       return WebString::FromUTF16(s);
                     });
      return blink::WebIDBKeyPath::Create(key_path_vector);
    }
    case blink::kWebIDBKeyPathTypeNull:
      return blink::WebIDBKeyPath::CreateNull();
    default:
      NOTREACHED();
      return blink::WebIDBKeyPath::CreateNull();
  }
}

}  // namespace content
