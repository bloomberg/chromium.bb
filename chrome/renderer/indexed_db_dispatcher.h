// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_
#define CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_

#include "base/id_map.h"
#include "base/string16.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"

namespace WebKit {
class WebFrame;
}

// Handle the indexed db related communication for this entire renderer.
class IndexedDBDispatcher {
 public:
  IndexedDBDispatcher();
  ~IndexedDBDispatcher();

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled.
  bool OnMessageReceived(const IPC::Message& msg);

  void RequestIndexedDatabaseOpen(
      const string16& name, const string16& description,
      WebKit::WebIDBCallbacks* callbacks, const string16& origin,
      WebKit::WebFrame* web_frame, int* exception_code);

  void SendIDBDatabaseDestroyed(int32 idb_database_id_);

 private:
  // Message handlers.  For each message we send, we need to handle both the
  // success and the error case.
  void OnIndexedDatabaseOpenSuccess(int32 response_id,
                                    int32 idb_database_id);
  void OnIndexedDatabaseOpenError(int32 response_id, int code,
                                  const string16& message);

  // Careful! WebIDBCallbacks wraps non-threadsafe data types. It must be
  // destroyed and used on the same thread it was created on.
  IDMap<WebKit::WebIDBCallbacks, IDMapOwnPointer>
      indexed_database_open_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcher);
};

#endif  // CHROME_RENDERER_INDEXED_DB_DISPATCHER_H_
