// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/api/webdata/web_data_service_base.h"
#include "chrome/browser/webdata/web_database.h"

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

class WebDatabaseService
    : public base::RefCountedThreadSafe<
          WebDatabaseService,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<scoped_ptr<WDTypedResult>(WebDatabase*)> ReadTask;
  typedef base::Callback<WebDatabase::State(WebDatabase*)> WriteTask;
  typedef base::Callback<void(sql::InitStatus)> InitCallback;

  // Takes the path to the WebDatabase file, and the locale (for migration
  // purposes).
  explicit WebDatabaseService(
      const base::FilePath& path, const std::string app_locale);

  // Adds |table| as a WebDatabaseTable that will participate in
  // managing the database, transferring ownership. All calls to this
  // method must be made before |LoadDatabase| is called.
  virtual void AddTable(scoped_ptr<WebDatabaseTable> table);

  // Initializes the web database service. Takes a callback which will return
  // the status of the DB after the init.
  virtual void LoadDatabase(const InitCallback& callback);

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

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WebDatabaseService>;

  virtual ~WebDatabaseService();

  base::FilePath path_;

  // The application locale.  The locale is needed for some database migrations,
  // and must be read on the UI thread.  It's cached here so that we can pass it
  // to the migration code on the DB thread.
  std::string app_locale_;

  // The primary owner is |WebDatabaseService| but is refcounted because
  // PostTask on DB thread may outlive us.
  scoped_refptr<WebDataServiceBackend> wds_backend_;
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_H_
