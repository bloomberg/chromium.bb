// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_context.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebIDBDatabase;
using WebKit::WebIDBFactory;

const FilePath::CharType IndexedDBContext::kIndexedDBDirectory[] =
    FILE_PATH_LITERAL("IndexedDB");

const FilePath::CharType IndexedDBContext::kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".indexeddb");

IndexedDBContext::IndexedDBContext(WebKitContext* webkit_context)
    : webkit_context_(webkit_context) {
}

IndexedDBContext::~IndexedDBContext() {
}

WebIDBFactory* IndexedDBContext::GetIDBFactory() {
  if (!idb_factory_.get())
    idb_factory_.reset(WebIDBFactory::create());
  DCHECK(idb_factory_.get());
  return idb_factory_.get();
}

FilePath IndexedDBContext::GetIndexedDBFilePath(
    const string16& origin_id) const {
  FilePath storage_dir = webkit_context_->data_path().Append(
      kIndexedDBDirectory);
  FilePath::StringType id = webkit_glue::WebStringToFilePathString(origin_id);
  return storage_dir.Append(id.append(kIndexedDBExtension));
}
