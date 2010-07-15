// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbindex_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/indexed_db_dispatcher.h"

using WebKit::WebDOMStringList;
using WebKit::WebString;
using WebKit::WebVector;

RendererWebIDBIndexImpl::RendererWebIDBIndexImpl(int32 idb_index_id)
    : idb_index_id_(idb_index_id) {
}

RendererWebIDBIndexImpl::~RendererWebIDBIndexImpl() {
  // TODO(jorlow): Is it possible for this to be destroyed but still have
  //               pending callbacks?  If so, fix!
  RenderThread::current()->Send(new ViewHostMsg_IDBIndexDestroyed(
      idb_index_id_));
}

WebString RendererWebIDBIndexImpl::name() const {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBIndexName(idb_index_id_, &result));
  return result;
}

WebString RendererWebIDBIndexImpl::keyPath() const {
  NullableString16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBIndexKeyPath(idb_index_id_, &result));
  return result;
}

bool RendererWebIDBIndexImpl::unique() const {
  bool result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBIndexUnique(idb_index_id_, &result));
  return result;
}
