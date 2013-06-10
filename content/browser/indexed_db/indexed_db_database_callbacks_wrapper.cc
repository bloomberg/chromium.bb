// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks_wrapper.h"

namespace content {

IndexedDBDatabaseCallbacksWrapper::IndexedDBDatabaseCallbacksWrapper(
    IndexedDBDatabaseCallbacks* callbacks)
    : callbacks_(callbacks) {}
IndexedDBDatabaseCallbacksWrapper::~IndexedDBDatabaseCallbacksWrapper() {}

void IndexedDBDatabaseCallbacksWrapper::OnForcedClose() {
  if (!callbacks_)
    return;
  callbacks_->onForcedClose();
  callbacks_.reset();
}
void IndexedDBDatabaseCallbacksWrapper::OnVersionChange(int64 old_version,
                                                        int64 new_version) {
  if (!callbacks_)
    return;
  callbacks_->onVersionChange(old_version, new_version);
}

void IndexedDBDatabaseCallbacksWrapper::OnAbort(
    int64 transaction_id,
    const IndexedDBDatabaseError& error) {
  if (!callbacks_)
    return;
  callbacks_->onAbort(
      transaction_id,
      WebKit::WebIDBDatabaseError(error.code(), error.message()));
}
void IndexedDBDatabaseCallbacksWrapper::OnComplete(int64 transaction_id) {
  if (!callbacks_)
    return;
  callbacks_->onComplete(transaction_id);
}

}  // namespace content
