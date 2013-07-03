// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection.h"

namespace content {

IndexedDBConnection::IndexedDBConnection(
    scoped_refptr<IndexedDBDatabase> database,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks)
    : database_(database), callbacks_(callbacks) {}

IndexedDBConnection::~IndexedDBConnection() {}

void IndexedDBConnection::Close() {
  if (!callbacks_)
    return;
  database_->Close(this);
  callbacks_ = NULL;
}

void IndexedDBConnection::ForceClose() {
  if (!callbacks_)
    return;
  database_->Close(this);
  callbacks_->OnForcedClose();
  callbacks_ = NULL;
}

}  // namespace WebKit
