// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_database_callbacks_impl.h"

#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseError.h"

using blink::WebIDBDatabaseCallbacks;

namespace content {

namespace {

void DeleteDatabaseCallbacks(WebIDBDatabaseCallbacks* callbacks) {
  IndexedDBDispatcher::ThreadSpecificInstance()
      ->UnregisterMojoOwnedDatabaseCallbacks(callbacks);
  delete callbacks;
}

void BuildErrorAndAbort(WebIDBDatabaseCallbacks* callbacks,
                        int64_t transaction_id,
                        int32_t code,
                        const base::string16& message) {
  callbacks->onAbort(transaction_id, blink::WebIDBDatabaseError(code, message));
}

}  // namespace

IndexedDBDatabaseCallbacksImpl::IndexedDBDatabaseCallbacksImpl(
    std::unique_ptr<WebIDBDatabaseCallbacks> callbacks)
    : callback_runner_(base::ThreadTaskRunnerHandle::Get()),
      callbacks_(callbacks.release()) {
  IndexedDBDispatcher::ThreadSpecificInstance()
      ->RegisterMojoOwnedDatabaseCallbacks(callbacks_);
}

IndexedDBDatabaseCallbacksImpl::~IndexedDBDatabaseCallbacksImpl() {
  callback_runner_->PostTask(FROM_HERE,
                             base::Bind(&DeleteDatabaseCallbacks, callbacks_));
}

void IndexedDBDatabaseCallbacksImpl::ForcedClose() {
  callback_runner_->PostTask(FROM_HERE,
                             base::Bind(&WebIDBDatabaseCallbacks::onForcedClose,
                                        base::Unretained(callbacks_)));
}

void IndexedDBDatabaseCallbacksImpl::VersionChange(int64_t old_version,
                                                   int64_t new_version) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebIDBDatabaseCallbacks::onVersionChange,
                 base::Unretained(callbacks_), old_version, new_version));
}

void IndexedDBDatabaseCallbacksImpl::Abort(int64_t transaction_id,
                                           int32_t code,
                                           const base::string16& message) {
  // Indirect through BuildErrorAndAbort because it isn't safe to pass a
  // WebIDBDatabaseError between threads.
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(&BuildErrorAndAbort, base::Unretained(callbacks_),
                            transaction_id, code, message));
}

void IndexedDBDatabaseCallbacksImpl::Complete(int64_t transaction_id) {
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(&WebIDBDatabaseCallbacks::onComplete,
                            base::Unretained(callbacks_), transaction_id));
}

}  // namespace content
