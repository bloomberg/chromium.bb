// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/serialized_script_value.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBTransaction.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBTransactionCallbacks.h"

// Template magic to figure out what message to send to the renderer based on
// which (overloaded) onSuccess method we expect to be called.
template <class Type> struct WebIDBToMsgHelper { };
template <> struct WebIDBToMsgHelper<WebKit::WebIDBDatabase> {
  typedef ViewMsg_IDBCallbacksSuccessIDBDatabase MsgType;
};
template <> struct WebIDBToMsgHelper<WebKit::WebIDBIndex> {
  typedef ViewMsg_IDBCallbacksSuccessIDBIndex MsgType;
};
template <> struct WebIDBToMsgHelper<WebKit::WebIDBObjectStore> {
  typedef ViewMsg_IDBCallbacksSuccessIDBObjectStore MsgType;
};
template <> struct WebIDBToMsgHelper<WebKit::WebIDBTransaction> {
  typedef ViewMsg_IDBCallbacksSuccessIDBTransaction MsgType;
};

// The code the following two classes share.
class IndexedDBCallbacksBase : public WebKit::WebIDBCallbacks {
 public:
  IndexedDBCallbacksBase(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id)
      : dispatcher_host_(dispatcher_host), response_id_(response_id) { }

  virtual void onError(const WebKit::WebIDBDatabaseError& error) {
    dispatcher_host_->Send(new ViewMsg_IDBCallbacksError(
        response_id_, error.code(), error.message()));
  }

 protected:
  IndexedDBDispatcherHost* dispatcher_host() const {
    return dispatcher_host_.get();
  }
  int32 response_id() const { return response_id_; }

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int32 response_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacksBase);
};

// A WebIDBCallbacks implementation that returns an object of WebObjectType.
template <class WebObjectType>
class IndexedDBCallbacks : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id)
      : IndexedDBCallbacksBase(dispatcher_host, response_id) { }

  virtual void onSuccess(WebObjectType* idb_object) {
    int32 object_id = dispatcher_host()->Add(idb_object);
    dispatcher_host()->Send(
        new typename WebIDBToMsgHelper<WebObjectType>::MsgType(response_id(),
                                                               object_id));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

// WebIDBCursor uses onSuccess(WebIDBCursor*) to indicate it has data, and
// onSuccess() without params to indicate it does not contain any data, i.e.,
// there is no key within the key range, or it has reached the end.
template <>
class IndexedDBCallbacks<WebKit::WebIDBCursor>
    : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id)
      : IndexedDBCallbacksBase(dispatcher_host, response_id) { }

  virtual void onSuccess(WebKit::WebIDBCursor* idb_object) {
    int32 object_id = dispatcher_host()->Add(idb_object);
    dispatcher_host()->Send(
        new ViewMsg_IDBCallbacksSuccessIDBCursor(response_id(), object_id));
  }

  virtual void onSuccess() {
    dispatcher_host()->Send(new ViewMsg_IDBCallbacksSuccessNull(response_id()));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

// WebIDBKey is implemented in WebKit as opposed to being an interface Chromium
// implements.  Thus we pass a const ___& version and thus we need this
// specialization.
template <>
class IndexedDBCallbacks<WebKit::WebIDBKey>
    : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id)
      : IndexedDBCallbacksBase(dispatcher_host, response_id) { }

  virtual void onSuccess(const WebKit::WebIDBKey& value) {
    dispatcher_host()->Send(
        new ViewMsg_IDBCallbacksSuccessIndexedDBKey(
            response_id(), IndexedDBKey(value)));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

// WebSerializedScriptValue is implemented in WebKit as opposed to being an
// interface Chromium implements.  Thus we pass a const ___& version and thus
// we need this specialization.
template <>
class IndexedDBCallbacks<WebKit::WebSerializedScriptValue>
    : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id)
      : IndexedDBCallbacksBase(dispatcher_host, response_id) { }

  virtual void onSuccess(const WebKit::WebSerializedScriptValue& value) {
    dispatcher_host()->Send(
        new ViewMsg_IDBCallbacksSuccessSerializedScriptValue(
            response_id(), SerializedScriptValue(value)));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

// A WebIDBCallbacks implementation that doesn't return a result.
template <>
class IndexedDBCallbacks<void> : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id)
      : IndexedDBCallbacksBase(dispatcher_host, response_id) { }

  virtual void onSuccess() {
    dispatcher_host()->Send(
        new ViewMsg_IDBCallbacksSuccessNull(response_id()));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

class IndexedDBTransactionCallbacks
    : public WebKit::WebIDBTransactionCallbacks {
 public:
  IndexedDBTransactionCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                                int transaction_id)
      : dispatcher_host_(dispatcher_host),
        transaction_id_(transaction_id) {
  }

  virtual void onAbort() {
    dispatcher_host_->Send(
        new ViewMsg_IDBTransactionCallbacksAbort(transaction_id_));
  }

  virtual void onComplete() {
    dispatcher_host_->Send(
        new ViewMsg_IDBTransactionCallbacksComplete(transaction_id_));
  }

  virtual void onTimeout() {
    dispatcher_host_->Send(
        new ViewMsg_IDBTransactionCallbacksTimeout(transaction_id_));
  }

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int transaction_id_;
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
