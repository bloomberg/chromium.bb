// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/webdata/common/webdata_export.h"
#include "content/public/browser/browser_thread.h"
#include "sql/init_status.h"

class WebDatabase;
class WebDatabaseService;
class WebDatabaseTable;

namespace base {
class Thread;
}

// Base for WebDataService class hierarchy.
class WEBDATA_EXPORT WebDataServiceBase {
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

  virtual ~WebDataServiceBase();

  // Cancel any pending request. You need to call this method if your
  // WebDataServiceConsumer is about to be deleted.
  virtual void CancelRequest(Handle h);

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

  // Register a callback to be notified that the database has loaded. Multiple
  // callbacks may be registered, and each will be called at most once
  // (following a successful database load), then cleared.
  // Note: if the database load is already complete, then the callback will NOT
  // be stored or called.
  virtual void RegisterDBLoadedCallback(
      const base::Callback<void(void)>& callback);

  // Returns true if the database load has completetd successfully, and
  // ShutdownOnUIThread has not yet been called.
  virtual bool IsDatabaseLoaded();

  // Returns a pointer to the DB (used by SyncableServices). May return NULL if
  // the database is not loaded or otherwise unavailable. Must be called on
  // DBThread.
  virtual WebDatabase* GetDatabase();

 protected:
  // Our database service.
  scoped_refptr<WebDatabaseService> wdbs_;

 private:
  ProfileErrorCallback profile_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceBase);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_
