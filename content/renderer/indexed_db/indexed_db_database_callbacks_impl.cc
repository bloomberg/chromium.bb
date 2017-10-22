// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/indexed_db/indexed_db_database_callbacks_impl.h"

#include <unordered_map>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/indexed_db/indexed_db_callbacks_impl.h"
#include "content/renderer/indexed_db/indexed_db_dispatcher.h"
#include "content/renderer/indexed_db/indexed_db_key_builders.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBObservation.h"

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
  callbacks->OnAbort(
      transaction_id,
      blink::WebIDBDatabaseError(code, blink::WebString::FromUTF16(message)));
}

void BuildObservationsAndNotify(WebIDBDatabaseCallbacks* callbacks,
                                indexed_db::mojom::ObserverChangesPtr changes) {
  std::vector<blink::WebIDBObservation> web_observations;
  for (const auto& observation : changes->observations) {
    blink::WebIDBObservation web_observation;
    web_observation.object_store_id = observation->object_store_id;
    web_observation.type = observation->type;
    web_observation.key_range =
        WebIDBKeyRangeBuilder::Build(observation->key_range);
    if (observation->value) {
      IndexedDBCallbacksImpl::ConvertValue(observation->value,
                                           &web_observation.value);
    }
    web_observations.push_back(std::move(web_observation));
  }
  std::unordered_map<int32_t, std::pair<int64_t, std::vector<int64_t>>>
      observer_transactions;

  for (const auto& transaction_pair : changes->transaction_map) {
    std::pair<int64_t, std::vector<int64_t>>& obs_txn =
        observer_transactions[transaction_pair.first];
    obs_txn.first = transaction_pair.second->id;
    for (int64_t scope : transaction_pair.second->scope) {
      obs_txn.second.push_back(scope);
    }
  }

  callbacks->OnChanges(changes->observation_index_map, web_observations,
                       observer_transactions);
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
  callback_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteDatabaseCallbacks, callbacks_));
}

void IndexedDBDatabaseCallbacksImpl::ForcedClose() {
  callback_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebIDBDatabaseCallbacks::OnForcedClose,
                                base::Unretained(callbacks_)));
}

void IndexedDBDatabaseCallbacksImpl::VersionChange(int64_t old_version,
                                                   int64_t new_version) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebIDBDatabaseCallbacks::OnVersionChange,
                     base::Unretained(callbacks_), old_version, new_version));
}

void IndexedDBDatabaseCallbacksImpl::Abort(int64_t transaction_id,
                                           int32_t code,
                                           const base::string16& message) {
  // Indirect through BuildErrorAndAbort because it isn't safe to pass a
  // WebIDBDatabaseError between threads.
  callback_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BuildErrorAndAbort, base::Unretained(callbacks_),
                     transaction_id, code, message));
}

void IndexedDBDatabaseCallbacksImpl::Complete(int64_t transaction_id) {
  callback_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebIDBDatabaseCallbacks::OnComplete,
                                base::Unretained(callbacks_), transaction_id));
}

void IndexedDBDatabaseCallbacksImpl::Changes(
    indexed_db::mojom::ObserverChangesPtr changes) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BuildObservationsAndNotify, base::Unretained(callbacks_),
                     base::Passed(&changes)));
}

}  // namespace content
