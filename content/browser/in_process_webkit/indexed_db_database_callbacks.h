// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DATABASE_CALLBACKS_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseCallbacks.h"

namespace content {
class IndexedDBDispatcherHost;

class IndexedDBDatabaseCallbacks : public WebKit::WebIDBDatabaseCallbacks {
 public:
  IndexedDBDatabaseCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                             int ipc_thread_id,
                             int ipc_database_callbacks_id);

  virtual ~IndexedDBDatabaseCallbacks();

  virtual void onForcedClose();
  virtual void onVersionChange(long long old_version,
                               long long new_version);
  virtual void onAbort(long long host_transaction_id,
                       const WebKit::WebIDBDatabaseError&);
  virtual void onComplete(long long host_transaction_id);

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int ipc_thread_id_;
  int ipc_database_callbacks_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DATABASE_CALLBACKS_H_
