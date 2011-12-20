// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_database_callbacks.h"

#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "content/common/indexed_db/indexed_db_messages.h"

IndexedDBDatabaseCallbacks::IndexedDBDatabaseCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    int thread_id,
    int database_id)
    : dispatcher_host_(dispatcher_host),
      thread_id_(thread_id),
      database_id_(database_id) {
}

IndexedDBDatabaseCallbacks::~IndexedDBDatabaseCallbacks() {
}

void IndexedDBDatabaseCallbacks::onVersionChange(
    const WebKit::WebString& requested_version) {
  dispatcher_host_->Send(
      new IndexedDBMsg_DatabaseCallbacksVersionChange(thread_id_, database_id_,
                                                      requested_version));
}
