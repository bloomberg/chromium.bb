// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webindexeddatabase_impl.h"

#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/indexed_db_dispatcher.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;

RendererWebIndexedDatabaseImpl::RendererWebIndexedDatabaseImpl() {
}

RendererWebIndexedDatabaseImpl::~RendererWebIndexedDatabaseImpl() {
}

void RendererWebIndexedDatabaseImpl::open(
    const WebString& name, const WebString& description, bool modify_database,
    WebIDBCallbacks* callbacks, const WebSecurityOrigin& origin,
    WebFrame* web_frame, int& exception_code) {
  IndexedDBDispatcher* dispatcher =
      RenderThread::current()->indexed_db_dispatcher();
  dispatcher->RequestIndexedDatabaseOpen(
      name, description, modify_database, callbacks,
      origin.databaseIdentifier(), web_frame, &exception_code);
}
