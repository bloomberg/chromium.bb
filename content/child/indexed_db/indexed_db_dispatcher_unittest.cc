// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "ipc/ipc_sync_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebIDBCallbacks.h"

using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseError;
using blink::WebIDBKey;
using blink::WebVector;

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

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy(
      base::MessageLoopProxy::current());
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter(
      new IPC::SyncMessageFilter(NULL));
  scoped_refptr<ThreadSafeSender> thread_safe_sender(new ThreadSafeSender(
      message_loop_proxy.get(), sync_message_filter.get()));

  MockCallbacks callbacks;
  IndexedDBDispatcher dispatcher(thread_safe_sender.get());
  IndexedDBKey key(0, blink::WebIDBKeyTypeNumber);
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
