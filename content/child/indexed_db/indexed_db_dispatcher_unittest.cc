// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebIDBCallbacks.h"

using WebKit::WebData;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;
using WebKit::WebIDBKey;
using WebKit::WebVector;

namespace content {
namespace {

class MockCallbacks : public WebIDBCallbacks {
 public:
  MockCallbacks() : error_seen_(false) {}

  virtual void onError(const WebIDBDatabaseError&) OVERRIDE {
    error_seen_ = true;
  }

  bool error_seen() const { return error_seen_; }

 private:
  bool error_seen_;
};

}  // namespace

TEST(IndexedDBDispatcherTest, ValueSizeTest) {
  const std::vector<char> data(kMaxIDBValueSizeInBytes + 1);
  const WebData value(&data.front(), data.size());
  const int32 ipc_dummy_id = -1;
  const int64 transaction_id = 1;
  const int64 object_store_id = 2;

  MockCallbacks callbacks;
  IndexedDBDispatcher dispatcher;
  IndexedDBKey key(0, WebIDBKey::NumberType);
  dispatcher.RequestIDBDatabasePut(ipc_dummy_id,
                                   transaction_id,
                                   object_store_id,
                                   value,
                                   key,
                                   WebIDBDatabase::AddOrUpdate,
                                   &callbacks,
                                   WebVector<long long>(),
                                   WebVector<WebVector<WebIDBKey> >());

  EXPECT_TRUE(callbacks.error_seen());
}

}  // namespace content
