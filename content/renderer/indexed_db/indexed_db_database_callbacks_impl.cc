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
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_observation.h"

using blink::WebVector;
using blink::WebIDBDatabaseCallbacks;
using blink::WebIDBObservation;

namespace content {

IndexedDBDatabaseCallbacksImpl::IndexedDBDatabaseCallbacksImpl(
    std::unique_ptr<WebIDBDatabaseCallbacks> callbacks)
    : callbacks_(std::move(callbacks)) {}

IndexedDBDatabaseCallbacksImpl::~IndexedDBDatabaseCallbacksImpl() = default;

void IndexedDBDatabaseCallbacksImpl::ForcedClose() {
  callbacks_->OnForcedClose();
}

void IndexedDBDatabaseCallbacksImpl::VersionChange(int64_t old_version,
                                                   int64_t new_version) {
  callbacks_->OnVersionChange(old_version, new_version);
}

void IndexedDBDatabaseCallbacksImpl::Abort(int64_t transaction_id,
                                           int32_t code,
                                           const base::string16& message) {
  callbacks_->OnAbort(
      transaction_id,
      blink::WebIDBDatabaseError(code, blink::WebString::FromUTF16(message)));
}

void IndexedDBDatabaseCallbacksImpl::Complete(int64_t transaction_id) {
  callbacks_->OnComplete(transaction_id);
}

void IndexedDBDatabaseCallbacksImpl::Changes(
    indexed_db::mojom::ObserverChangesPtr changes) {
  WebVector<WebIDBObservation> web_observations;
  web_observations.reserve(changes->observations.size());
  for (const auto& observation : changes->observations) {
    web_observations.emplace_back(
        observation->object_store_id, observation->type,
        WebIDBKeyRangeBuilder::Build(observation->key_range),
        IndexedDBCallbacksImpl::ConvertValue(observation->value));
  }

  WebIDBDatabaseCallbacks::ObservationIndexMap observation_index_map(
      changes->observation_index_map.begin(),
      changes->observation_index_map.end());

  std::unordered_map<int32_t, std::pair<int64_t, std::vector<int64_t>>>
      observer_transactions;
  for (const auto& transaction_pair : changes->transaction_map) {
    // Moving an int64_t is rather silly. Sadly, std::make_pair's overloads
    // accept either two rvalue arguments, or none.
    observer_transactions[transaction_pair.first] =
        std::make_pair<int64_t, std::vector<int64_t>>(
            std::move(transaction_pair.second->id),
            std::move(transaction_pair.second->scope));
  }

  callbacks_->OnChanges(observation_index_map, std::move(web_observations),
                        observer_transactions);
}

}  // namespace content
