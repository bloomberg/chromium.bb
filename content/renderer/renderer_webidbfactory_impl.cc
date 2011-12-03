// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webidbfactory_impl.h"

#include "content/renderer/render_thread_impl.h"
#include "content/renderer/indexed_db_dispatcher.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMStringList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

using WebKit::WebDOMStringList;
using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
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
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBFactoryGetDatabaseNames(
      callbacks, origin.databaseIdentifier(), web_frame);
}

void RendererWebIDBFactoryImpl::open(
    const WebString& name,
    WebIDBCallbacks* callbacks,
    const WebSecurityOrigin& origin,
    WebFrame* web_frame,
    const WebString& data_dir) {
  // Don't send the data_dir. We know what we want on the Browser side of
  // things.
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBFactoryOpen(
      name, callbacks, origin.databaseIdentifier(), web_frame);
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
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBFactoryDeleteDatabase(
      name, callbacks, origin.databaseIdentifier(), web_frame);
}
