// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_
#define CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "sql/init_status.h"

class WebDatabase;
class WebDatabaseService;
class WebDatabaseTable;

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
  //
  // The ownership of |wdbs| is shared, with the primary owner being the
  // WebDataServiceWrapper, and secondary owners being subclasses of
  // WebDataServiceBase, which receive |wdbs| upon construction. The
  // WebDataServiceWrapper handles the initializing and shutting down and of
  // the |wdbs| object.
  WebDataServiceBase(scoped_refptr<WebDatabaseService> wdbs,
                     const ProfileErrorCallback& callback);

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
  virtual void Init();

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

  // Returns a SupportsUserData objects that may be used to store data
  // owned by the DB thread on this object. Should be called only from
  // the DB thread, and will be destroyed on the DB thread soon after
  // |ShutdownOnUIThread()| is called.
  base::SupportsUserData* GetDBUserData();

 protected:
  virtual ~WebDataServiceBase();
  virtual void ShutdownOnDBThread();

  // Our database service.
  scoped_refptr<WebDatabaseService> wdbs_;

  // True if we've received a notification that the WebDatabase has loaded.
  bool db_loaded_;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WebDataServiceBase>;

  ProfileErrorCallback profile_error_callback_;

  // This makes the destructor public, and thus allows us to aggregate
  // SupportsUserData. It is private by default to prevent incorrect
  // usage in class hierarchies where it is inherited by
  // reference-counted objects.
  class SupportsUserDataAggregatable : public base::SupportsUserData {
   public:
    SupportsUserDataAggregatable() {}
    virtual ~SupportsUserDataAggregatable() {}
   private:
    DISALLOW_COPY_AND_ASSIGN(SupportsUserDataAggregatable);
  };

  // Storage for user data to be accessed only on the DB thread. May
  // be used e.g. for SyncableService subclasses that need to be owned
  // by this object. Is created on first call to |GetDBUserData()|.
  scoped_ptr<SupportsUserDataAggregatable> db_thread_user_data_;

  // Called after database is successfully loaded. By default this function does
  // nothing. Subclasses can override to support notification.
  virtual void NotifyDatabaseLoadedOnUIThread();

  void DBInitFailed(sql::InitStatus sql_status);
  void DBInitSucceeded();
  void DatabaseInitOnDB(sql::InitStatus status);
};

#endif  // CHROME_BROWSER_API_WEBDATA_WEB_DATA_SERVICE_BASE_H_
