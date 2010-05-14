// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_context.h"

#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIndexedDatabase.h"

using WebKit::WebIDBDatabase;
using WebKit::WebIndexedDatabase;

IndexedDBContext::IndexedDBContext() {
}

IndexedDBContext::~IndexedDBContext() {
}

WebIndexedDatabase* IndexedDBContext::GetIndexedDatabase() {
  if (!indexed_database_.get())
    indexed_database_.reset(WebIndexedDatabase::create());
  DCHECK(indexed_database_.get());
  return indexed_database_.get();
}
