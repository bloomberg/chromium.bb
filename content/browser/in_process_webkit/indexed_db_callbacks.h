// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBCursor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransaction.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

class IndexedDBMsg_CallbacksSuccessIDBDatabase;
class IndexedDBMsg_CallbacksSuccessIDBTransaction;

// Template magic to figure out what message to send to the renderer based on
// which (overloaded) onSuccess method we expect to be called.
template <class Type> struct WebIDBToMsgHelper { };
template <> struct WebIDBToMsgHelper<WebKit::WebIDBDatabase> {
  typedef IndexedDBMsg_CallbacksSuccessIDBDatabase MsgType;
};
template <> struct WebIDBToMsgHelper<WebKit::WebIDBTransaction> {
  typedef IndexedDBMsg_CallbacksSuccessIDBTransaction MsgType;
};

// The code the following two classes share.
class IndexedDBCallbacksBase : public WebKit::WebIDBCallbacks {
 public:
  IndexedDBCallbacksBase(IndexedDBDispatcherHost* dispatcher_host,
                         int32 response_id);

  virtual ~IndexedDBCallbacksBase();

  virtual void onError(const WebKit::WebIDBDatabaseError& error);
  virtual void onBlocked();

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
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id,
      const GURL& origin_url)
      : IndexedDBCallbacksBase(dispatcher_host, response_id),
        origin_url_(origin_url) {
  }

  virtual void onSuccess(WebObjectType* idb_object) {
    int32 object_id = dispatcher_host()->Add(idb_object, origin_url_);
    dispatcher_host()->Send(
        new typename WebIDBToMsgHelper<WebObjectType>::MsgType(response_id(),
                                                               object_id));
  }

 private:
  GURL origin_url_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

// WebIDBCursor uses onSuccess(WebIDBCursor*) when a cursor has been opened,
// onSuccessWithContinuation() when a continue() call has succeeded, or
// onSuccess() without params to indicate it does not contain any data, i.e.,
// there is no key within the key range, or it has reached the end.
template <>
class IndexedDBCallbacks<WebKit::WebIDBCursor>
    : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id,
      int32 cursor_id)
      : IndexedDBCallbacksBase(dispatcher_host, response_id),
        cursor_id_(cursor_id) { }

  virtual void onSuccess(WebKit::WebIDBCursor* idb_object);
  virtual void onSuccess(const WebKit::WebSerializedScriptValue& value);
  virtual void onSuccessWithContinuation();
  virtual void onSuccessWithPrefetch(
      const WebKit::WebVector<WebKit::WebIDBKey>& keys,
      const WebKit::WebVector<WebKit::WebIDBKey>& primaryKeys,
      const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values);

 private:
  // The id of the cursor this callback concerns, or -1 if the cursor
  // does not exist yet.
  int32 cursor_id_;

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

  virtual void onSuccess(const WebKit::WebIDBKey& value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

// WebDOMStringList is implemented in WebKit as opposed to being an
// interface Chromium implements.  Thus we pass a const ___& version and thus
// we need this specialization.
template <>
class IndexedDBCallbacks<WebKit::WebDOMStringList>
    : public IndexedDBCallbacksBase {
 public:
  IndexedDBCallbacks(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id)
      : IndexedDBCallbacksBase(dispatcher_host, response_id) { }

  virtual void onSuccess(const WebKit::WebDOMStringList& value);

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

  virtual void onSuccess(const WebKit::WebSerializedScriptValue& value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
