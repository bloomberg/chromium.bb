// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebIDBKey.h"

using WebKit::WebIDBKey;
using WebKit::WebVector;

namespace content {

namespace {

TEST(IndexedDBKeyTest, KeySizeEstimates) {
  std::vector<IndexedDBKey> keys;
  std::vector<WebIDBKey> web_keys;
  std::vector<size_t> estimates;

  keys.push_back(IndexedDBKey());
  web_keys.push_back(WebIDBKey::createInvalid());
  estimates.push_back(static_cast<size_t>(16));  // Overhead.

  keys.push_back(IndexedDBKey(WebIDBKey::NullType));
  web_keys.push_back(WebIDBKey::createNull());
  estimates.push_back(static_cast<size_t>(16));

  double number = 3.14159;
  keys.push_back(IndexedDBKey(number, WebIDBKey::NumberType));
  web_keys.push_back(WebIDBKey::createNumber(number));
  estimates.push_back(static_cast<size_t>(24));  // Overhead + sizeof(double).

  double date = 1370884329.0;
  keys.push_back(IndexedDBKey(date, WebIDBKey::DateType));
  web_keys.push_back(WebIDBKey::createDate(date));
  estimates.push_back(static_cast<size_t>(24));  // Overhead + sizeof(double).

  const string16 string(1024, static_cast<char16>('X'));
  keys.push_back(IndexedDBKey(string));
  web_keys.push_back(WebIDBKey::createString(string));
  // Overhead + string length * sizeof(char16).
  estimates.push_back(static_cast<size_t>(2064));

  const size_t array_size = 1024;
  IndexedDBKey::KeyArray array;
  WebVector<WebIDBKey> web_array(static_cast<size_t>(array_size));
  double value = 123.456;
  for (size_t i = 0; i < array_size; ++i) {
    array.push_back(IndexedDBKey(value, WebIDBKey::NumberType));
    web_array[i] = WebIDBKey::createNumber(value);
  }
  keys.push_back(IndexedDBKey(array));
  web_keys.push_back(WebIDBKey::createArray(array));
  // Overhead + array length * (Overhead + sizeof(double)).
  estimates.push_back(static_cast<size_t>(24592));

  ASSERT_EQ(keys.size(), web_keys.size());
  ASSERT_EQ(keys.size(), estimates.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    EXPECT_EQ(estimates[i], keys[i].size_estimate());
    EXPECT_EQ(estimates[i], IndexedDBKey(web_keys[i]).size_estimate());
  }
}

}  // namespace

}  // namespace content
