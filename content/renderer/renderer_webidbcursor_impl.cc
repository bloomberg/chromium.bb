// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webidbcursor_impl.h"

#include "content/common/indexed_db_messages.h"
#include "content/renderer/indexed_db_dispatcher.h"
#include "content/renderer/render_thread_impl.h"

using WebKit::WebExceptionCode;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBKey;
using WebKit::WebSerializedScriptValue;

RendererWebIDBCursorImpl::RendererWebIDBCursorImpl(int32 idb_cursor_id)
    : idb_cursor_id_(idb_cursor_id),
      continue_count_(0),
      used_prefetches_(0),
      pending_onsuccess_callbacks_(0),
      prefetch_amount_(kMinPrefetchAmount) {
}

RendererWebIDBCursorImpl::~RendererWebIDBCursorImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object wich owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  ChildThread::current()->Send(new IndexedDBHostMsg_CursorDestroyed(
      idb_cursor_id_));
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->CursorDestroyed(idb_cursor_id_);
}

unsigned short RendererWebIDBCursorImpl::direction() const {
  int direction;
  ChildThread::current()->Send(
      new IndexedDBHostMsg_CursorDirection(idb_cursor_id_, &direction));
  return direction;
}

WebIDBKey RendererWebIDBCursorImpl::key() const {
  return key_;
}

WebIDBKey RendererWebIDBCursorImpl::primaryKey() const {
  return primary_key_;
}

WebSerializedScriptValue RendererWebIDBCursorImpl::value() const {
  return value_;
}

void RendererWebIDBCursorImpl::update(const WebSerializedScriptValue& value,
                                      WebIDBCallbacks* callbacks,
                                      WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorUpdate(
      content::SerializedScriptValue(value), callbacks, idb_cursor_id_, &ec);
}

void RendererWebIDBCursorImpl::continueFunction(const WebIDBKey& key,
                                                WebIDBCallbacks* callbacks,
                                                WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();

  if (key.type() == WebIDBKey::InvalidType) {
    // No key, so this would qualify for a prefetch.
    ++continue_count_;

    if (!prefetch_keys_.empty()) {
      // We have a prefetch cache, so serve the result from that.
      CachedContinue(callbacks);
      return;
    }

    if (continue_count_ > kPrefetchContinueThreshold) {
      // Request pre-fetch.
      dispatcher->RequestIDBCursorPrefetch(prefetch_amount_, callbacks,
                                           idb_cursor_id_, &ec);

      // Increase prefetch_amount_ exponentially.
      prefetch_amount_ *= 2;
      if (prefetch_amount_ > kMaxPrefetchAmount)
        prefetch_amount_ = kMaxPrefetchAmount;

      return;
    }
  } else {
    // Key argument supplied. We couldn't prefetch this.
    ResetPrefetchCache();
  }

  dispatcher->RequestIDBCursorContinue(IndexedDBKey(key), callbacks,
                                       idb_cursor_id_, &ec);
}

void RendererWebIDBCursorImpl::deleteFunction(WebIDBCallbacks* callbacks,
                                              WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorDelete(callbacks, idb_cursor_id_, &ec);
}

void RendererWebIDBCursorImpl::postSuccessHandlerCallback()
{
  pending_onsuccess_callbacks_--;

  // If the onsuccess callback called continue() on the cursor again,
  // and that continue was served by the prefetch cache, then
  // pending_onsuccess_callbacks_ would be incremented.
  // If not, it means the callback did something else, or nothing at all,
  // in which case we need to reset the cache.

  if (pending_onsuccess_callbacks_ == 0)
    ResetPrefetchCache();
}

void RendererWebIDBCursorImpl::SetKeyAndValue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    const content::SerializedScriptValue& value) {
  key_ = key;
  primary_key_ = primary_key;
  value_ = value;
}

void RendererWebIDBCursorImpl::SetPrefetchData(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    const std::vector<content::SerializedScriptValue>& values) {
  prefetch_keys_.assign(keys.begin(), keys.end());
  prefetch_primary_keys_.assign(primary_keys.begin(), primary_keys.end());
  prefetch_values_.assign(values.begin(), values.end());

  used_prefetches_ = 0;
  pending_onsuccess_callbacks_ = 0;
}

void RendererWebIDBCursorImpl::CachedContinue(
    WebKit::WebIDBCallbacks* callbacks) {
  DCHECK(prefetch_keys_.size() > 0);
  DCHECK(prefetch_primary_keys_.size() == prefetch_keys_.size());
  DCHECK(prefetch_values_.size() == prefetch_keys_.size());

  key_ = prefetch_keys_.front();
  primary_key_ = prefetch_primary_keys_.front();
  value_ = prefetch_values_.front();

  prefetch_keys_.pop_front();
  prefetch_primary_keys_.pop_front();
  prefetch_values_.pop_front();
  used_prefetches_++;

  pending_onsuccess_callbacks_++;
  callbacks->onSuccessWithContinuation();
}

void RendererWebIDBCursorImpl::ResetPrefetchCache() {
  continue_count_ = 0;
  prefetch_amount_ = kMinPrefetchAmount;

  if (!prefetch_keys_.size()) {
    // No prefetch cache, so no need to reset the cursor in the back-end.
    return;
  }

  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorPrefetchReset(used_prefetches_,
                                            prefetch_keys_.size(),
                                            idb_cursor_id_);
  prefetch_keys_.clear();
  prefetch_primary_keys_.clear();
  prefetch_values_.clear();

  pending_onsuccess_callbacks_ = 0;
}
