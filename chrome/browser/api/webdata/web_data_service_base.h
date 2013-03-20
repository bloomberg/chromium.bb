// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_
#define CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "sql/init_status.h"

class WebDatabase;
class WebDatabaseService;

namespace base {
class Thread;
}

// Base for WebDataService class hierarchy.
class WebDataServiceBase
    : public base::RefCountedThreadSafe<WebDataServiceBase,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  // All requests return an opaque handle of the following type.
  typedef int Handle;

  // Users of this class may provide a callback to handle errors
  // (e.g. by showing a UI). The callback is called only on error, and
  // takes a single parameter, the sql::InitStatus value from trying
  // to open the database.
  // TODO(joi): Should we combine this with WebDatabaseService::InitCallback?
  typedef base::Callback<void(sql::InitStatus)> ProfileErrorCallback;

  // |callback| will only be invoked on error, and only if
  // |callback.is_null()| evaluates to false.
  explicit WebDataServiceBase(const ProfileErrorCallback& callback);

  // Cancel any pending request. You need to call this method if your
  // WebDataServiceConsumer is about to be deleted.
  virtual void CancelRequest(Handle h);

  // Returns the notification source for this service. This may use a
  // pointer other than this object's |this| pointer.
  virtual content::NotificationSource GetNotificationSource();

  // Shutdown the web data service. The service can no longer be used after this
  // call.
  virtual void ShutdownOnUIThread();

  // Initializes the web data service.
  virtual void Init(const base::FilePath& path);

  // Unloads the database without actually shutting down the service.  This can
  // be used to temporarily reduce the browser process' memory footprint.
  void UnloadDatabase();

  // Unloads the database permanently and shuts down service.
  void ShutdownDatabase();

  // Returns true if the database load has completetd successfully, and
  // ShutdownOnUIThread has not yet been called.
  virtual bool IsDatabaseLoaded();

  // Returns a pointer to the DB (used by SyncableServices). May return NULL if
  // the database is not loaded or otherwise unavailable. Must be called on
  // DBThread.
  virtual WebDatabase* GetDatabase();

 protected:
  virtual ~WebDataServiceBase();

  // Our database service.
  scoped_ptr<WebDatabaseService> wdbs_;

  // True if we've received a notification that the WebDatabase has loaded.
  bool db_loaded_;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WebDataServiceBase>;

  ProfileErrorCallback profile_error_callback_;

  void DBInitFailed(sql::InitStatus sql_status);
  void NotifyDatabaseLoadedOnUIThread();
  void DatabaseInitOnDB(sql::InitStatus status);
};

#endif  // CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_
