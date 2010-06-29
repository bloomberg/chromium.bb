// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DATA_DELETER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DATA_DELETER_H_

#include <string>

#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "webkit/database/database_tracker.h"

class Extension;
class Profile;

// A helper class that takes care of removing local storage, databases and
// cookies for a given extension. This is used by
// ExtensionsService::ClearExtensionData() upon uninstalling an extension.
class ExtensionDataDeleter
  : public base::RefCountedThreadSafe<ExtensionDataDeleter,
                                      ChromeThread::DeleteOnUIThread> {
 public:
  ExtensionDataDeleter(Profile* profile, const GURL& extension_url);

  // Start removing data. The extension should not be running when this is
  // called. Cookies are deleted on the current thread, local storage and
  // databases are deleted asynchronously on the webkit and file threads,
  // respectively. This function must be called from the UI thread.
  void StartDeleting();

 private:
  // Deletes the cookies for the extension. May only be called on the io
  // thread.
  void DeleteCookiesOnIOThread();

  // Deletes the database for the extension. May only be called on the file
  // thread.
  void DeleteDatabaseOnFileThread();

  // Deletes local storage for the extension. May only be called on the webkit
  // thread.
  void DeleteLocalStorageOnWebkitThread();

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

  DISALLOW_COPY_AND_ASSIGN(ExtensionDataDeleter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DATA_DELETER_H_
