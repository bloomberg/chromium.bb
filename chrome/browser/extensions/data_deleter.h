// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DATA_DELETER_H_
#define CHROME_BROWSER_EXTENSIONS_DATA_DELETER_H_

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/string16.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace appcache {
class AppCacheService;
}

namespace content {
class DOMStorageContext;
class IndexedDBContext;
}

namespace fileapi {
class FileSystemContext;
}

namespace net {
class URLRequestContextGetter;
}

namespace webkit_database {
class DatabaseTracker;
}

class Profile;

namespace extensions {

// A helper class that takes care of removing local storage, databases and
// cookies for a given extension. This is used by
// ExtensionService::ClearExtensionData() upon uninstalling an extension.
class DataDeleter : public base::RefCountedThreadSafe<
    DataDeleter, content::BrowserThread::DeleteOnUIThread> {
 public:
  // Starts removing data. The extension should not be running when this is
  // called. Cookies are deleted on the current thread, local storage and
  // databases/settings are deleted asynchronously on the webkit and file
  // threads, respectively. This function must be called from the UI thread.
  static void StartDeleting(
      Profile* profile,
      const std::string& extension_id,
      const GURL& storage_origin,
      bool is_storage_isolated);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DataDeleter>;

  DataDeleter(Profile* profile,
              const std::string& extension_id,
              const GURL& storage_origin,
              bool is_storage_isolated);
  ~DataDeleter();

  // Deletes the cookies for the extension. May only be called on the io
  // thread.
  void DeleteCookiesOnIOThread();

  // Deletes the database for the extension. May only be called on the file
  // thread.
  void DeleteDatabaseOnFileThread();

  // Deletes indexed db files for the extension. May only be called on the
  // webkit thread.
  void DeleteIndexedDBOnWebkitThread();

  // Deletes filesystem files for the extension. May only be called on the
  // file thread.
  void DeleteFileSystemOnFileThread();

  // Deletes appcache files for the extension. May only be called on the IO
  // thread.
  void DeleteAppcachesOnIOThread(appcache::AppCacheService* appcache_service);

  // The ID of the extension being deleted.
  const std::string extension_id_;

  // The database context for deleting the database.
  scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;

  // Provides access to the request context.
  scoped_refptr<net::URLRequestContextGetter> extension_request_context_;

  // The origin of the extension/app for which we're going to clear data.
  GURL storage_origin_;

  // The security origin identifier for which we're deleting stuff.
  string16 origin_id_;

  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  scoped_refptr<content::IndexedDBContext> indexed_db_context_;

  // If non-empty, the extension we're deleting is an isolated app, and this
  // is its directory which we should delete.
  FilePath isolated_app_path_;

  DISALLOW_COPY_AND_ASSIGN(DataDeleter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DATA_DELETER_H_
