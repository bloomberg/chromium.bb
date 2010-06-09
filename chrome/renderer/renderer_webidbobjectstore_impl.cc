// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbobjectstore_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/indexed_db_dispatcher.h"
#include "chrome/renderer/render_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebString;

RendererWebIDBObjectStoreImpl::RendererWebIDBObjectStoreImpl(
    int32 idb_object_store_id)
    : idb_object_store_id_(idb_object_store_id) {
}

RendererWebIDBObjectStoreImpl::~RendererWebIDBObjectStoreImpl() {
  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreDestroyed(idb_object_store_id_));
}

WebString RendererWebIDBObjectStoreImpl::name() const {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreName(idb_object_store_id_, &result));
  return result;
}

WebString RendererWebIDBObjectStoreImpl::keyPath() const {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreKeyPath(idb_object_store_id_, &result));
  return result;
}
