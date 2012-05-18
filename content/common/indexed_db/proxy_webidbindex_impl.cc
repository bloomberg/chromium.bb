// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbindex_impl.h"

#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/proxy_webidbtransaction_impl.h"
#include "content/common/child_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

using content::IndexedDBKeyPath;
using content::IndexedDBKeyRange;
using WebKit::WebExceptionCode;
using WebKit::WebDOMStringList;
using WebKit::WebIDBKeyPath;
using WebKit::WebString;
using WebKit::WebVector;

RendererWebIDBIndexImpl::RendererWebIDBIndexImpl(int32 idb_index_id)
    : idb_index_id_(idb_index_id) {
}

RendererWebIDBIndexImpl::~RendererWebIDBIndexImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object wich owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_IndexDestroyed(
      idb_index_id_));
}

WebString RendererWebIDBIndexImpl::name() const {
  string16 result;
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_IndexName(idb_index_id_, &result));
  return result;
}

WebIDBKeyPath RendererWebIDBIndexImpl::keyPath() const {
  IndexedDBKeyPath result;
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_IndexKeyPath(idb_index_id_, &result));
  return result;
}

bool RendererWebIDBIndexImpl::unique() const {
  bool result;
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_IndexUnique(idb_index_id_, &result));
  return result;
}

bool RendererWebIDBIndexImpl::multiEntry() const {
  bool result;
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_IndexMultiEntry(idb_index_id_, &result));
  return result;
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
      range, direction, callbacks,  idb_index_id_, transaction, &ec);
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
      range, direction, callbacks,  idb_index_id_, transaction, &ec);
}

void RendererWebIDBIndexImpl::count(
    const WebKit::WebIDBKeyRange& range,
    WebKit::WebIDBCallbacks* callbacks,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBIndexCount(
      range, callbacks, idb_index_id_, transaction, &ec);
}

void RendererWebIDBIndexImpl::getObject(
    const WebKit::WebIDBKeyRange& key_range,
    WebKit::WebIDBCallbacks* callbacks,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBIndexGetObject(
      IndexedDBKeyRange(key_range), callbacks, idb_index_id_,
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
      IndexedDBKeyRange(key_range), callbacks, idb_index_id_,
      transaction, &ec);
}
