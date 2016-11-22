// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_dispatcher.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/indexed_db/webidbcursor_impl.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBObservation.h"

using blink::WebIDBKey;
using blink::WebIDBObservation;
using blink::WebIDBObserver;
using base::ThreadLocalPointer;

namespace content {
static base::LazyInstance<ThreadLocalPointer<IndexedDBDispatcher> >::Leaky
    g_idb_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

namespace {

IndexedDBDispatcher* const kHasBeenDeleted =
    reinterpret_cast<IndexedDBDispatcher*>(0x1);

}  // unnamed namespace

IndexedDBDispatcher::IndexedDBDispatcher() {
  g_idb_dispatcher_tls.Pointer()->Set(this);
}

IndexedDBDispatcher::~IndexedDBDispatcher() {
  in_destructor_ = true;
  mojo_owned_callback_state_.clear();
  mojo_owned_database_callback_state_.clear();

  g_idb_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

IndexedDBDispatcher* IndexedDBDispatcher::ThreadSpecificInstance() {
  if (g_idb_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS IndexedDBDispatcher.";
    g_idb_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_idb_dispatcher_tls.Pointer()->Get())
    return g_idb_dispatcher_tls.Pointer()->Get();

  IndexedDBDispatcher* dispatcher = new IndexedDBDispatcher();
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

void IndexedDBDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

std::vector<WebIDBObservation> IndexedDBDispatcher::ConvertObservations(
    const std::vector<IndexedDBMsg_Observation>& idb_observations) {
  std::vector<WebIDBObservation> web_observations;
  for (const auto& idb_observation : idb_observations) {
    WebIDBObservation web_observation;
    web_observation.objectStoreId = idb_observation.object_store_id;
    web_observation.type = idb_observation.type;
    web_observation.keyRange =
        WebIDBKeyRangeBuilder::Build(idb_observation.key_range);
    // TODO(palakj): Assign value to web_observation.
    web_observations.push_back(std::move(web_observation));
  }
  return web_observations;
}

void IndexedDBDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IndexedDBDispatcher, msg)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksChanges,
                        OnDatabaseChanges)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // If a message gets here, IndexedDBMessageFilter already determined that it
  // is an IndexedDB message.
  DCHECK(handled) << "Didn't handle a message defined at line "
                  << IPC_MESSAGE_ID_LINE(msg.type());
}

int32_t IndexedDBDispatcher::RegisterObserver(
    std::unique_ptr<WebIDBObserver> observer) {
  return observers_.Add(observer.release());
}

void IndexedDBDispatcher::RemoveObservers(
    const std::vector<int32_t>& observer_ids_to_remove) {
  for (int32_t id : observer_ids_to_remove)
    observers_.Remove(id);
}

void IndexedDBDispatcher::RegisterMojoOwnedCallbacks(
    IndexedDBCallbacksImpl::InternalState* callbacks) {
  mojo_owned_callback_state_[callbacks] = base::WrapUnique(callbacks);
}

void IndexedDBDispatcher::UnregisterMojoOwnedCallbacks(
    IndexedDBCallbacksImpl::InternalState* callbacks) {
  if (in_destructor_)
    return;

  auto it = mojo_owned_callback_state_.find(callbacks);
  DCHECK(it != mojo_owned_callback_state_.end());
  it->second.release();
  mojo_owned_callback_state_.erase(it);
}

void IndexedDBDispatcher::RegisterMojoOwnedDatabaseCallbacks(
    blink::WebIDBDatabaseCallbacks* callbacks) {
  mojo_owned_database_callback_state_[callbacks] = base::WrapUnique(callbacks);
}

void IndexedDBDispatcher::UnregisterMojoOwnedDatabaseCallbacks(
    blink::WebIDBDatabaseCallbacks* callbacks) {
  if (in_destructor_)
    return;

  auto it = mojo_owned_database_callback_state_.find(callbacks);
  DCHECK(it != mojo_owned_database_callback_state_.end());
  it->second.release();
  mojo_owned_database_callback_state_.erase(it);
}

void IndexedDBDispatcher::OnDatabaseChanges(
    int32_t ipc_thread_id,
    const IndexedDBMsg_ObserverChanges& changes) {
  DCHECK_EQ(ipc_thread_id, CurrentWorkerId());
  std::vector<WebIDBObservation> observations(
      ConvertObservations(changes.observations));
  for (auto& it : changes.observation_index) {
    WebIDBObserver* observer = observers_.Lookup(it.first);
    // An observer can be removed from the renderer, but still exist in the
    // backend. Moreover, observer might have recorded some changes before being
    // removed from the backend and thus, have its id be present in changes.
    if (!observer)
      continue;
    observer->onChange(observations, std::move(it.second));
  }
}

void IndexedDBDispatcher::RegisterCursor(WebIDBCursorImpl* cursor) {
  DCHECK(!base::ContainsValue(cursors_, cursor));
  cursors_.insert(cursor);
}

void IndexedDBDispatcher::UnregisterCursor(WebIDBCursorImpl* cursor) {
  DCHECK(base::ContainsValue(cursors_, cursor));
  cursors_.erase(cursor);
}

void IndexedDBDispatcher::ResetCursorPrefetchCaches(
    int64_t transaction_id,
    WebIDBCursorImpl* exception_cursor) {
  for (WebIDBCursorImpl* cursor : cursors_) {
    if (cursor != exception_cursor &&
        cursor->transaction_id() == transaction_id)
      cursor->ResetPrefetchCache();
  }
}

}  // namespace content
