// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BACKEND_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BACKEND_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_export.h"

class WebDatabase;
class WebDatabaseTable;
class WebDataRequest;
class WebDataRequestManager;

namespace tracked_objects {
class Location;
}

// WebDataServiceBackend handles all database tasks posted by
// WebDatabaseService. It is refcounted to allow asynchronous destruction on the
// DB thread.

// TODO(caitkp): Rename this class to WebDatabaseBackend.

class WEBDATA_EXPORT WebDataServiceBackend
    : public base::RefCountedDeleteOnMessageLoop<WebDataServiceBackend> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded(sql::InitStatus status) = 0;
  };

  WebDataServiceBackend(const base::FilePath& path,
                        Delegate* delegate,
                        const scoped_refptr<base::MessageLoopProxy>& db_thread);

  // Must call only before InitDatabaseWithCallback.
  void AddTable(scoped_ptr<WebDatabaseTable> table);

  // Initializes the database and notifies caller via callback when complete.
  // Callback is called synchronously.
  void InitDatabase();

  // Opens the database file from the profile path if an init has not yet been
  // attempted. Separated from the constructor to ease construction/destruction
  // of this object on one thread but database access on the DB thread. Returns
  // the status of the database.
  sql::InitStatus LoadDatabaseIfNecessary();

  // Shuts down the database.
  void ShutdownDatabase();

  // Task wrappers to update requests and and notify |request_manager_|. These
  // are used in cases where the request is being made from the UI thread and an
  // asyncronous callback is required to notify the client of |request|'s
  // completion.
  void DBWriteTaskWrapper(
      const WebDatabaseService::WriteTask& task,
      scoped_ptr<WebDataRequest> request);
  void DBReadTaskWrapper(
      const WebDatabaseService::ReadTask& task,
      scoped_ptr<WebDataRequest> request);

  // Task runners to run database tasks.
  void ExecuteWriteTask(const WebDatabaseService::WriteTask& task);
  scoped_ptr<WDTypedResult> ExecuteReadTask(
      const WebDatabaseService::ReadTask& task);

  const scoped_refptr<WebDataRequestManager>& request_manager() {
    return request_manager_;
  }

  WebDatabase* database() { return db_.get(); }

 protected:
  friend class base::RefCountedDeleteOnMessageLoop<WebDataServiceBackend>;
  friend class base::DeleteHelper<WebDataServiceBackend>;

  virtual ~WebDataServiceBackend();

 private:
  // Commit the current transaction.
  void Commit();

  // Path to database file.
  base::FilePath db_path_;

  // The tables that participate in managing the database. These are
  // owned here but other than that this class does nothing with
  // them. Their initialization is in whatever factory creates
  // WebDatabaseService, and lookup by type is provided by the
  // WebDatabase class. The tables need to be owned by this refcounted
  // object, or they themselves would need to be refcounted. Owning
  // them here rather than having WebDatabase own them makes for
  // easier unit testing of WebDatabase.
  ScopedVector<WebDatabaseTable> tables_;

  scoped_ptr<WebDatabase> db_;

  // Keeps track of all pending requests made to the db.
  scoped_refptr<WebDataRequestManager> request_manager_;

  // State of database initialization. Used to prevent the executing of tasks
  // before the db is ready.
  sql::InitStatus init_status_;

  // True if an attempt has been made to load the database (even if the attempt
  // fails), used to avoid continually trying to reinit if the db init fails.
  bool init_complete_;

  // Delegate. See the class definition above for more information.
  scoped_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceBackend);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BACKEND_H_
