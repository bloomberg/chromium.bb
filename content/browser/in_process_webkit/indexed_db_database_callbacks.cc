// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_database_callbacks.h"

#include "chrome/common/indexed_db_messages.h"
#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"

IndexedDBDatabaseCallbacks::IndexedDBDatabaseCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    int database_id)
    : dispatcher_host_(dispatcher_host),
      database_id_(database_id) {
}

IndexedDBDatabaseCallbacks::~IndexedDBDatabaseCallbacks() {
}

void IndexedDBDatabaseCallbacks::onVersionChange(
    const WebKit::WebString& requested_version) {
  dispatcher_host_->Send(
      new IndexedDBMsg_DatabaseCallbacksVersionChange(
          database_id_, requested_version));
}
