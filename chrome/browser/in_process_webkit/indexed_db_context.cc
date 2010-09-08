// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_context.h"

#include "base/logging.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBFactory.h"

using WebKit::WebIDBDatabase;
using WebKit::WebIDBFactory;

IndexedDBContext::IndexedDBContext() {
}

IndexedDBContext::~IndexedDBContext() {
}

WebIDBFactory* IndexedDBContext::GetIDBFactory() {
  if (!idb_factory_.get())
    idb_factory_.reset(WebIDBFactory::create());
  DCHECK(idb_factory_.get());
  return idb_factory_.get();
}
