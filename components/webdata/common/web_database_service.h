// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/webdata_export.h"

class WebDataServiceBackend;
class WebDataRequestManager;

namespace content {
class BrowserContext;
}

namespace tracked_objects {
class Location;
}

class WDTypedResult;
class WebDataServiceConsumer;


////////////////////////////////////////////////////////////////////////////////
//
// WebDatabaseService defines the interface to a generic data repository
// responsible for controlling access to the web database (metadata associated
// with web pages).
//
////////////////////////////////////////////////////////////////////////////////

class WEBDATA_EXPORT WebDatabaseService
    : public base::RefCountedThreadSafe<
          WebDatabaseService,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<scoped_ptr<WDTypedResult>(WebDatabase*)> ReadTask;
  typedef base::Callback<WebDatabase::State(WebDatabase*)> WriteTask;

  // Takes the path to the WebDatabase file.
  explicit WebDatabaseService(const base::FilePath& path);

  // Adds |table| as a WebDatabaseTable that will participate in
  // managing the database, transferring ownership. All calls to this
  // method must be made before |LoadDatabase| is called.
  virtual void AddTable(scoped_ptr<WebDatabaseTable> table);

  // Initializes the web database service. Takes a callback which will return
  // the status of the DB after the init.
  virtual void LoadDatabase();

  // Unloads the database without actually shutting down the service.  This can
  // be used to temporarily reduce the browser process' memory footprint.
  virtual void UnloadDatabase();

  // Unloads database and will not reload.
  virtual void ShutdownDatabase();

  // Gets a ptr to the WebDatabase (owned by WebDatabaseService).
  // TODO(caitkp): remove this method once SyncServices no longer depend on it.
  virtual WebDatabase* GetDatabaseOnDB() const;

  // Schedule an update/write task on the DB thread.
  virtual void ScheduleDBTask(
      const tracked_objects::Location& from_here,
      const WriteTask& task);

  // Schedule a read task on the DB thread.
  virtual WebDataServiceBase::Handle ScheduleDBTaskWithResult(
      const tracked_objects::Location& from_here,
      const ReadTask& task,
      WebDataServiceConsumer* consumer);

  // Cancel an existing request for a task on the DB thread.
  // TODO(caitkp): Think about moving the definition of the Handle type to
  // somewhere else.
  virtual void CancelRequest(WebDataServiceBase::Handle h);

  void AddObserver(WebDatabaseObserver* observer);
  void RemoveObserver(WebDatabaseObserver* observer);

 private:
  class BackendDelegate;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WebDatabaseService>;
  // We have to friend RCTS<> so WIN shared-lib build is happy (crbug/112250).
  friend class base::RefCountedThreadSafe<WebDatabaseService,
      content::BrowserThread::DeleteOnUIThread>;
  friend class BackendDelegate;

  virtual ~WebDatabaseService();

  void OnDatabaseLoadDone(sql::InitStatus status);

  base::FilePath path_;

  // The primary owner is |WebDatabaseService| but is refcounted because
  // PostTask on DB thread may outlive us.
  scoped_refptr<WebDataServiceBackend> wds_backend_;

  ObserverList<WebDatabaseObserver> observer_list_;

  // All vended weak pointers are invalidated in ShutdownDatabase().
  base::WeakPtrFactory<WebDatabaseService> weak_ptr_factory_;
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_SERVICE_H_
