// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_context.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "googleurl/src/url_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebIDBDatabase;
using WebKit::WebIDBFactory;
using WebKit::WebSecurityOrigin;

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
    const string16& database_name,
    const WebSecurityOrigin& origin) const {
  FilePath storage_dir = webkit_context_->data_path().Append(
      kIndexedDBDirectory);
  FilePath::StringType id = webkit_glue::WebStringToFilePathString(
      WebIDBFactory::databaseFileName(database_name, origin));
  return storage_dir.Append(id);
}

// static
bool IndexedDBContext::SplitIndexedDBFileName(
    const FilePath& file_name,
    std::string* database_name,
    WebSecurityOrigin* security_origin) {
  FilePath::StringType base_name =
      file_name.BaseName().RemoveExtension().value();
  size_t db_name_separator = base_name.find(FILE_PATH_LITERAL("@"));
  if (db_name_separator == FilePath::StringType::npos ||
      db_name_separator == 0) {
    return false;
  }

  *security_origin =
      WebSecurityOrigin::createFromDatabaseIdentifier(
          webkit_glue::FilePathStringToWebString(
              base_name.substr(0, db_name_separator)));
#if defined(OS_POSIX)
  std::string name = base_name.substr(db_name_separator + 1);
#elif defined(OS_WIN)
  std::string name = WideToUTF8(base_name.substr(db_name_separator + 1));
#endif
  url_canon::RawCanonOutputT<char16> output;
  url_util::DecodeURLEscapeSequences(name.c_str(), name.length(), &output);
  *database_name = UTF16ToUTF8(string16(output.data(), output.length()));
  return true;
}
