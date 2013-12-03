// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

TEST(IndexedDBKeyTest, KeySizeEstimates) {
  std::vector<IndexedDBKey> keys;
  std::vector<size_t> estimates;

  keys.push_back(IndexedDBKey());
  estimates.push_back(static_cast<size_t>(16));  // Overhead.

  keys.push_back(IndexedDBKey(blink::WebIDBKeyTypeNull));
  estimates.push_back(static_cast<size_t>(16));

  double number = 3.14159;
  keys.push_back(IndexedDBKey(number, blink::WebIDBKeyTypeNumber));
  estimates.push_back(static_cast<size_t>(24));  // Overhead + sizeof(double).

  double date = 1370884329.0;
  keys.push_back(IndexedDBKey(date, blink::WebIDBKeyTypeDate));
  estimates.push_back(static_cast<size_t>(24));  // Overhead + sizeof(double).

  const base::string16 string(1024, static_cast<char16>('X'));
  keys.push_back(IndexedDBKey(string));
  // Overhead + string length * sizeof(char16).
  estimates.push_back(static_cast<size_t>(2064));

  const size_t array_size = 1024;
  IndexedDBKey::KeyArray array;
  double value = 123.456;
  for (size_t i = 0; i < array_size; ++i) {
    array.push_back(IndexedDBKey(value, blink::WebIDBKeyTypeNumber));
  }
  keys.push_back(IndexedDBKey(array));
  // Overhead + array length * (Overhead + sizeof(double)).
  estimates.push_back(static_cast<size_t>(24592));

  ASSERT_EQ(keys.size(), estimates.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    EXPECT_EQ(estimates[i], keys[i].size_estimate());
  }
}

}  // namespace

}  // namespace content
