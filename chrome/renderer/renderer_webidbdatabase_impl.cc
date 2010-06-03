// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbdatabase_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/indexed_db_dispatcher.h"

using WebKit::WebDOMStringList;
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

WebString RendererWebIDBDatabaseImpl::name() {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseName(idb_database_id_, &result));
  return result;
}

WebString RendererWebIDBDatabaseImpl::description() {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseDescription(idb_database_id_, &result));
  return result;
}

WebString RendererWebIDBDatabaseImpl::version() {
  string16 result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseVersion(idb_database_id_, &result));
  return result;
}

WebDOMStringList RendererWebIDBDatabaseImpl::objectStores() {
  std::vector<string16> result;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseObjectStores(idb_database_id_, &result));
  WebDOMStringList webResult;
  for (std::vector<string16>::const_iterator it = result.begin();
       it != result.end(); ++it) {
    webResult.append(*it);
  }
  return webResult;
}
