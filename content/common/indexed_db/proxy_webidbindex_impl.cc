// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbindex_impl.h"

#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/proxy_webidbtransaction_impl.h"
#include "content/common/child_thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"

using WebKit::WebExceptionCode;
using WebKit::WebDOMStringList;
using WebKit::WebIDBKeyPath;
using WebKit::WebString;
using WebKit::WebVector;

namespace content {

RendererWebIDBIndexImpl::RendererWebIDBIndexImpl(int32 ipc_index_id)
    : ipc_index_id_(ipc_index_id) {
}

RendererWebIDBIndexImpl::~RendererWebIDBIndexImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object wich owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_IndexDestroyed(
      ipc_index_id_));
}

void RendererWebIDBIndexImpl::openObjectCursor(
    const WebKit::WebIDBKeyRange& range,
    unsigned short direction,
    WebKit::WebIDBCallbacks* callbacks,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBIndexOpenObjectCursor(
      range, direction, callbacks,  ipc_index_id_, transaction, &ec);
}

void RendererWebIDBIndexImpl::openKeyCursor(
    const WebKit::WebIDBKeyRange& range,
    unsigned short direction,
    WebKit::WebIDBCallbacks* callbacks,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBIndexOpenKeyCursor(
      range, direction, callbacks,  ipc_index_id_, transaction, &ec);
}

void RendererWebIDBIndexImpl::count(
    const WebKit::WebIDBKeyRange& range,
    WebKit::WebIDBCallbacks* callbacks,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBIndexCount(
      range, callbacks, ipc_index_id_, transaction, &ec);
}

void RendererWebIDBIndexImpl::getObject(
    const WebKit::WebIDBKeyRange& key_range,
    WebKit::WebIDBCallbacks* callbacks,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBIndexGetObject(
      IndexedDBKeyRange(key_range), callbacks, ipc_index_id_,
      transaction, &ec);
}

void RendererWebIDBIndexImpl::getKey(
    const WebKit::WebIDBKeyRange& key_range,
    WebKit::WebIDBCallbacks* callbacks,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBIndexGetKey(
      IndexedDBKeyRange(key_range), callbacks, ipc_index_id_,
      transaction, &ec);
}

}  // namespace content
