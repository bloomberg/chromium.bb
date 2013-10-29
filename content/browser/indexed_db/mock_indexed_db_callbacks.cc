// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockIndexedDBCallbacks::MockIndexedDBCallbacks()
    : IndexedDBCallbacks(NULL, 0, 0) {}

MockIndexedDBCallbacks::~MockIndexedDBCallbacks() { EXPECT_TRUE(connection_); }

void MockIndexedDBCallbacks::OnSuccess(
    scoped_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  connection_ = connection.Pass();
}

}  // namespace content
