// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbdatabase_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/indexed_db_dispatcher.h"
#include "chrome/renderer/renderer_webidbobjectstore_impl.h"
#include "chrome/renderer/renderer_webidbtransaction_impl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

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
  // TODO(jorlow): Is it possible for this to be destroyed but still have
  //               pending callbacks?  If so, fix!
  RenderThread::current()->Send(new ViewHostMsg_IDBDatabaseDestroyed(
      idb_database_id_));
}

WebString RendererWebIDBDatabaseImpl::name() const {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseName(idb_database_id_, &result));
  return result;
}

WebString RendererWebIDBDatabaseImpl::description() const {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseDescription(idb_database_id_, &result));
  return result;
}

WebString RendererWebIDBDatabaseImpl::version() const {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseVersion(idb_database_id_, &result));
  return result;
}

WebDOMStringList RendererWebIDBDatabaseImpl::objectStoreNames() const {
  std::vector<string16> result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseObjectStoreNames(idb_database_id_, &result));
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
  ViewHostMsg_IDBDatabaseCreateObjectStore_Params params;
  params.name_ = name;
  params.key_path_ = key_path;
  params.auto_increment_ = auto_increment;
  params.transaction_id_ = IndexedDBDispatcher::TransactionId(transaction);
  params.idb_database_id_ = idb_database_id_;

  int object_store;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseCreateObjectStore(params, &object_store, &ec));
  if (!object_store)
    return NULL;
  return new RendererWebIDBObjectStoreImpl(object_store);
}

void RendererWebIDBDatabaseImpl::removeObjectStore(
    const WebString& name,
    const WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseRemoveObjectStore(
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
      new ViewHostMsg_IDBDatabaseTransaction(
          idb_database_id_, object_stores, mode,
          timeout, &transaction_id, &ec));
  if (!transaction_id)
    return NULL;
  return new RendererWebIDBTransactionImpl(transaction_id);
}
