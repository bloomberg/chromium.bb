// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/proxy_webidbcursor_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExceptionCode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"

using WebKit::WebData;
using WebKit::WebExceptionCode;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;
using WebKit::WebIDBKey;

namespace content {

namespace {

class MockDispatcher : public IndexedDBDispatcher {
 public:
  MockDispatcher()
      : prefetch_calls_(0)
      , last_prefetch_count_(0)
      , continue_calls_(0)
      , destroyed_cursor_id_(0) {
  }

  virtual void RequestIDBCursorPrefetch(
      int n,
      WebIDBCallbacks* callbacks,
      int32 ipc_cursor_id,
      WebExceptionCode*) OVERRIDE {
    ++prefetch_calls_;
    last_prefetch_count_ = n;
    callbacks_.reset(callbacks);
  }

  virtual void RequestIDBCursorContinue(
      const IndexedDBKey&,
      WebIDBCallbacks* callbacks,
      int32 ipc_cursor_id,
      WebExceptionCode*) OVERRIDE {
    ++continue_calls_;
    callbacks_.reset(callbacks);
  }

  virtual void CursorDestroyed(int32 ipc_cursor_id) OVERRIDE {
    destroyed_cursor_id_ = ipc_cursor_id;
  }

  int prefetch_calls() { return prefetch_calls_; }
  int continue_calls() { return continue_calls_; }
  int last_prefetch_count() { return last_prefetch_count_; }
  int32 destroyed_cursor_id() { return destroyed_cursor_id_; }

 private:
  int prefetch_calls_;
  int last_prefetch_count_;
  int continue_calls_;
  int32 destroyed_cursor_id_;
  scoped_ptr<WebIDBCallbacks> callbacks_;
};

class MockContinueCallbacks : public WebIDBCallbacks {
 public:
  MockContinueCallbacks(IndexedDBKey* key = 0)
      : key_(key) {
  }

  virtual void onSuccess(const WebIDBKey& key,
                         const WebIDBKey& primaryKey,
                         const WebData& value) {
    if (key_)
      key_->Set(key);
  }

 private:
  IndexedDBKey* key_;
};

}  // namespace

TEST(RendererWebIDBCursorImplTest, PrefetchTest) {

  WebIDBKey null_key;
  null_key.assignNull();
  WebExceptionCode ec = 0;

  MockDispatcher dispatcher;

  {
    RendererWebIDBCursorImpl cursor(RendererWebIDBCursorImpl::kInvalidCursorId);

    // Call continue() until prefetching should kick in.
    int continue_calls = 0;
    EXPECT_EQ(dispatcher.continue_calls(), 0);
    for (int i = 0; i < RendererWebIDBCursorImpl::kPrefetchContinueThreshold;
         ++i) {
      cursor.continueFunction(null_key, new MockContinueCallbacks(), ec);
      EXPECT_EQ(ec, 0);
      EXPECT_EQ(dispatcher.continue_calls(), ++continue_calls);
      EXPECT_EQ(dispatcher.prefetch_calls(), 0);
    }

    // Do enough repetitions to verify that the count grows each time,
    // but not so many that the maximum limit is hit.
    const int kPrefetchRepetitions = 5;

    int expected_key = 0;
    int last_prefetch_count = 0;
    for (int repetitions = 0; repetitions < kPrefetchRepetitions;
         ++repetitions) {

      // Initiate the prefetch
      cursor.continueFunction(null_key, new MockContinueCallbacks(), ec);
      EXPECT_EQ(ec, 0);
      EXPECT_EQ(dispatcher.continue_calls(), continue_calls);
      EXPECT_EQ(dispatcher.prefetch_calls(), repetitions + 1);

      // Verify that the requested count has increased since last time.
      int prefetch_count = dispatcher.last_prefetch_count();
      EXPECT_GT(prefetch_count, last_prefetch_count);
      last_prefetch_count = prefetch_count;

      // Fill the prefetch cache as requested.
      std::vector<IndexedDBKey> keys(prefetch_count);
      std::vector<IndexedDBKey> primary_keys(prefetch_count);
      std::vector<WebData> values(prefetch_count);
      for (int i = 0; i < prefetch_count; ++i) {
        keys[i].SetNumber(expected_key + i);
      }
      cursor.SetPrefetchData(keys, primary_keys, values);

      // Note that the real dispatcher would call cursor->CachedContinue()
      // immediately after cursor->SetPrefetchData() to service the request
      // that initiated the prefetch.

      // Verify that the cache is used for subsequent continue() calls.
      for (int i = 0; i < prefetch_count; ++i) {
        IndexedDBKey key;
        cursor.continueFunction(null_key, new MockContinueCallbacks(&key), ec);
        EXPECT_EQ(ec, 0);
        EXPECT_EQ(dispatcher.continue_calls(), continue_calls);
        EXPECT_EQ(dispatcher.prefetch_calls(), repetitions + 1);

        EXPECT_EQ(key.type(), WebIDBKey::NumberType);
        EXPECT_EQ(key.number(), expected_key++);
      }
    }
  }

  EXPECT_EQ(dispatcher.destroyed_cursor_id(),
            RendererWebIDBCursorImpl::kInvalidCursorId);
}

}  // namespace content
