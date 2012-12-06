// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_TRANSACTION_CALLBACKS_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_TRANSACTION_CALLBACKS_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransactionCallbacks.h"

namespace content {
class IndexedDBDispatcherHost;

class IndexedDBTransactionCallbacks
    : public WebKit::WebIDBTransactionCallbacks {
 public:
  IndexedDBTransactionCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                int ipc_thread_id,
                                int ipc_transaction_id);

  virtual ~IndexedDBTransactionCallbacks();

  virtual void onAbort(const WebKit::WebIDBDatabaseError& error);
  virtual void onComplete();

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int ipc_thread_id_;
  int ipc_transaction_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_TRANSACTION_CALLBACKS_H_
