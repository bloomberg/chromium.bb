// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/indexed_db/proxy_webidbcursor_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "ipc/ipc_sync_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebIDBCallbacks.h"

using WebKit::WebData;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBKey;
using WebKit::WebIDBKeyTypeNumber;

namespace content {

namespace {

class MockDispatcher : public IndexedDBDispatcher {
 public:
  MockDispatcher(ThreadSafeSender* thread_safe_sender)
      : IndexedDBDispatcher(thread_safe_sender),
        prefetch_calls_(0),
        last_prefetch_count_(0),
        continue_calls_(0),
        destroyed_cursor_id_(0) {}

  virtual void RequestIDBCursorPrefetch(int n,
                                        WebIDBCallbacks* callbacks,
                                        int32 ipc_cursor_id) OVERRIDE {
    ++prefetch_calls_;
    last_prefetch_count_ = n;
    callbacks_.reset(callbacks);
  }

  virtual void RequestIDBCursorContinue(const IndexedDBKey&,
                                        WebIDBCallbacks* callbacks,
                                        int32 ipc_cursor_id) OVERRIDE {
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
  MockContinueCallbacks(IndexedDBKey* key = 0) : key_(key) {}

  virtual void onSuccess(const WebIDBKey& key,
                         const WebIDBKey& primaryKey,
                         const WebData& value) {

    if (key_)
      *key_ = IndexedDBKeyBuilder::Build(key);
  }

 private:
  IndexedDBKey* key_;
};

}  // namespace

TEST(RendererWebIDBCursorImplTest, PrefetchTest) {

  WebIDBKey null_key;
  null_key.assignNull();

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy(
      base::MessageLoopProxy::current());
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter(
      new IPC::SyncMessageFilter(NULL));
  scoped_refptr<ThreadSafeSender> thread_safe_sender(new ThreadSafeSender(
      message_loop_proxy.get(), sync_message_filter.get()));

  MockDispatcher dispatcher(thread_safe_sender.get());

  {
    RendererWebIDBCursorImpl cursor(RendererWebIDBCursorImpl::kInvalidCursorId,
                                    thread_safe_sender.get());

    // Call continue() until prefetching should kick in.
    int continue_calls = 0;
    EXPECT_EQ(dispatcher.continue_calls(), 0);
    for (int i = 0; i < RendererWebIDBCursorImpl::kPrefetchContinueThreshold;
         ++i) {
      cursor.continueFunction(null_key, new MockContinueCallbacks());
      EXPECT_EQ(++continue_calls, dispatcher.continue_calls());
      EXPECT_EQ(0, dispatcher.prefetch_calls());
    }

    // Do enough repetitions to verify that the count grows each time,
    // but not so many that the maximum limit is hit.
    const int kPrefetchRepetitions = 5;

    int expected_key = 0;
    int last_prefetch_count = 0;
    for (int repetitions = 0; repetitions < kPrefetchRepetitions;
         ++repetitions) {

      // Initiate the prefetch
      cursor.continueFunction(null_key, new MockContinueCallbacks());
      EXPECT_EQ(continue_calls, dispatcher.continue_calls());
      EXPECT_EQ(repetitions + 1, dispatcher.prefetch_calls());

      // Verify that the requested count has increased since last time.
      int prefetch_count = dispatcher.last_prefetch_count();
      EXPECT_GT(prefetch_count, last_prefetch_count);
      last_prefetch_count = prefetch_count;

      // Fill the prefetch cache as requested.
      std::vector<IndexedDBKey> keys;
      std::vector<IndexedDBKey> primary_keys(prefetch_count);
      std::vector<WebData> values(prefetch_count);
      for (int i = 0; i < prefetch_count; ++i) {
        keys.push_back(IndexedDBKey(expected_key + i, WebIDBKeyTypeNumber));
      }
      cursor.SetPrefetchData(keys, primary_keys, values);

      // Note that the real dispatcher would call cursor->CachedContinue()
      // immediately after cursor->SetPrefetchData() to service the request
      // that initiated the prefetch.

      // Verify that the cache is used for subsequent continue() calls.
      for (int i = 0; i < prefetch_count; ++i) {
        IndexedDBKey key;
        cursor.continueFunction(null_key, new MockContinueCallbacks(&key));
        EXPECT_EQ(continue_calls, dispatcher.continue_calls());
        EXPECT_EQ(repetitions + 1, dispatcher.prefetch_calls());

        EXPECT_EQ(WebIDBKeyTypeNumber, key.type());
        EXPECT_EQ(expected_key++, key.number());
      }
    }
  }

  EXPECT_EQ(dispatcher.destroyed_cursor_id(),
            RendererWebIDBCursorImpl::kInvalidCursorId);
}

}  // namespace content
