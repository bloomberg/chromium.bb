// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "components/webdata/common/webdata_export.h"
#include "sql/init_status.h"

class WebDatabase;
class WebDatabaseService;

namespace base {
class SingleThreadTaskRunner;
}

// Base for WebDataService class hierarchy.
// WebDataServiceBase is destroyed on the UI thread.
class WEBDATA_EXPORT WebDataServiceBase
    : public base::RefCountedDeleteOnSequence<WebDataServiceBase> {
 public:
  // All requests return an opaque handle of the following type.
  typedef int Handle;

  // Users of this class may provide a callback to handle errors
  // (e.g. by showing a UI). The callback is called only on error, and
  // takes a single parameter, the sql::InitStatus value from trying
  // to open the database.
  // TODO(joi): Should we combine this with WebDatabaseService::InitCallback?
  typedef base::Callback<void(sql::InitStatus, const std::string&)>
      ProfileErrorCallback;

  typedef base::Closure DBLoadedCallback;

  // |callback| will only be invoked on error, and only if
  // |callback.is_null()| evaluates to false.
  //
  // The ownership of |wdbs| is shared, with the primary owner being the
  // WebDataServiceWrapper, and secondary owners being subclasses of
  // WebDataServiceBase, which receive |wdbs| upon construction. The
  // WebDataServiceWrapper handles the initializing and shutting down and of
  // the |wdbs| object.
  // WebDataServiceBase is destroyed on |ui_thread|.
  WebDataServiceBase(
      scoped_refptr<WebDatabaseService> wdbs,
      const ProfileErrorCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread);

  // Cancel any pending request. You need to call this method if your
  // WebDataServiceConsumer is about to be deleted.
  virtual void CancelRequest(Handle h);

  // Shutdown the web data service. The service can no longer be used after this
  // call.
  virtual void ShutdownOnUIThread();

  // Initializes the web data service.
  virtual void Init();

  // Unloads the database and shuts down service.
  void ShutdownDatabase();

  // Register a callback to be notified that the database has loaded. Multiple
  // callbacks may be registered, and each will be called at most once
  // (following a successful database load), then cleared.
  // Note: if the database load is already complete, then the callback will NOT
  // be stored or called.
  virtual void RegisterDBLoadedCallback(const DBLoadedCallback& callback);

  // Returns true if the database load has completetd successfully, and
  // ShutdownOnUIThread has not yet been called.
  virtual bool IsDatabaseLoaded();

  // Returns a pointer to the DB (used by SyncableServices). May return NULL if
  // the database is not loaded or otherwise unavailable. Must be called on
  // DBThread.
  virtual WebDatabase* GetDatabase();

 protected:
  friend class base::RefCountedDeleteOnSequence<WebDataServiceBase>;
  friend class base::DeleteHelper<WebDataServiceBase>;

  virtual ~WebDataServiceBase();

  // Our database service.
  scoped_refptr<WebDatabaseService> wdbs_;

 private:
  ProfileErrorCallback profile_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceBase);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_
