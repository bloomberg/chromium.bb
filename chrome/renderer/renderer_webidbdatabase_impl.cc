// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbdatabase_impl.h"

#include "chrome/common/indexed_db_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/indexed_db_dispatcher.h"
#include "chrome/renderer/renderer_webidbobjectstore_impl.h"
#include "chrome/renderer/renderer_webidbtransaction_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"

using WebKit::WebDOMStringList;
using WebKit::WebExceptionCode;
using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBTransaction;
using WebKit::WebString;
using WebKit::WebVector;

RendererWebIDBDatabaseImpl::RendererWebIDBDatabaseImpl(int32 idb_database_id)
    : idb_database_id_(idb_database_id) {
}

RendererWebIDBDatabaseImpl::~RendererWebIDBDatabaseImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object which owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  RenderThread::current()->Send(new IndexedDBHostMsg_DatabaseDestroyed(
      idb_database_id_));
}

WebString RendererWebIDBDatabaseImpl::name() const {
  string16 result;
  RenderThread::current()->Send(
      new IndexedDBHostMsg_DatabaseName(idb_database_id_, &result));
  return result;
}

WebString RendererWebIDBDatabaseImpl::version() const {
  string16 result;
  RenderThread::current()->Send(
      new IndexedDBHostMsg_DatabaseVersion(idb_database_id_, &result));
  return result;
}

WebDOMStringList RendererWebIDBDatabaseImpl::objectStoreNames() const {
  std::vector<string16> result;
  RenderThread::current()->Send(
      new IndexedDBHostMsg_DatabaseObjectStoreNames(idb_database_id_, &result));
  WebDOMStringList webResult;
  for (std::vector<string16>::const_iterator it = result.begin();
       it != result.end(); ++it) {
    webResult.append(*it);
  }
  return webResult;
}

WebKit::WebIDBObjectStore* RendererWebIDBDatabaseImpl::createObjectStore(
    const WebKit::WebString& name,
    const WebKit::WebString& key_path,
    bool auto_increment,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBHostMsg_DatabaseCreateObjectStore_Params params;
  params.name = name;
  params.key_path = key_path;
  params.auto_increment = auto_increment;
  params.transaction_id = IndexedDBDispatcher::TransactionId(transaction);
  params.idb_database_id = idb_database_id_;

  int object_store;
  RenderThread::current()->Send(
      new IndexedDBHostMsg_DatabaseCreateObjectStore(
          params, &object_store, &ec));
  if (!object_store)
    return NULL;
  return new RendererWebIDBObjectStoreImpl(object_store);
}

void RendererWebIDBDatabaseImpl::deleteObjectStore(
    const WebString& name,
    const WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  RenderThread::current()->Send(
      new IndexedDBHostMsg_DatabaseDeleteObjectStore(
          idb_database_id_, name,
          IndexedDBDispatcher::TransactionId(transaction), &ec));
}

void RendererWebIDBDatabaseImpl::setVersion(
    const WebString& version,
    WebIDBCallbacks* callbacks,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      RenderThread::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBDatabaseSetVersion(
      version, callbacks, idb_database_id_, &ec);
}

WebKit::WebIDBTransaction* RendererWebIDBDatabaseImpl::transaction(
    const WebDOMStringList& names,
    unsigned short mode,
    unsigned long timeout,
    WebExceptionCode& ec) {
  std::vector<string16> object_stores;
  object_stores.reserve(names.length());
  for (unsigned int i = 0; i < names.length(); ++i)
    object_stores.push_back(names.item(i));

  int transaction_id;
  RenderThread::current()->Send(
      new IndexedDBHostMsg_DatabaseTransaction(
          idb_database_id_, object_stores, mode,
          timeout, &transaction_id, &ec));
  if (!transaction_id)
    return NULL;
  return new RendererWebIDBTransactionImpl(transaction_id);
}

void RendererWebIDBDatabaseImpl::close() {
  RenderThread::current()->Send(
      new IndexedDBHostMsg_DatabaseClose(idb_database_id_));
}
