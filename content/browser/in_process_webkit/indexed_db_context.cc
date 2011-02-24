// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_context.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebIDBDatabase;
using WebKit::WebIDBFactory;
using WebKit::WebSecurityOrigin;

namespace {

void ClearLocalState(const FilePath& indexeddb_path,
                     const char* url_scheme_to_be_skipped) {
  file_util::FileEnumerator file_enumerator(
      indexeddb_path, false, file_util::FileEnumerator::FILES);
  // TODO(pastarmovj): We might need to consider exchanging this loop for
  // something more efficient in the future.
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() != IndexedDBContext::kIndexedDBExtension)
      continue;
    WebSecurityOrigin origin =
        WebSecurityOrigin::createFromDatabaseIdentifier(
            webkit_glue::FilePathToWebString(file_path.BaseName()));
    if (!EqualsASCII(origin.protocol(), url_scheme_to_be_skipped))
      file_util::Delete(file_path, false);
  }
}

}  // namespace

const FilePath::CharType IndexedDBContext::kIndexedDBDirectory[] =
    FILE_PATH_LITERAL("IndexedDB");

const FilePath::CharType IndexedDBContext::kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".indexeddb");

IndexedDBContext::IndexedDBContext(WebKitContext* webkit_context)
    : clear_local_state_on_exit_(false) {
  data_path_ = webkit_context->data_path().Append(kIndexedDBDirectory);
}

IndexedDBContext::~IndexedDBContext() {
  // Not being on the WEBKIT thread here means we are running in a unit test
  // where no clean up is needed.
  if (clear_local_state_on_exit_ &&
      BrowserThread::CurrentlyOn(BrowserThread::WEBKIT)) {
    ClearLocalState(data_path_, chrome::kExtensionScheme);
  }
}

WebIDBFactory* IndexedDBContext::GetIDBFactory() {
  if (!idb_factory_.get())
    idb_factory_.reset(WebIDBFactory::create());
  DCHECK(idb_factory_.get());
  return idb_factory_.get();
}

FilePath IndexedDBContext::GetIndexedDBFilePath(
    const string16& origin_id) const {
  FilePath::StringType id = webkit_glue::WebStringToFilePathString(origin_id);
  return data_path_.Append(id.append(kIndexedDBExtension));
}

void IndexedDBContext::DeleteIndexedDBFile(const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  // TODO(pastarmovj): Close all database connections that use that file.
  file_util::Delete(file_path, false);
}

void IndexedDBContext::DeleteIndexedDBForOrigin(const string16& origin_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  // TODO(pastarmovj): Remove this check once we are safe to delete any time.
  FilePath idb_file = GetIndexedDBFilePath(origin_id);
  DCHECK_EQ(idb_file.BaseName().value().substr(0, strlen("chrome-extension")),
            FILE_PATH_LITERAL("chrome-extension"));
  DeleteIndexedDBFile(GetIndexedDBFilePath(origin_id));
}
