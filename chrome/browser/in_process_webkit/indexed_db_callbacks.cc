// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_callbacks.h"

#include "base/logging.h"
#include "chrome/browser/in_process_webkit/indexed_db_dispatcher_host.h"

using WebKit::WebIDBDatabase;
using WebKit::WebSerializedScriptValue;

IndexedDBCallbacks::IndexedDBCallbacks(IndexedDBDispatcherHost* parent,
                                       int32 response_id)
    : parent_(parent),
      response_id_(response_id) {
}

IndexedDBCallbacks::~IndexedDBCallbacks() {
}

void IndexedDBCallbacks::onSuccess(WebIDBDatabase* idb_database) {
  NOTREACHED();
}

void IndexedDBCallbacks::onSuccess(const WebSerializedScriptValue& value) {
  NOTREACHED();
}

