// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbfactory_impl.h"

#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/child_thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseCallbacks;
using WebKit::WebString;

namespace content {

RendererWebIDBFactoryImpl::RendererWebIDBFactoryImpl() {
}

RendererWebIDBFactoryImpl::~RendererWebIDBFactoryImpl() {
}

void RendererWebIDBFactoryImpl::getDatabaseNames(
    WebIDBCallbacks* callbacks,
    const WebString& database_identifier,
    const WebString& data_dir_unused) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBFactoryGetDatabaseNames(
      callbacks, database_identifier);
}

void RendererWebIDBFactoryImpl::open(
    const WebString& name,
    long long version,
    long long transaction_id,
    WebIDBCallbacks* callbacks,
    WebIDBDatabaseCallbacks* database_callbacks,
    const WebString& database_identifier,
    const WebString& data_dir) {
  // Don't send the data_dir. We know what we want on the Browser side of
  // things.
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBFactoryOpen(
      name, version, transaction_id, callbacks, database_callbacks,
      database_identifier);
}

void RendererWebIDBFactoryImpl::deleteDatabase(
    const WebString& name,
    WebIDBCallbacks* callbacks,
    const WebString& database_identifier,
    const WebString& data_dir) {
  // Don't send the data_dir. We know what we want on the Browser side of
  // things.
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBFactoryDeleteDatabase(
      name, callbacks, database_identifier);
}

}  // namespace content
