// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbdatabase_impl.h"

#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/indexed_db_dispatcher.h"

RendererWebIDBDatabaseImpl::RendererWebIDBDatabaseImpl(int32 idb_database_id)
    : idb_database_id_(idb_database_id) {
}

RendererWebIDBDatabaseImpl::~RendererWebIDBDatabaseImpl() {
  IndexedDBDispatcher* dispatcher =
      RenderThread::current()->indexed_db_dispatcher();
  dispatcher->SendIDBDatabaseDestroyed(idb_database_id_);
}
