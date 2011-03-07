// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DATA_DELETER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DATA_DELETER_H_
#pragma once

#include "base/ref_counted.h"
#include "base/string16.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace webkit_database {
class DatabaseTracker;
}

namespace fileapi {
class FileSystemContext;
}

class Profile;
class URLRequestContextGetter;
class WebKitContext;

// A helper class that takes care of removing local storage, databases and
// cookies for a given extension. This is used by
// ExtensionService::ClearExtensionData() upon uninstalling an extension.
class ExtensionDataDeleter
  : public base::RefCountedThreadSafe<ExtensionDataDeleter,
                                      BrowserThread::DeleteOnUIThread> {
 public:
  ExtensionDataDeleter(Profile* profile, const GURL& extension_url);

  // Start removing data. The extension should not be running when this is
  // called. Cookies are deleted on the current thread, local storage and
  // databases are deleted asynchronously on the webkit and file threads,
  // respectively. This function must be called from the UI thread.
  void StartDeleting();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<ExtensionDataDeleter>;

  ~ExtensionDataDeleter();

  // Deletes the cookies for the extension. May only be called on the io
  // thread.
  void DeleteCookiesOnIOThread();

  // Deletes the database for the extension. May only be called on the file
  // thread.
  void DeleteDatabaseOnFileThread();

  // Deletes local storage for the extension. May only be called on the webkit
  // thread.
  void DeleteLocalStorageOnWebkitThread();

  // Deletes indexed db files for the extension. May only be called on the
  // webkit thread.
  void DeleteIndexedDBOnWebkitThread();

  // Deletes filesystem files for the extension. May only be called on the
  // file thread.
  void DeleteFileSystemOnFileThread();

  // The database context for deleting the database.
  scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;

  // Provides access to the extension request context.
  scoped_refptr<URLRequestContextGetter> extension_request_context_;

  // The URL of the extension we're removing data for.
  GURL extension_url_;

  // The security origin identifier for which we're deleting stuff.
  string16 origin_id_;

  // Webkit context for accessing the DOM storage helper.
  scoped_refptr<WebKitContext> webkit_context_;

  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDataDeleter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DATA_DELETER_H_
