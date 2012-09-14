// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbfactory_impl.h"

#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/child_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMStringList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

using WebKit::WebDOMStringList;
using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseCallbacks;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;

RendererWebIDBFactoryImpl::RendererWebIDBFactoryImpl() {
}

RendererWebIDBFactoryImpl::~RendererWebIDBFactoryImpl() {
}

void RendererWebIDBFactoryImpl::getDatabaseNames(
    WebIDBCallbacks* callbacks,
    const WebSecurityOrigin& origin,
    WebFrame* web_frame,
    const WebString& data_dir_unused) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBFactoryGetDatabaseNames(
      callbacks, origin.databaseIdentifier(), web_frame);
}

void RendererWebIDBFactoryImpl::open(
    const WebString& name,
    long long version,
    WebIDBCallbacks* callbacks,
    WebIDBDatabaseCallbacks* database_callbacks,
    const WebSecurityOrigin& origin,
    WebFrame* web_frame,
    const WebString& data_dir) {
  // Don't send the data_dir. We know what we want on the Browser side of
  // things.
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBFactoryOpen(
      name, version, callbacks, database_callbacks, origin.databaseIdentifier(),
      web_frame);
}

void RendererWebIDBFactoryImpl::deleteDatabase(
    const WebString& name,
    WebIDBCallbacks* callbacks,
    const WebSecurityOrigin& origin,
    WebFrame* web_frame,
    const WebString& data_dir) {
  // Don't send the data_dir. We know what we want on the Browser side of
  // things.
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBFactoryDeleteDatabase(
      name, callbacks, origin.databaseIdentifier(), web_frame);
}
